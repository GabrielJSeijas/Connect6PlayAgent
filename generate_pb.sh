#!/bin/bash

docker run --rm -v "$PWD":/app -w /app debian:12 bash -c "
apt-get update &&
apt-get install -y protobuf-compiler protobuf-compiler-grpc &&

# limpiar carpeta pb
rm -rf agente_cpp/pb &&

# crear carpeta limpia
mkdir -p agente_cpp/pb &&

protoc -I=. \
  --cpp_out=agente_cpp/pb \
  --grpc_out=agente_cpp/pb \
  --plugin=protoc-gen-grpc=/usr/bin/grpc_cpp_plugin \
  connect6.proto
"