#include <grpcpp/grpcpp.h>
#include "pb/connect6.grpc.pb.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <atomic>

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReaderWriter;
using connect6::GameServer;
using connect6::GameState;
using connect6::PlayerAction;

namespace {

// Constantes del juego
constexpr int N = 19;         // Tamaño del tablero (19x19)
constexpr int EMPTY = 0;      // Celda vacía
constexpr int SELF  = 1;      // Piedra propia
constexpr int ENEMY = 2;      // Piedra enemiga

constexpr int INF_POS = 1000000000; // Valor positivo infinito (para búsqueda)
constexpr int INF_NEG = -1000000000;// Valor negativo infinito

// Estructura para una celda del tablero
struct Cell {
    int r = -1;
    int c = -1;
};

// Estructura para un movimiento (puede ser una o dos piedras)
struct TurnMove {
    Cell a;
    Cell b;
    bool oneStone = false;
};

// Definición del tablero como una matriz de N x N
using Grid = std::array<std::array<int8_t, N>, N>;

// Clase para controlar el tiempo disponible para pensar
class TimeBudget {
public:
    explicit TimeBudget(int milliseconds)
        : deadline_(std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds)) {}

    bool expired() const {
        return std::chrono::steady_clock::now() >= deadline_;
    }

private:
    std::chrono::steady_clock::time_point deadline_;
};

// Verifica si una celda está dentro del tablero
inline bool inside(int r, int c) {
    return r >= 0 && r < N && c >= 0 && c < N;
}

// Carga el tablero desde el estado recibido del servidor
void loadBoardFromState(const GameState& state, Grid& grid) {
    for (int r = 0; r < N; ++r) {
        const auto& row = state.board(r);
        for (int c = 0; c < N; ++c) {
            auto color = row.cells(c);
            if (color == connect6::UNKNOWN) {
                grid[r][c] = EMPTY;
            } else if (color == state.my_color()) {
                grid[r][c] = SELF;
            } else {
                grid[r][c] = ENEMY;
            }
        }
    }
}

// Verifica si hay alguna piedra en el tablero
bool boardHasAnyStone(const Grid& grid) {
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            if (grid[r][c] != EMPTY) return true;
        }
    }
    return false;
}

// Recolecta las celdas vacías cercanas a piedras existentes (para limitar opciones)
std::vector<Cell> collectNearbyOptions(const Grid& grid) {
    std::vector<Cell> result;
    bool seen[N][N] = {};

    // Si el tablero está vacío, elige el centro
    if (!boardHasAnyStone(grid)) {
        for (int r = 8; r <= 10; ++r) {
            for (int c = 8; c <= 10; ++c) {
                result.push_back({r, c});
            }
        }
        return result;
    }

    // Busca celdas vacías cerca de piedras ya puestas
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            if (grid[r][c] == EMPTY) continue;

            for (int dr = -2; dr <= 2; ++dr) {
                for (int dc = -2; dc <= 2; ++dc) {
                    int nr = r + dr;
                    int nc = c + dc;

                    if (!inside(nr, nc)) continue;
                    if (grid[nr][nc] != EMPTY) continue;
                    if (seen[nr][nc]) continue;

                    seen[nr][nc] = true;
                    result.push_back({nr, nc});
                }
            }
        }
    }

    return result;
}

// Genera todos los movimientos posibles (una o dos piedras)
std::vector<TurnMove> buildMoves(const std::vector<Cell>& options, int stonesToPlace) {
    std::vector<TurnMove> moves;

    if (stonesToPlace == 1) {
        moves.reserve(options.size());
        for (const auto& p : options) {
            moves.push_back({p, {-1, -1}, true});
        }
        return moves;
    }

    const size_t sz = options.size();
    if (sz < 2) return moves;

    moves.reserve((sz * (sz - 1)) / 2);
    for (size_t i = 0; i < sz; ++i) {
        for (size_t j = i + 1; j < sz; ++j) {
            moves.push_back({options[i], options[j], false});
        }
    }
    return moves;
}

// Coloca una piedra en el tablero
inline void putStone(Grid& grid, const Cell& p, int who) {
    grid[p.r][p.c] = static_cast<int8_t>(who);
}

// Quita una piedra del tablero
inline void removeStone(Grid& grid, const Cell& p) {
    grid[p.r][p.c] = EMPTY;
}

