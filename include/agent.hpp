#ifndef AGENT_HPP
#define AGENT_HPP

#include "board.hpp"
#include <vector>
#include <limits>

class Agent {
public:
    // Estructura para representar una jugada de 2 piedras
    struct DoubleMove {
        BoardPoint p1;
        BoardPoint p2;
        int score;
    };

    // Función principal que llamará main.cpp
    DoubleMove getBestMove(Board& currentBoard, Player me, int stonesRequired);

private:
    // El corazón del proyecto: Poda Alfa-Beta 
    int alphaBeta(Board& board, int depth, int alpha, int beta, bool isMaximizing, Player me);
    
    // Función heurística: evalúa qué tan bueno es el tablero [cite: 24, 53]
    int evaluate(const Board& board, Player me);
};

#endif