FROM debian:12

RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler \
    protobuf-compiler-grpc \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copiar código fuente del agente
COPY agente_cpp/ia_connect6.cpp .
COPY connect6.proto .

# Generar protobuf y gRPC
RUN protoc -I=. \
    --cpp_out=pb \
    --grpc_out=pb \
    --plugin=protoc-gen-grpc=/usr/bin/grpc_cpp_plugin \
    connect6.proto

# Compilar agente
RUN g++ -std=c++17 ia_connect6.cpp \
    pb/connect6.pb.cc \
    pb/connect6.grpc.pb.cc \
    -o ia_connect6 \
    `pkg-config --cflags --libs protobuf grpc++`

CMD ["./ia_connect6"]