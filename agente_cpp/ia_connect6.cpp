#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cmath>

#include <grpcpp/grpcpp.h>
#include "pb/connect6.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReaderWriter;
using grpc::Status;

using connect6::GameServer;
using connect6::PlayerAction;
using connect6::GameState;
using connect6::Point;

using namespace std;

typedef int8_t Board[19][19];

struct Pos {
    int x, y;
};

struct MovePair {
    Pos p1{-1, -1};
    Pos p2{-1, -1};
    bool single_stone = true;
};

static atomic<bool> timeout_flag(false);

// =========================
// Utilidades de tablero
// =========================

bool in_bounds(int x, int y) {
    return x >= 0 && x < 19 && y >= 0 && y < 19;
}

bool isCellEmpty(Board board, int x, int y) {
    return in_bounds(x, y) && board[x][y] == 0;
}

void place_stone(Board board, int x, int y, int who) {
    board[x][y] = static_cast<int8_t>(who);
}

void remove_stone(Board board, int x, int y) {
    board[x][y] = 0;
}

bool same_pos(const Pos& a, const Pos& b) {
    return a.x == b.x && a.y == b.y;
}

void sync_board(const connect6::GameState& state, Board local_board) {
    for (int r = 0; r < 19; ++r) {
        const auto& row = state.board(r);
        for (int c = 0; c < 19; ++c) {
            connect6::PlayerColor color = row.cells(c);
            if (color == state.my_color()) {
                local_board[r][c] = 1; // mío
            } else if (color == connect6::UNKNOWN) {
                local_board[r][c] = 0; // vacío
            } else {
                local_board[r][c] = 2; // oponente
            }
        }
    }
}

bool board_isCellEmpty(Board board) {
    for (int x = 0; x < 19; ++x) {
        for (int y = 0; y < 19; ++y) {
            if (board[x][y] != 0) return false;
        }
    }
    return true;
}

int count_dir(Board board, int x, int y, int dx, int dy, int who) {
    int cnt = 0;
    int nx = x + dx;
    int ny = y + dy;
    while (in_bounds(nx, ny) && board[nx][ny] == who) {
        cnt++;
        nx += dx;
        ny += dy;
    }
    return cnt;
}

bool check_win_from(Board board, int x, int y, int who) {
    if (!in_bounds(x, y) || board[x][y] != who) return false;

    const int dirs[4][2] = {
        {1, 0}, {0, 1}, {1, 1}, {1, -1}
    };

    for (auto& d : dirs) {
        int total = 1;
        total += count_dir(board, x, y, d[0], d[1], who);
        total += count_dir(board, x, y, -d[0], -d[1], who);
        if (total >= 6) return true;
    }
    return false;
}

bool has_win(Board board, int who) {
    for (int x = 0; x < 19; ++x) {
        for (int y = 0; y < 19; ++y) {
            if (board[x][y] == who && check_win_from(board, x, y, who)) {
                return true;
            }
        }
    }
    return false;
}

// =========================
// Candidatos
// =========================

vector<Pos> get_candidates(Board board) {
    vector<Pos> candidates;
    bool visited[19][19] = {false};

    if (board_isCellEmpty(board)) {
        for (int x = 8; x <= 10; ++x) {
            for (int y = 8; y <= 10; ++y) {
                candidates.push_back({x, y});
            }
        }
        return candidates;
    }

    for (int x = 0; x < 19; ++x) {
        for (int y = 0; y < 19; ++y) {
            if (board[x][y] != 0) {
                for (int dx = -2; dx <= 2; ++dx) {
                    for (int dy = -2; dy <= 2; ++dy) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (in_bounds(nx, ny) && board[nx][ny] == 0 && !visited[nx][ny]) {
                            visited[nx][ny] = true;
                            candidates.push_back({nx, ny});
                        }
                    }
                }
            }
        }
    }

    if (candidates.empty()) {
        for (int x = 0; x < 19; ++x) {
            for (int y = 0; y < 19; ++y) {
                if (board[x][y] == 0) {
                    candidates.push_back({x, y});
                }
            }
        }
    }

    return candidates;
}

