#include "agent.hpp"
#include <algorithm>
#include <iostream>

// Implementación del constructor y funciones básicas
Agent::DoubleMove Agent::getBestMove(Board& currentBoard, Player me, int stonesRequired) {
    DoubleMove bestMove;
    int bestEval = std::numeric_limits<int>::min();
    Player opponent = (me == Player::BLACK) ? Player::WHITE : Player::BLACK;

    // 1. Generar movimientos posibles (simplificado: todas las celdas vacías)
    // En una IA competitiva, aquí filtrarías solo celdas cerca de otras fichas
    // En src/agent.cpp, dentro de getBestMove
std::vector<BoardPoint> moves;
for (int x = 0; x < 19; ++x) {
    for (int y = 0; y < 19; ++y) {
        if (currentBoard.grid[x][y] == Player::NONE) {
            // Solo agregar si hay una ficha a distancia 2 o menos
            bool near = false;
            for (int dx = -2; dx <= 2 && !near; ++dx) {
                for (int dy = -2; dy <= 2 && !near; ++dy) {
                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < 19 && ny >= 0 && ny < 19 && 
                        currentBoard.grid[nx][ny] != Player::NONE) {
                        near = true;
                    }
                }
            }
            if (near || (x == 9 && y == 9)) moves.push_back({x, y});
        }
    }
}

    // 2. Lógica para 1 o 2 piedras según stonesRequired [cite: 74]
    if (stonesRequired == 1) {
        for (auto& p : moves) {
            currentBoard.placeStone(p.x, p.y, me);
            int eval = alphaBeta(currentBoard, 2, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), false, me);
            currentBoard.grid[p.x][p.y] = Player::NONE;
            if (eval > bestEval) {
                bestEval = eval;
                bestMove.p1 = p;
            }
        }
    } else {
        // Para 2 piedras, evaluamos combinaciones (esto es lo que consume tiempo)
        // Limitamos la búsqueda para no exceder los 10 segundos
        for (size_t i = 0; i < std::min(moves.size(), (size_t)20); ++i) { 
            for (size_t j = i + 1; j < std::min(moves.size(), (size_t)21); ++j) {
                currentBoard.placeStone(moves[i].x, moves[i].y, me);
                currentBoard.placeStone(moves[j].x, moves[j].y, me);
                
                int eval = alphaBeta(currentBoard, 1, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), false, me);
                
                currentBoard.grid[moves[i].x][moves[i].y] = Player::NONE;
                currentBoard.grid[moves[j].x][moves[j].y] = Player::NONE;

                if (eval > bestEval) {
                    bestEval = eval;
                    bestMove.p1 = moves[i];
                    bestMove.p2 = moves[j];
                }
            }
        }
    }
    return bestMove;
}

int Agent::alphaBeta(Board& board, int depth, int alpha, int beta, bool isMaximizing, Player me) {
    Player opponent = (me == Player::BLACK) ? Player::WHITE : Player::BLACK;

    if (board.checkWin(me)) return 1000000 + depth;
    if (board.checkWin(opponent)) return -1000000 - depth;
    if (depth == 0) return evaluate(board, me);

    if (isMaximizing) {
        int maxEval = std::numeric_limits<int>::min();
        for (int x = 0; x < 19; x++) {
            for (int y = 0; y < 19; y++) {
                if (board.isValidMove(x, y)) {
                    board.placeStone(x, y, me);
                    int eval = alphaBeta(board, depth - 1, alpha, beta, false, me);
                    board.grid[x][y] = Player::NONE;
                    maxEval = std::max(maxEval, eval);
                    alpha = std::max(alpha, eval);
                    if (beta <= alpha) break; 
                }
            }
        }
        return maxEval;
    } else {
        int minEval = std::numeric_limits<int>::max();
        for (int x = 0; x < 19; x++) {
            for (int y = 0; y < 19; y++) {
                if (board.isValidMove(x, y)) {
                    board.placeStone(x, y, opponent);
                    int eval = alphaBeta(board, depth - 1, alpha, beta, true, me);
                    board.grid[x][y] = Player::NONE;
                    minEval = std::min(minEval, eval);
                    beta = std::min(beta, eval);
                    if (beta <= alpha) break;
                }
            }
        }
        return minEval;
    }
}

int Agent::evaluate(const Board& board, Player me) {
    int score = 0;
    Player opponent = (me == Player::BLACK) ? Player::WHITE : Player::BLACK;

    // Un truco simple: dar puntos por tener fichas cerca del centro o de otras fichas
    for (int x = 0; x < 19; x++) {
        for (int y = 0; y < 19; y++) {
            if (board.grid[x][y] == me) {
                // Puntos por posición central (más posibilidades de conectar 6)
                score += (9 - std::abs(9 - x)) + (9 - std::abs(9 - y));
            } else if (board.grid[x][y] == opponent) {
                score -= (9 - std::abs(9 - x)) + (9 - std::abs(9 - y));
            }
        }
    }
    return score;
}