// Aplica un movimiento al tablero
void applyMove(Grid& grid, const TurnMove& mv, int who) {
    putStone(grid, mv.a, who);
    if (!mv.oneStone) {
        putStone(grid, mv.b, who);
    }
}

// Deshace un movimiento en el tablero
void undoMove(Grid& grid, const TurnMove& mv) {
    removeStone(grid, mv.a);
    if (!mv.oneStone) {
        removeStone(grid, mv.b);
    }
}

// Asigna un valor a una línea de 6 celdas según cuántas piedras hay
int lineValueByCount(int count, bool mine) {
    if (mine) {
        switch (count) {
            case 6: return 1000000;
            case 5: return 10000;
            case 4: return 10000;
            case 3: return 100;
            case 2: return 10;
            default: return 0;
        }
    } else {
        switch (count) {
            case 6: return -1000000;
            case 5: return -90000;
            case 4: return -90000;
            case 3: return -500;
            case 2: return -50;
            default: return 0;
        }
    }
}

// Evalúa una ventana de 6 celdas en una dirección
int scoreWindow6(const Grid& grid, int r, int c, int dr, int dc) {
    int mine = 0;
    int opp = 0;

    for (int k = 0; k < 6; ++k) {
        int nr = r + dr * k;
        int nc = c + dc * k;

        if (!inside(nr, nc)) return 0;

        if (grid[nr][nc] == SELF) ++mine;
        else if (grid[nr][nc] == ENEMY) ++opp;
    }

    if (mine > 0 && opp > 0) return 0; // Línea bloqueada
    if (mine > 0) return lineValueByCount(mine, true);
    if (opp > 0) return lineValueByCount(opp, false);
    return 0;
}

// Evalúa el tablero sumando el valor de todas las líneas posibles
int staticEval(const Grid& grid) {
    static const int dirs[4][2] = {
        {1, 0},   // vertical
        {0, 1},   // horizontal
        {1, 1},   // diagonal1 
        {1, -1}   // diagonal2 
    };

    int total = 0;

    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            for (const auto& d : dirs) {
                total += scoreWindow6(grid, r, c, d[0], d[1]);
            }
        }
    }

    return total;
}

// Algoritmo de búsqueda minimax con poda alfa-beta
int alphabeta(
    Grid& grid,
    int depth,
    int alpha,
    int beta,
    bool maximizing,
    int stonesToPlace,
    const TimeBudget& timer
) {
    if (timer.expired()) {
        // Si se acaba el tiempo, retorna un valor de emergencia
        return maximizing ? INF_NEG / 2 : INF_POS / 2;
    }

    if (depth == 0) {
        // Si se llega a la profundidad máxima, evalúa el tablero
        return staticEval(grid);
    }

    auto options = collectNearbyOptions(grid);
    auto moves = buildMoves(options, stonesToPlace);

    if (moves.empty()) {
        return staticEval(grid);
    }

    if (maximizing) {
        int best = INF_NEG;

        for (const auto& mv : moves) {
            applyMove(grid, mv, SELF);
            int value = alphabeta(grid, depth - 1, alpha, beta, false, 2, timer);
            undoMove(grid, mv);

            if (value > best) best = value;
            if (value > alpha) alpha = value;
            if (beta <= alpha) break; // Poda beta
            if (timer.expired()) break;
        }

        return best;
    }

    int best = INF_POS;

    for (const auto& mv : moves) {
        applyMove(grid, mv, ENEMY);
        int value = alphabeta(grid, depth - 1, alpha, beta, true, 2, timer);
        undoMove(grid, mv);

        if (value < best) best = value;
        if (value < beta) beta = value;
        if (beta <= alpha) break; // Poda alfa
        if (timer.expired()) break;
    }

    return best;
}

// Busca el mejor movimiento en una profundidad dada
TurnMove searchOneDepth(Grid& grid, int depth, int stonesToPlace, const TimeBudget& timer) {
    auto options = collectNearbyOptions(grid);
    auto moves = buildMoves(options, stonesToPlace);

    TurnMove bestMove{};
    int bestScore = INF_NEG;

    if (moves.empty()) {
        // Si no hay movimientos, elige el centro
        bestMove.oneStone = (stonesToPlace == 1);
        bestMove.a = {9, 9};
        bestMove.b = {9, 10};
        return bestMove;
    }

    for (const auto& mv : moves) {
        if (timer.expired()) break;

        applyMove(grid, mv, SELF);
        int value = alphabeta(grid, depth - 1, INF_NEG, INF_POS, false, 2, timer);
        undoMove(grid, mv);

        if (value > bestScore) {
            bestScore = value;
            bestMove = mv;
        }
    }

    return bestMove;
}

