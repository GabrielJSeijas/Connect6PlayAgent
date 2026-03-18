#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
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

class Connect6Client {
public:
    Connect6Client(std::shared_ptr<Channel> channel, std::string teamName)
        : stub_(GameServer::NewStub(channel)), teamName_(teamName) {}

    void Play() {
        ClientContext context;
        std::shared_ptr<ClientReaderWriter<PlayerAction, GameState>> stream(stub_->Play(&context));

        if (!stream) {
            std::cerr << "ERROR: no se pudo crear el stream gRPC." << std::endl;
            return;
        }

        PlayerAction registerAction;
        registerAction.set_register_team(teamName_);

        if (!stream->Write(registerAction)) {
            std::cerr << "ERROR: no se pudo registrar el equipo." << std::endl;
            return;
        }

        std::cout << "Registrado como: " << teamName_ << std::endl;

        GameState state;
        Board localBoard;
        Agent ai;

        while (stream->Read(&state)) {
            if (state.status() == GameState::WAITING) {
                std::cout << "Esperando oponente..." << std::endl;
                continue;
            }

            if (state.status() == GameState::FINISHED) {
                std::cout << "Juego terminado. Resultado: " << state.result() << std::endl;
                break;
            }

            if (!state.is_my_turn()) {
                continue;
            }

            std::cout << "Es mi turno. Piedras requeridas: " << state.stones_required() << std::endl;

            localBoard.reset();
            for (int i = 0; i < 19; ++i) {
                for (int j = 0; j < 19; ++j) {
                    auto cell = state.board(i).cells(j);
                    localBoard.grid[i][j] = static_cast<Player>(cell);
                }
            }

            Agent::DoubleMove decision =
                ai.getBestMove(localBoard,
                               static_cast<Player>(state.my_color()),
                               state.stones_required());

            bool p1_ok = decision.p1.x >= 0 && decision.p1.x < 19 &&
                         decision.p1.y >= 0 && decision.p1.y < 19;

            bool p2_ok = decision.p2.x >= 0 && decision.p2.x < 19 &&
                         decision.p2.y >= 0 && decision.p2.y < 19;

            if (!p1_ok) {
                std::cerr << "ERROR: primera piedra invalida: ("
                          << decision.p1.x << ", " << decision.p1.y << ")" << std::endl;
                continue;
            }

            if (state.stones_required() == 2) {
                if (!p2_ok) {
                    std::cerr << "ERROR: segunda piedra invalida: ("
                              << decision.p2.x << ", " << decision.p2.y << ")" << std::endl;
                    continue;
                }

                if (decision.p1.x == decision.p2.x && decision.p1.y == decision.p2.y) {
                    std::cerr << "ERROR: ambas piedras apuntan a la misma posicion." << std::endl;
                    continue;
                }
            }

            PlayerAction moveAction;
            connect6::Move* move = moveAction.mutable_move();

            auto* p1 = move->add_stones();
            p1->set_x(decision.p1.x);
            p1->set_y(decision.p1.y);

            if (state.stones_required() == 2) {
                auto* p2 = move->add_stones();
                p2->set_x(decision.p2.x);
                p2->set_y(decision.p2.y);
            }

            std::cout << "Enviando movimiento: (" << decision.p1.x << ", " << decision.p1.y << ")";
            if (state.stones_required() == 2) {
                std::cout << " y (" << decision.p2.x << ", " << decision.p2.y << ")";
            }
            std::cout << std::endl;

            if (!stream->Write(moveAction)) {
                std::cerr << "ERROR: no se pudo enviar el movimiento." << std::endl;
                break;
            }

            std::cout << "Movimiento enviado." << std::endl;
        }

        stream->WritesDone();
        Status status = stream->Finish();

        if (!status.ok()) {
            std::cerr << "gRPC termino con error: "
                      << status.error_code() << " - "
                      << status.error_message() << std::endl;
        } else {
            std::cout << "Conexion cerrada correctamente." << std::endl;
        }
    }

private:
    std::unique_ptr<GameServer::Stub> stub_;
    std::string teamName_;
};

int main(int argc, char** argv) {
    std::string server_addr = "localhost:50051";
    const char* env_addr = std::getenv("SERVER_ADDR");
    if (env_addr) server_addr = env_addr;

    std::string team_name = "Agent_Gabriel_USB";
    const char* env_team = std::getenv("TEAM_NAME");
    if (env_team) team_name = env_team;

    while (true) {
        std::cout << "Conectando a " << server_addr
                  << " como " << team_name << "..." << std::endl;

        Connect6Client client(
            grpc::CreateChannel(server_addr, grpc::InsecureChannelCredentials()),
            team_name
        );

        client.Play();

        std::cout << "Reconectando en 2 segundos..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    return 0;
}