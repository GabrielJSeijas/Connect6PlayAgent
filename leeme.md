¡Claro, Gabriel! Aquí tienes un desglose detallado de la lógica que armamos para que se lo expliques a tu compañero. Esta estructura sigue las reglas de Inteligencia Artificial I de la USB.
1. include/board.hpp y src/board.cpp

Es el motor físico del juego. Representa el tablero de 19×19 líneas.

    Board(): El constructor que inicializa todo en vacío (Player::NONE).

    reset(): Limpia el tablero para una nueva partida.

    placeStone(x, y, p): Coloca una ficha de color p en la intersección (x, y).

    isValidMove(x, y): Verifica que la coordenada esté dentro del rango (0−18) y que la casilla no esté ocupada.

    checkWin(p): La función más crítica. Revisa en 4 direcciones (horizontal, vertical y 2 diagonales) si el jugador p logró alinear 6 o más fichas.

2. include/agent.hpp y src/agent.cpp

Es el cerebro de la IA. Aquí implementamos la búsqueda adversaria.

    getBestMove(...): Es la función que llama el main.

        Usa el "Parche de Proximidad": En lugar de evaluar 361 celdas, solo mira las que están cerca de fichas ya puestas para ahorrar tiempo y evitar el Timeout de 10 segundos.

        Decide si poner 1 o 2 piedras según lo que pida el servidor (stones_required).

    alphaBeta(...): Implementa el algoritmo de Poda Alfa-Beta.

        Explora el árbol de decisiones hacia el futuro.

        Poda: Si encuentra una rama que es peor que una ya evaluada, deja de buscar ahí para ganar velocidad.

    evaluate(...): La función heurística.

        Le da puntaje al tablero:

            Muchos puntos por 6 en línea (victoria).

            Puntos altos por 4 o 5 en línea (amenazas).

            Defensa: Penaliza doble si el oponente tiene líneas largas, obligando al bot a bloquear.

3. src/main.cpp

Es el comunicador. Traduce los mensajes de red a objetos que tu IA entiende.

    Connect6Client(...): Configura la conexión con el servidor usando gRPC.

    Play(): El bucle infinito de la partida.

        Registro: Envía el nombre del equipo al servidor.

        Lectura: Recibe el GameState (tablero actual, tu color, si es tu turno).

        Traducción: Convierte el tablero de gRPC (repeated Row) a tu matriz Board de C++.

        Respuesta: Llama a la IA y empaqueta las coordenadas en un mensaje Move para enviarlo de vuelta.

4. Archivos de Infraestructura

    CMakeLists.txt: Configura la compilación, busca las librerías de gRPC/Protobuf y genera los archivos .pb.h automáticamente.

    Dockerfile: Crea una imagen de Ubuntu 24.04 con todo lo necesario para que el profesor pueda ejecutar tu código sin instalar nada en su PC.

    docker-compose.yml: Levanta dos contenedores: el servidor de la arena y tu IA, conectándolos en una red privada.

¡Claro, Gabriel! Como eres estudiante de la USB y estás usando WSL (Ubuntu) en tu laptop ASUS TUF, la buena noticia es que lo que hagas en WSL es técnicamente Linux, pero aquí te explico cómo manejarlo en ambos mundos para que tu partner no tenga dudas.
🐧 En Linux (y WSL 2)

Dado que ya tienes instalado cmake, gRPC y protobuf, tienes dos formas de compilar y ejecutar:
Opción A: Usando Docker (Recomendado para la entrega)

Es la forma más segura porque garantiza que las librerías sean las correctas. 

    Situarse en la raíz del proyecto (donde está el docker-compose.yml).

    Construir y levantar:
    Bash

    docker compose up --build

    Ver el juego: Abre tu navegador en http://localhost:8080. 

Opción B: Compilación Nativa (Para desarrollo rápido)

Si quieres compilar sin usar Docker cada vez:

    Crear carpeta de construcción:
    Bash

    mkdir -p build && cd build

    Configurar:
    Bash

    cmake ..

    Compilar:
    Bash

    make

    Ejecutar:
    Bash

    ./mi_agente

    Nota: Para que funcione, el servidor de Parilli debe estar corriendo en otra terminal.

🪟 En Windows

Para Windows, lo más sencillo es delegar todo a Docker Desktop, ya que configurar gRPC nativamente en Windows con Visual Studio es un dolor de cabeza innecesario para este proyecto.

    Instalar Docker Desktop: Asegúrate de que esté corriendo.

    WSL Integration: Como vimos antes, activa la integración con tu Ubuntu en los settings de Docker.

    PowerShell / CMD: Puedes navegar hasta la carpeta de tu proyecto y ejecutar el mismo comando:
    PowerShell

    docker-compose up --build

    Interfaz: Al igual que en Linux, accedes vía http://localhost:8080