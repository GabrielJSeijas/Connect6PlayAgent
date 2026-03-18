#include "board.hpp"
#include <cstring>

Board::Board() {
    reset();
}

void Board::reset() {
    for (int i = 0; i < SIZE; ++i) {
        for (int j = 0; j < SIZE; ++j) {
            grid[i][j] = Player::NONE;
        }
    }
}

void Board::placeStone(int x, int y, Player p) {
    if (isValidMove(x, y)) {
        grid[x][y] = p;
    }
}

bool Board::isValidMove(int x, int y) const {
    return x >= 0 && x < SIZE && y >= 0 && y < SIZE && grid[x][y] == Player::NONE;
}

bool Board::checkWin(Player p) const {
    // Direcciones: Derecha, Abajo, Diagonal abajo-derecha, Diagonal abajo-izquierda
    int dx[] = {1, 0, 1, 1};
    int dy[] = {0, 1, 1, -1};

    for (int x = 0; x < SIZE; ++x) {
        for (int y = 0; y < SIZE; ++y) {
            if (grid[x][y] != p) continue;

            for (int dir = 0; dir < 4; ++dir) {
                int count = 1;
                for (int step = 1; step < 6; ++step) {
                    int nx = x + dx[dir] * step;
                    int ny = y + dy[dir] * step;

                    if (nx >= 0 && nx < SIZE && ny >= 0 && ny < SIZE && grid[nx][ny] == p) {
                        count++;
                    } else {
                        break;
                    }
                }
                if (count >= 6) return true; // Gana con 6 o más [cite: 16]
            }
        }
    }
    return false;
}