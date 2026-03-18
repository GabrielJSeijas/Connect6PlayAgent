#include "agent.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <vector>

static bool samePoint(const BoardPoint& a, const BoardPoint& b) {
    return a.x == b.x && a.y == b.y;
}

static std::vector<BoardPoint> generateCandidateMoves(Board& currentBoard) {
    std::vector<BoardPoint> moves;

    for (int x = 0; x < 19; ++x) {
        for (int y = 0; y < 19; ++y) {
            if (currentBoard.grid[x][y] == Player::NONE) {
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

                if (near || (x == 9 && y == 9)) {
                    moves.push_back({x, y});
                }
            }
        }
    }

    return moves;
}

static std::vector<BoardPoint> findImmediateWinningCells(
    Board& board,
    const std::vector<BoardPoint>& moves,
    Player player
) {
    std::vector<BoardPoint> winningCells;

    for (const auto& p : moves) {
        if (!board.isValidMove(p.x, p.y)) continue;

        board.placeStone(p.x, p.y, player);
        bool win = board.checkWin(player);
        board.grid[p.x][p.y] = Player::NONE;

        if (win) {
            winningCells.push_back(p);
        }
    }

    return winningCells;
}

static std::vector<std::pair<BoardPoint, BoardPoint>> findImmediateWinningPairs(
    Board& board,
    const std::vector<BoardPoint>& moves,
    Player player,
    size_t limit = 14
) {
    std::vector<std::pair<BoardPoint, BoardPoint>> winningPairs;
    size_t maxMoves = std::min(moves.size(), limit);

    for (size_t i = 0; i < maxMoves; ++i) {
        if (!board.isValidMove(moves[i].x, moves[i].y)) continue;

        board.placeStone(moves[i].x, moves[i].y, player);

        for (size_t j = i + 1; j < maxMoves; ++j) {
            if (!board.isValidMove(moves[j].x, moves[j].y)) continue;

            board.placeStone(moves[j].x, moves[j].y, player);
            bool win = board.checkWin(player);
            board.grid[moves[j].x][moves[j].y] = Player::NONE;

            if (win) {
                winningPairs.push_back({moves[i], moves[j]});
            }
        }

        board.grid[moves[i].x][moves[i].y] = Player::NONE;
    }

    return winningPairs;
}

static int countBlockedPairsByOneStone(
    const BoardPoint& p,
    const std::vector<std::pair<BoardPoint, BoardPoint>>& pairs
) {
    int count = 0;
    for (const auto& pr : pairs) {
        if (samePoint(p, pr.first) || samePoint(p, pr.second)) {
            count++;
        }
    }
    return count;
}

static int countBlockedPairsByTwoStones(
    const BoardPoint& a,
    const BoardPoint& b,
    const std::vector<std::pair<BoardPoint, BoardPoint>>& pairs
) {
    int count = 0;
    for (const auto& pr : pairs) {
        if (samePoint(a, pr.first) || samePoint(a, pr.second) ||
            samePoint(b, pr.first) || samePoint(b, pr.second)) {
            count++;
        }
    }
    return count;
}

static bool isWinningMove(Board& board, BoardPoint p, Player player) {
    if (!board.isValidMove(p.x, p.y)) return false;

    board.placeStone(p.x, p.y, player);
    bool win = board.checkWin(player);
    board.grid[p.x][p.y] = Player::NONE;

    return win;
}

static bool findImmediateWinningMove(Board& board, const std::vector<BoardPoint>& moves, Player player, BoardPoint& winningMove) {
    for (const auto& p : moves) {
        if (isWinningMove(board, p, player)) {
            winningMove = p;
            return true;
        }
    }
    return false;
}

