FROM debian:12

RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    libgrpc++-dev \
    libprotobuf-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY agente_cpp/agente-connecta6.cpp .
COPY agente_cpp/pb/ ./pb/

RUN g++ -std=c++17 agente-connecta6.cpp \
    pb/connect6.pb.cc \
    pb/connect6.grpc.pb.cc \
    -o agente-connecta6 \
    `pkg-config --cflags --libs protobuf grpc++`

CMD ["./agente-connecta6"]