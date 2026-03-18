FROM ubuntu:24.04

# Evitar prompts interactivos durante la instalación
ENV DEBIAN_FRONTEND=noninteractive

# Instalar dependencias de compilación y gRPC
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler-grpc \
    pkg-config

WORKDIR /app
COPY . .

# Compilar el proyecto
RUN mkdir -p build && cd build && \
    cmake .. && \
    make

# Ejecutar el binario generado
CMD ["./build/mi_agente"]