vector<MovePair> generate_move_combinations(const vector<Pos>& candidates, int stones_required, size_t limit = 16) {
    vector<MovePair> combinations;
    size_t n = min(candidates.size(), limit);

    if (stones_required == 1) {
        for (size_t i = 0; i < n; ++i) {
            combinations.push_back({candidates[i], {-1, -1}, true});
        }
    } else {
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                combinations.push_back({candidates[i], candidates[j], false});
            }
        }
    }

    return combinations;
}

// =========================
// Táctica inmediata
// =========================

vector<Pos> find_immediate_winning_cells(Board board, const vector<Pos>& candidates, int who) {
    vector<Pos> wins;

    for (const auto& p : candidates) {
        if (!isCellEmpty(board, p.x, p.y)) continue;

        place_stone(board, p.x, p.y, who);
        bool win = check_win_from(board, p.x, p.y, who) || has_win(board, who);
        remove_stone(board, p.x, p.y);

        if (win) wins.push_back(p);
    }

    return wins;
}

vector<pair<Pos, Pos>> find_immediate_winning_pairs(Board board, const vector<Pos>& candidates, int who, size_t limit = 14) {
    vector<pair<Pos, Pos>> wins;
    size_t n = min(candidates.size(), limit);

    for (size_t i = 0; i < n; ++i) {
        if (!isCellEmpty(board, candidates[i].x, candidates[i].y)) continue;

        place_stone(board, candidates[i].x, candidates[i].y, who);

        for (size_t j = i + 1; j < n; ++j) {
            if (!isCellEmpty(board, candidates[j].x, candidates[j].y)) continue;

            place_stone(board, candidates[j].x, candidates[j].y, who);
            bool win = has_win(board, who);
            remove_stone(board, candidates[j].x, candidates[j].y);

            if (win) {
                wins.push_back({candidates[i], candidates[j]});
            }
        }

        remove_stone(board, candidates[i].x, candidates[i].y);
    }

    return wins;
}

int count_blocked_pairs_by_one_stone(const Pos& p, const vector<pair<Pos, Pos>>& pairs) {
    int count = 0;
    for (const auto& pr : pairs) {
        if (same_pos(p, pr.first) || same_pos(p, pr.second)) {
            count++;
        }
    }
    return count;
}

int count_blocked_pairs_by_two_stones(const Pos& a, const Pos& b, const vector<pair<Pos, Pos>>& pairs) {
    int count = 0;
    for (const auto& pr : pairs) {
        if (same_pos(a, pr.first) || same_pos(a, pr.second) ||
            same_pos(b, pr.first) || same_pos(b, pr.second)) {
            count++;
        }
    }
    return count;
}

// =========================
// Evaluación
// =========================
int score_by_count(int count, bool is_mine) {
    if (is_mine) {
        switch (count) {
            case 6: return 10000000; // Ganamos
            case 5: return 100000;
            case 4: return 10000;
            case 3: return 1000;
            default: return 0;
        }
    } else {
        switch (count) {
            case 5: return -8000000; // Bloqueo de 5 es vida o muerte
            case 4: return -4000000; // BLOQUEO DE 4: ¡ESTE ES EL CAMBIO CLAVE!
            case 3: return -50000;   
            case 2: return -5000;
            default: return 0;
        }
    }
}
int evaluate_window(Board board, int x, int y, int dx, int dy) {
    int mine = 0;
    int opp = 0;
    int consecutive_opp = 0;
    int max_consecutive_opp = 0;

    for (int i = 0; i < 6; ++i) {
        int nx = x + i * dx;
        int ny = y + i * dy;
        if (!in_bounds(nx, ny)) return 0;

        if (board[nx][ny] == 1) {
            mine++;
            consecutive_opp = 0;
        } else if (board[nx][ny] == 2) {
            opp++;
            consecutive_opp++;
            max_consecutive_opp = max(max_consecutive_opp, consecutive_opp);
        } else {
            consecutive_opp = 0;
        }
    }

    if (mine > 0 && opp > 0) return 0;
    
    // Si el oponente tiene fichas seguidas, penalizamos MUCHO más
    if (opp > 0) {
       return score_by_count(max_consecutive_opp, false);
    }
    
    if (mine > 0) {
        // Opcional: podrías hacer lo mismo para mios si quieres ser más agresivo atacando
        return score_by_count(mine, true);
    }
    return 0;
}

