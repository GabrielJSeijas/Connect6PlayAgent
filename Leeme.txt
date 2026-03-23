# Descripción:
Este proyecto consiste en el desarrollo de un agente inteligente para jugar Connect6, implementado en C++ y conectado a un servidor mediante gRPC. El agente es capaz de recibir el estado del juego, procesarlo y generar jugadas de manera autónoma utilizando técnicas de búsqueda en árboles y heurísticas.

# Requisitos:
Antes de ejecutar el proyecto, debe de tener instalado:

Docker
Docker Compose

# Instrucciones de ejecución:

- Ubicarse en la carpeta raíz del proyecto:
`cd Connect6PlayAgent`

- Construir los contenedores:
`docker compose build`

- Ejecutar el sistema:
`docker compose up`

Esto levantará:

- El servidor de Connect6
- El agente IA (ia-angsei)


# Funcionamiento del agente

El agente implementa:

- Algoritmo Minimax con poda Alpha-Beta
- Profundización iterativa para manejo de tiempo
- Generación de jugadas basada en proximidad
- Evaluación heurística por ventanas de 6 posiciones

La estrategia del agente es principalmente agresiva, priorizando la creación de secuencias propias, pero considerando amenazas del oponente.

# Estructura del proyecto
.
├── agente-connecta6.cpp   # Implementación del agente
├── pb/                    # Archivos protobuf y gRPC
├── Dockerfile             # Configuración del contenedor del agente
├── docker-compose.yml     # Orquestación del sistema