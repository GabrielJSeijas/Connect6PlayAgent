#ifndef BOARD_HPP
#define BOARD_HPP

#include <vector>
#include <cstdint>

// Usamos los mismos valores que el .proto para consistencia
enum class Player : int32_t {
    NONE = 0,
    BLACK = 1,
    WHITE = 2
};

struct BoardPoint {
    int x, y;
};

class Board {
public:
    static const int SIZE = 19;
    Player grid[SIZE][SIZE];

    Board();
    void placeStone(int x, int y, Player p);
    bool isValidMove(int x, int y) const;
    bool checkWin(Player p) const; // Verifica si hay 6 en línea
    void reset();
};

#endif