int evaluate_fitness(Board board) {
    if (has_win(board, 1)) return 1000000;
    if (has_win(board, 2)) return -1000000;

    int total = 0;
    const int dirs[4][2] = {
        {1, 0}, {0, 1}, {1, 1}, {1, -1}
    };

    for (auto& d : dirs) {
        for (int x = 0; x < 19; ++x) {
            for (int y = 0; y < 19; ++y) {
                total += evaluate_window(board, x, y, d[0], d[1]);
            }
        }
    }

    for (int x = 0; x < 19; ++x) {
        for (int y = 0; y < 19; ++y) {
            if (board[x][y] == 1) {
                total += (9 - abs(9 - x)) + (9 - abs(9 - y));
            } else if (board[x][y] == 2) {
                total -= (9 - abs(9 - x)) + (9 - abs(9 - y));
            }
        }
    }

    return total;
}

// =========================
// Alpha-beta
// =========================

int alphabeta(Board board,
              int depth,
              int alpha,
              int beta,
              bool maximizing,
              int stones_required,
              chrono::steady_clock::time_point deadline) {
    if (chrono::steady_clock::now() >= deadline || timeout_flag.load()) {
        timeout_flag.store(true);
        return evaluate_fitness(board);
    }

    if (has_win(board, 1)) return 1000000 + depth;
    if (has_win(board, 2)) return -1000000 - depth;
    if (depth == 0) return evaluate_fitness(board);

    auto candidates = get_candidates(board);
    auto moves = generate_move_combinations(candidates, stones_required, 12);

    if (moves.empty()) return evaluate_fitness(board);

    if (maximizing) {
        int best = -1000000000;

        for (const auto& m : moves) {
            if (!isCellEmpty(board, m.p1.x, m.p1.y)) continue;
            if (!m.single_stone && !isCellEmpty(board, m.p2.x, m.p2.y)) continue;
            if (!m.single_stone && same_pos(m.p1, m.p2)) continue;

            place_stone(board, m.p1.x, m.p1.y, 1);
            if (!m.single_stone) place_stone(board, m.p2.x, m.p2.y, 1);

            int val = alphabeta(board, depth - 1, alpha, beta, false, 2, deadline);

            remove_stone(board, m.p1.x, m.p1.y);
            if (!m.single_stone) remove_stone(board, m.p2.x, m.p2.y);

            best = max(best, val);
            alpha = max(alpha, val);

            if (beta <= alpha || timeout_flag.load()) break;
        }

        return best;
    } else {
        int best = 1000000000;

        for (const auto& m : moves) {
            if (!isCellEmpty(board, m.p1.x, m.p1.y)) continue;
            if (!m.single_stone && !isCellEmpty(board, m.p2.x, m.p2.y)) continue;
            if (!m.single_stone && same_pos(m.p1, m.p2)) continue;

            place_stone(board, m.p1.x, m.p1.y, 2);
            if (!m.single_stone) place_stone(board, m.p2.x, m.p2.y, 2);

            int val = alphabeta(board, depth - 1, alpha, beta, true, 2, deadline);

            remove_stone(board, m.p1.x, m.p1.y);
            if (!m.single_stone) remove_stone(board, m.p2.x, m.p2.y);

            best = min(best, val);
            beta = min(beta, val);

            if (beta <= alpha || timeout_flag.load()) break;
        }

        return best;
    }
}

// =========================
// Resolver jugada
// =========================

vector<Pos> find_threats(Board board, int who, int stones_needed_to_win) {
    vector<Pos> threats;
    auto candidates = get_candidates(board);
    for (const auto& p : candidates) {
        if (!isCellEmpty(board, p.x, p.y)) continue;
        
        place_stone(board, p.x, p.y, who);
        // Si al poner esta ficha, el oponente queda a 1 de ganar (tiene 5), es una amenaza
        bool is_threat = false;
        
        const int dirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
        for (auto& d : dirs) {
            int total = 1 + count_dir(board, p.x, p.y, d[0], d[1], who) 
                          + count_dir(board, p.x, p.y, -d[0], -d[1], who);
            if (total >= (6 - stones_needed_to_win + 1)) { 
                is_threat = true; 
                break; 
            }
        }
        
        remove_stone(board, p.x, p.y);
        if (is_threat) threats.push_back(p);
    }
    return threats;
}