// Realiza búsqueda iterativa para elegir el mejor movimiento
TurnMove chooseMove(Grid grid, int stonesToPlace) {
    TimeBudget timer(8000); // 8 segundos de tiempo máximo

    TurnMove best{};
    bool hasCompletedDepth = false;

    // Búsqueda iterativa por profundidad
    for (int depth = 1; depth <= 100; ++depth) {
        TurnMove candidate = searchOneDepth(grid, depth, stonesToPlace, timer);

        if (timer.expired()) {
            std::cout << "Poco tiempo. Usando best play de la profundidad anterior.\n";
            break;
        }

        best = candidate;
        hasCompletedDepth = true;
        std::cout << "Listo profundidad" << depth << "papá.\n";
    }

    // Si no se pudo buscar, elige el primer movimiento posible
    if (!hasCompletedDepth) {
        auto options = collectNearbyOptions(grid);
        auto moves = buildMoves(options, stonesToPlace);
        if (!moves.empty()) return moves.front();

        if (stonesToPlace == 1) {
            return {{9, 9}, {-1, -1}, true};
        }
        return {{9, 9}, {9, 10}, false};
    }

    return best;
}

// Envía el movimiento elegido al servidor
void sendMove(
    const TurnMove& mv,
    int stonesToPlace,
    const std::shared_ptr<ClientReaderWriter<PlayerAction, GameState>>& stream
) {
    PlayerAction action;
    auto* move = action.mutable_move();

    auto* s1 = move->add_stones();
    s1->set_x(mv.a.r);
    s1->set_y(mv.a.c);

    if (stonesToPlace == 2 && !mv.oneStone) {
        auto* s2 = move->add_stones();
        s2->set_x(mv.b.r);
        s2->set_y(mv.b.c);
    }

    stream->Write(action);

    std::cout << "Movimiento enviado: (" << mv.a.r << "," << mv.a.c << ")";
    if (stonesToPlace == 2 && !mv.oneStone) {
        std::cout << " y (" << mv.b.r << "," << mv.b.c << ")";
    }
    std::cout << "\n";
}

// Lógica principal para jugar una partida contra el servidor
void runMatch(std::shared_ptr<Channel> channel, const std::string& teamName) {
    auto stub = GameServer::NewStub(channel);
    ClientContext context;

    std::shared_ptr<ClientReaderWriter<PlayerAction, GameState>> stream(stub->Play(&context));

    PlayerAction registration;
    registration.set_register_team(teamName);

    std::cout << "Registrando equipo: " << teamName << std::endl;
    stream->Write(registration);

    GameState state;
    while (stream->Read(&state)) {
        if (state.status() == connect6::GameState_Status_WAITING) {
            std::cout << "Esperando víctima..." << std::endl;
            continue;
        }

        if (state.status() == connect6::GameState_Status_FINISHED) {
            std::cout << "Partida finalizada. Ganador: " << state.winner() << std::endl;
            break;
        }

        if (state.status() != connect6::GameState_Status_PLAYING) {
            continue;
        }

        if (!state.is_my_turn()) {
            std::cout << "Zzz..." << std::endl;
            continue;
        }

        std::cout << "Juega la bestia. Piedras requeridas: "
                  << state.stones_required() << std::endl;

        Grid grid{};
        loadBoardFromState(state, grid);

        TurnMove best = chooseMove(grid, state.stones_required());
        sendMove(best, state.stones_required(), stream);
    }
}

} // namespace

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    std::cout.setf(std::ios::unitbuf);

    const char* envAddr = std::getenv("SERVER_ADDR");

    std::string server = envAddr ? envAddr : "servidor:50051";
    std::string team = "Bot_CPP_AngSei";

    // Ciclo principal: reconecta si se pierde la conexión
    while (true) {
        std::cout << "Conectando a " << server << " como " << team << "..." << std::endl;
        auto channel = grpc::CreateChannel(server, grpc::InsecureChannelCredentials());
        runMatch(channel, team);

        std::cout << "Reconectando en 3 segundos..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    return 0;
}