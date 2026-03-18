cmake_minimum_required(VERSION 3.10)
project(Connect6Agent)

set(CMAKE_CXX_STANDARD 17)

# Buscar paquetes necesarios
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# Configurar rutas para archivos generados
set(PROTO_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(PROTO_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${PROTO_OUT_DIR})

set(PROTO_FILE "${PROTO_SRC_DIR}/connect6.proto")
set(PROTO_GENERATED_HEADER "${PROTO_OUT_DIR}/connect6.pb.h")
set(PROTO_GENERATED_SRC "${PROTO_OUT_DIR}/connect6.pb.cc")
set(GRPC_GENERATED_HEADER "${PROTO_OUT_DIR}/connect6.grpc.pb.h")
set(GRPC_GENERATED_SRC "${PROTO_OUT_DIR}/connect6.grpc.pb.cc")

# Comando para compilar el .proto
add_custom_command(
    OUTPUT "${PROTO_GENERATED_SRC}" "${PROTO_GENERATED_HEADER}" "${GRPC_GENERATED_SRC}" "${GRPC_GENERATED_HEADER}"
    COMMAND protobuf::protoc
    ARGS --cpp_out "${PROTO_OUT_DIR}" --grpc_out "${PROTO_OUT_DIR}"
         --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
         -I "${PROTO_SRC_DIR}" "${PROTO_FILE}"
    DEPENDS "${PROTO_FILE}"
)

# Definir el ejecutable y sus fuentes
add_executable(mi_agente 
    src/main.cpp 
    src/board.cpp 
    src/agent.cpp
    ${PROTO_GENERATED_SRC} 
    ${GRPC_GENERATED_SRC}
)

# Incluir las carpetas de headers
target_include_directories(mi_agente PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include 
    ${PROTO_OUT_DIR}
)

# Vincular librerías
target_link_libraries(mi_agente 
    gRPC::grpc++ 
    protobuf::libprotobuf 
    Threads::Threads
)