MovePair choose_move(Board board, int stones_required) {
    MovePair best;
    best.single_stone = (stones_required == 1);

    // 1. Iniciar semilla para que no sea repetitivo
    static bool seeded = false;
    if (!seeded) { srand(time(NULL)); seeded = true; }

    auto candidates = get_candidates(board);

    if (candidates.empty()) {
        return {{9, 9}, {10, 9}, stones_required == 1};
    }
    // BLOQUEO DINÁMICO
    // Primero: ¿El rival tiene 5? (Gana con 1 ficha)
    auto emergency = find_threats(board, 2, 1); 
    if (!emergency.empty()) {
        if (stones_required == 1) return {emergency[0], {-1, -1}, true};
        Pos p2 = (emergency.size() > 1) ? emergency[1] : candidates[0];
        return {emergency[0], p2, false};
    }

    // Segundo: ¿El rival tiene 4? (Gana con sus 2 fichas de su turno)
    auto threats_4 = find_threats(board, 2, 2);
    if (!threats_4.empty()) {
        // Si detectamos 4 del rival, usamos nuestras fichas para estorbar ahí mismo
        if (stones_required == 1) return {threats_4[0], {-1, -1}, true};
        Pos p1 = threats_4[0];
        Pos p2 = (threats_4.size() > 1) ? threats_4[1] : candidates[0];
        return {p1, p2, false};
    }

    // 2. TÁCTICA INMEDIATA: Si gano en este turno, gano.
    if (stones_required == 1) {
        auto my_wins = find_immediate_winning_cells(board, candidates, 1);
        if (!my_wins.empty()) return {my_wins[0], {-1, -1}, true};
    } else {
        auto my_pairs = find_immediate_winning_pairs(board, candidates, 1, 14);
        if (!my_pairs.empty()) return {my_pairs[0].first, my_pairs[0].second, false};
    }

    // 3. BLOQUEO CRÍTICO: Si el rival gana con 1 o 2 fichas, BLOQUEAR YA.
    // Esto es lo que evita que te deje ganar en horizontales o diagonales.
    auto opp_wins = find_immediate_winning_cells(board, candidates, 2);
    if (!opp_wins.empty()) {
        if (stones_required == 1) {
            return {opp_wins[0], {-1, -1}, true};
        } else {
            // Si requiere 2 piedras, ponemos una donde él ganaría y la otra para estorbar
            Pos p1 = opp_wins[0];
            Pos p2 = (opp_wins.size() > 1) ? opp_wins[1] : candidates[0];
            if (same_pos(p1, p2)) p2 = candidates[1];
            return {p1, p2, false};
        }
    }

    // 4. ALPHA-BETA CON ALEATORIEDAD (Para no ser repetitivo)
    auto deadline = chrono::steady_clock::now() + chrono::milliseconds(7000);
    int best_eval = -2000000000;
    vector<MovePair> equal_best_moves; // Lista para guardar jugadas igual de buenas

    auto moves = generate_move_combinations(candidates, stones_required, 12);

    for (const auto& m : moves) {
        if (chrono::steady_clock::now() >= deadline) break;
        if (!isCellEmpty(board, m.p1.x, m.p1.y)) continue;

        place_stone(board, m.p1.x, m.p1.y, 1);
        if (!m.single_stone) place_stone(board, m.p2.x, m.p2.y, 1);

        // Llamamos a Alpha-Beta para ver qué tan buena es esta rama
        int eval = alphabeta(board, 1, -1000000000, 1000000000, false, 2, deadline);

        remove_stone(board, m.p1.x, m.p1.y);
        if (!m.single_stone) remove_stone(board, m.p2.x, m.p2.y);

        if (eval > best_eval) {
            best_eval = eval;
            equal_best_moves.clear();
            equal_best_moves.push_back(m);
        } else if (eval == best_eval) {
            equal_best_moves.push_back(m);
        }
    }

    // 5. ELECCIÓN FINAL: Elige una de las mejores opciones al azar
    if (!equal_best_moves.empty()) {
        best = equal_best_moves[rand() % equal_best_moves.size()];
    } else {
        best.p1 = candidates[0];
        if (!best.single_stone) best.p2 = candidates[1];
    }

    return best;
}

