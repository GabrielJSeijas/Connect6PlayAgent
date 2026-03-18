#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>

#include "connect6.grpc.pb.h"
#include "board.hpp"
#include "agent.hpp"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReaderWriter;
using grpc::Status;
using connect6::GameServer;
using connect6::PlayerAction;
using connect6::GameState;
using connect6::Move;

class Connect6Client {
public:
    Connect6Client(std::shared_ptr<Channel> channel, std::string teamName)
        : stub_(GameServer::NewStub(channel)), teamName_(teamName) {}

    void Play() {
        ClientContext context;
        std::shared_ptr<ClientReaderWriter<PlayerAction, GameState>> stream(stub_->Play(&context));

        // 1. Registrar el equipo
        PlayerAction registerAction;
        registerAction.set_register_team(teamName);
        stream->Write(registerAction);
        std::cout << "Registrado como: " << teamName_ << std::endl;

        GameState state;
        Board localBoard;
        Agent ai;

        // 2. Bucle principal de juego
        while (stream->Read(&state)) {
            if (state.status() == GameState::WAITING) {
                std::cout << "Esperando oponente..." << std::endl;
                continue;
            }

            if (state.status() == GameState::FINISHED) {
                std::cout << "Juego terminado. Resultado: " << state.result() << std::endl;
                break;
            }

            if (state.is_my_turn()) {
                std::cout << "Es mi turno. Piedras requeridas: " << state.stones_required() << std::endl;

                // Actualizar tablero local con el estado del servidor
                localBoard.reset();
                for (int i = 0; i < 19; ++i) {
                    for (int j = 0; j < 19; ++j) {
                        auto cell = state.board(i).cells(j);
                        localBoard.grid[i][j] = static_cast<Player>(cell);
                    }
                }

                // Pedir jugada a la IA
                Agent::DoubleMove decision = ai.getBestMove(localBoard, static_cast<Player>(state.my_color()), state.stones_required());

                // Preparar respuesta
                PlayerAction moveAction;
                connect6::Move* move = moveAction.mutable_move();
                
                // Primera piedra
                auto* p1 = move->add_stones();
                p1->set_x(decision.p1.x);
                p1->set_y(decision.p1.y);

                // Segunda piedra (si es requerida)
                if (state.stones_required() == 2) {
                    auto* p2 = move->add_stones();
                    p2->set_x(decision.p2.x);
                    p2->set_y(decision.p2.y);
                }

                stream->Write(moveAction);
                std::cout << "Movimiento enviado." << std::endl;
            }
        }
        stream->WritesDone();
        Status status = stream->Finish();
    }

private:
    std::unique_ptr<GameServer::Stub> stub_;
    std::string teamName_;
};

int main(int argc, char** argv) {
    std::string server_addr = "localhost:8080";
    // Si corres en Docker, usa la variable de entorno
    const char* env_addr = std::getenv("SERVER_ADDR");
    if (env_addr) server_addr = env_addr;

    std::string team_name = "Agent_Gabriel_USB";

    Connect6Client client(grpc::CreateChannel(server_addr, grpc::InsecureChannelCredentials()), team_name);
    client.Play();

    return 0;
}