// Implementación del constructor y funciones básicas
Agent::DoubleMove Agent::getBestMove(Board& currentBoard, Player me, int stonesRequired) {
    DoubleMove bestMove;
    bestMove.p1 = {9, 9};
    bestMove.p2 = {9, 9};

    int bestEval = std::numeric_limits<int>::min();
    Player opponent = (me == Player::BLACK) ? Player::WHITE : Player::BLACK;

    std::vector<BoardPoint> moves = generateCandidateMoves(currentBoard);

    if (moves.empty()) {
        for (int x = 0; x < 19; ++x) {
            for (int y = 0; y < 19; ++y) {
                if (currentBoard.grid[x][y] == Player::NONE) {
                    bestMove.p1 = {x, y};
                    if (stonesRequired == 2) bestMove.p2 = {x, y};
                    return bestMove;
                }
            }
        }
    }

    // =====================================================
    // 1. SI YO PUEDO GANAR YA, PRIORIZAR ESO
    // =====================================================
    if (stonesRequired == 1) {
        std::vector<BoardPoint> myWinningCells =
            findImmediateWinningCells(currentBoard, moves, me);

        if (!myWinningCells.empty()) {
            bestMove.p1 = myWinningCells[0];
            return bestMove;
        }
    } else {
        auto myWinningPairs =
            findImmediateWinningPairs(currentBoard, moves, me);

        if (!myWinningPairs.empty()) {
            bestMove.p1 = myWinningPairs[0].first;
            bestMove.p2 = myWinningPairs[0].second;
            return bestMove;
        }
    }

    // =====================================================
    // 2. SI EL RIVAL GANA CON 1 PIEDRA, BLOQUEAR ESO
    // =====================================================
    std::vector<BoardPoint> opponentWinningCells =
        findImmediateWinningCells(currentBoard, moves, opponent);

    if (!opponentWinningCells.empty()) {
        if (stonesRequired == 1) {
            bestMove.p1 = opponentWinningCells[0];
            return bestMove;
        } else {
            bestMove.p1 = opponentWinningCells[0];

            if (opponentWinningCells.size() >= 2) {
                bestMove.p2 = opponentWinningCells[1];
                return bestMove;
            }

            // Solo había una amenaza de 1 piedra; la segunda piedra se usa para ayudar a bloquear pares
            auto opponentWinningPairs =
                findImmediateWinningPairs(currentBoard, moves, opponent);

            int bestBlockScore = -1;
            BoardPoint bestSecond = bestMove.p1;

            for (const auto& p : moves) {
                if (!currentBoard.isValidMove(p.x, p.y)) continue;
                if (samePoint(p, bestMove.p1)) continue;

                int score = countBlockedPairsByTwoStones(bestMove.p1, p, opponentWinningPairs);
                if (score > bestBlockScore) {
                    bestBlockScore = score;
                    bestSecond = p;
                }
            }

            bestMove.p2 = bestSecond;
            return bestMove;
        }
    }

    // =====================================================
    // 3. SI EL RIVAL GANA CON 2 PIEDRAS EN SU SIGUIENTE TURNO, BLOQUEAR PARES
    // =====================================================
    auto opponentWinningPairs =
        findImmediateWinningPairs(currentBoard, moves, opponent);

    if (!opponentWinningPairs.empty()) {
        if (stonesRequired == 1) {
            int bestBlockScore = -1;
            BoardPoint bestBlock = moves[0];

            for (const auto& p : moves) {
                if (!currentBoard.isValidMove(p.x, p.y)) continue;

                int score = countBlockedPairsByOneStone(p, opponentWinningPairs);
                if (score > bestBlockScore) {
                    bestBlockScore = score;
                    bestBlock = p;
                }
            }

            bestMove.p1 = bestBlock;
            return bestMove;
        } else {
            int bestBlockScore = -1;

            for (size_t i = 0; i < std::min(moves.size(), (size_t)20); ++i) {
                if (!currentBoard.isValidMove(moves[i].x, moves[i].y)) continue;

                for (size_t j = i + 1; j < std::min(moves.size(), (size_t)21); ++j) {
                    if (!currentBoard.isValidMove(moves[j].x, moves[j].y)) continue;

                    int score = countBlockedPairsByTwoStones(moves[i], moves[j], opponentWinningPairs);
                    if (score > bestBlockScore) {
                        bestBlockScore = score;
                        bestMove.p1 = moves[i];
                        bestMove.p2 = moves[j];
                    }
                }
            }

            if (bestBlockScore > 0) {
                return bestMove;
            }
        }
    }

    // =====================================================
    // 4. SI NO HAY URGENCIA TÁCTICA, USAR ALPHA-BETA
    // =====================================================
    if (stonesRequired == 1) {
        for (auto& p : moves) {
            if (!currentBoard.isValidMove(p.x, p.y)) continue;

            currentBoard.placeStone(p.x, p.y, me);

            int eval = alphaBeta(
                currentBoard,
                2,
                std::numeric_limits<int>::min(),
                std::numeric_limits<int>::max(),
                false,
                me
            );

            currentBoard.grid[p.x][p.y] = Player::NONE;

            if (eval > bestEval) {
                bestEval = eval;
                bestMove.p1 = p;
            }
        }
    } else {
        for (size_t i = 0; i < std::min(moves.size(), (size_t)20); ++i) {
            if (!currentBoard.isValidMove(moves[i].x, moves[i].y)) continue;

            for (size_t j = i + 1; j < std::min(moves.size(), (size_t)21); ++j) {
                if (!currentBoard.isValidMove(moves[j].x, moves[j].y)) continue;

                currentBoard.placeStone(moves[i].x, moves[i].y, me);
                currentBoard.placeStone(moves[j].x, moves[j].y, me);

                int eval = alphaBeta(
                    currentBoard,
                    1,
                    std::numeric_limits<int>::min(),
                    std::numeric_limits<int>::max(),
                    false,
                    me
                );

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

    // =====================================================
    // 5. VALIDACIÓN FINAL
    // =====================================================
    if (!currentBoard.isValidMove(bestMove.p1.x, bestMove.p1.y)) {
        for (const auto& p : moves) {
            if (currentBoard.isValidMove(p.x, p.y)) {
                bestMove.p1 = p;
                break;
            }
        }
    }

    if (stonesRequired == 2) {
        if (!currentBoard.isValidMove(bestMove.p2.x, bestMove.p2.y) ||
            samePoint(bestMove.p1, bestMove.p2)) {
            for (const auto& p : moves) {
                if (currentBoard.isValidMove(p.x, p.y) &&
                    !samePoint(p, bestMove.p1)) {
                    bestMove.p2 = p;
                    break;
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