// =========================
// gRPC
// =========================

void playGame(shared_ptr<Channel> channel, string teamName) {
    auto stub = GameServer::NewStub(channel);
    ClientContext context;

    shared_ptr<ClientReaderWriter<PlayerAction, GameState>> stream(stub->Play(&context));

    if (!stream) {
        cerr << "ERROR: no se pudo abrir el stream." << endl;
        return;
    }

    cout << "Registrando equipo: " << teamName << endl;
    PlayerAction register_action;
    register_action.set_register_team(teamName);

    if (!stream->Write(register_action)) {
        cerr << "ERROR: no se pudo registrar el equipo." << endl;
        return;
    }

    GameState state;
    while (stream->Read(&state)) {
        if (state.status() == connect6::GameState_Status_WAITING) {
            cout << "Esperando contrincante..." << endl;
        }
        else if (state.status() == connect6::GameState_Status_PLAYING) {
            if (state.is_my_turn()) {
                cout << "Es mi turno. Piedras requeridas: " << state.stones_required() << endl;

                Board current_board;
                sync_board(state, current_board);

                timeout_flag.store(false);
                MovePair best_action = choose_move(current_board, state.stones_required());

                if (!in_bounds(best_action.p1.x, best_action.p1.y) ||
                    !isCellEmpty(current_board, best_action.p1.x, best_action.p1.y)) {
                    cerr << "ERROR: primera piedra invalida." << endl;
                    continue;
                }

                if (state.stones_required() == 2) {
                    if (!in_bounds(best_action.p2.x, best_action.p2.y) ||
                        !isCellEmpty(current_board, best_action.p2.x, best_action.p2.y) ||
                        same_pos(best_action.p1, best_action.p2)) {
                        cerr << "ERROR: segunda piedra invalida." << endl;
                        continue;
                    }
                }

                PlayerAction move_action;
                auto* move = move_action.mutable_move();

                Point* stone1 = move->add_stones();
                stone1->set_x(best_action.p1.x);
                stone1->set_y(best_action.p1.y);

                if (state.stones_required() == 2) {
                    Point* stone2 = move->add_stones();
                    stone2->set_x(best_action.p2.x);
                    stone2->set_y(best_action.p2.y);
                }

                cout << "Enviando movimiento: (" << best_action.p1.x << ", " << best_action.p1.y << ")";
                if (state.stones_required() == 2) {
                    cout << " y (" << best_action.p2.x << ", " << best_action.p2.y << ")";
                }
                cout << endl;

                if (!stream->Write(move_action)) {
                    cerr << "ERROR: no se pudo enviar el movimiento." << endl;
                    break;
                }

                cout << "Movimiento enviado." << endl;
            } else {
                cout << "Esperando al oponente..." << endl;
            }
        }
        else if (state.status() == connect6::GameState_Status_FINISHED) {
            cout << "Partida finalizada. Ganador: " << state.winner() << endl;
            break;
        }
    }

    stream->WritesDone();
    Status status = stream->Finish();
    if (!status.ok()) {
        cerr << "gRPC termino con error: " << status.error_code()
             << " - " << status.error_message() << endl;
    } else {
        cout << "Conexion cerrada correctamente." << endl;
    }
}

int main() {
    char* addr_env = std::getenv("SERVER_ADDR");
    string target_str = (addr_env) ? addr_env : "servidor:50051";

    char* team_env = std::getenv("TEAM_NAME");
    string team_name = (team_env) ? team_env : "Gabriel_USB_Bot";

    while (true) {
        cout << "Conectando a " << target_str << " como " << team_name << "..." << endl;

        auto channel = grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials());
        playGame(channel, team_name);

        cout << "Reconectando en 3 segundos..." << endl;
        this_thread::sleep_for(chrono::seconds(3));
    }

    return 0;
}