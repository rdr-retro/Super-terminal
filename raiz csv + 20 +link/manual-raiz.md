# Raiz - Gestor de Contraseñas Seguro con Cámara

---

1. Tecnologías usadas

- C++17: Lenguaje base para todo el desarrollo, aprovechando características modernas y rendimiento nativo.
- SFML (Simple and Fast Multimedia Library): Para la interfaz gráfica, manejo de ventanas, eventos, texto, sprites y entrada de usuario.  
- OpenCV 4: Biblioteca de visión por computadora para capturar imágenes directamente desde la cámara web.
- std::filesystem (C++17): Para manejar archivos y directorios de forma moderna y segura.
- XOR Encryption: Método sencillo y rápido para cifrar/descifrar texto simétricamente usando la clave generada desde la cámara.
- Multithreading y concurrencia: Aunque el programa base no usa hilos explícitamente, la librería xor_lib sí puede usarlo para generación indefinida de claves.
- UTF-8 y texto Unicode: Manejo adecuado de textos con acentos y símbolos en la interfaz gráfica.
- CMake / Make (opcional): Para facilitar compilación y gestión de dependencias (recomendado).

---

2. Requisitos previos

Como requisitos necesitas:

1. Sistema operativo

- Ubuntu 20.04 o superior (otras distros Linux similares también valen, pero aquí se asume Ubuntu).
- También se podría adaptar a Windows o macOS, pero aquí se asume Ubuntu para simplicidad.

2. Compilador

- g++ con soporte C++17. Ubuntu 20.04 trae g++ 9 o superior que cumple.
- Para verificar:  
  g++ --version

3. Librerías necesarias

- SFML 2.5+  
  Para gráficos, ventana, texto, eventos y sprites.  
  Instalación:  
  sudo apt update
  sudo apt install libsfml-dev

- OpenCV 4.x  
  Para acceder a la cámara y capturar imágenes.  
  Instalación:  
  sudo apt install libopencv-dev

- CMake (opcional pero recomendado)  
  Para gestionar el build si usas CMakeLists.txt.  
  sudo apt install cmake

4. Fuente y recursos gráficos

- Archivo de fuente Product Sans Regular.ttf  
  Coloca este archivo en el mismo directorio que el ejecutable o proyecto.
- Iconos:  
  - nocamera.png (icono cámara apagada)  
  - sicamera.png (icono cámara encendida)  
  - tree.png (imagen decorativa para menú principal)  
  Estos archivos deben estar en la carpeta del ejecutable o ruta relativa usada en el código.

---

3. Cómo compilar y ejecutar

Paso 1: Descarga el código

- Coloca tu código fuente (por ejemplo, main.cpp), la librería xor_lib (xor_lib.cpp, xor_lib.hpp), y recursos en un mismo directorio.

Paso 2: Compilar la librería

Compila el archivo .cpp de la librería:

g++ -c xor_lib.cpp -o xor_lib.o `pkg-config --cflags opencv4 sfml-all`

- pkg-config incluye las rutas necesarias para OpenCV y SFML.

Paso 3: Compilar el programa principal y enlazar

g++ main.cpp xor_lib.o -o RaizGestor `pkg-config --cflags --libs opencv4 sfml-all`

Si usas otras versiones de SFML o OpenCV, ajusta pkg-config o incluye manualmente las rutas.

Paso 4: Ejecutar

./RaizGestor

---

Uso

- Al iniciar, el programa detecta si hay cámara disponible.  
- Si la cámara no está, muestra mensaje de error y espera que presiones F5 para reintentar.  
- Desde el menú principal puedes:  
  - Crear usuario: se genera una clave desde la cámara para proteger tu Capsula.  
  - Iniciar sesión: seleccionar usuario y poner la clave generada.  
- En la Capsula puedes añadir, borrar y consultar tus entradas con servicios, usuarios y contraseñas cifradas con la libreria xor.  
- El programa cifra las contraseñas localmente usando la clave generada con la cámara, sin enviarlas a ningún lado.  

---

Estructura de carpetas
~~~
RaizGestor/
│
├── main.cpp           # Código fuente principal de la app
├── xor_lib.cpp        # Implementación librería xor_lib
├── xor_lib.hpp        # Declaraciones librería xor_lib
├── Product Sans Regular.ttf  # Fuente usada en UI
├── nocamera.png       # Icono cámara desactivada
├── sicamera.png       # Icono cámara activada
├── tree.png           # Imagen árbol menú principal
├── Capsulas/          # Carpeta donde se guardan usuarios y datos cifrados
│   ├── usuario1/
│   │   └── usuario1.dat
│   └── usuario2/
│       └── usuario2.dat
└── README.md          # Este archivo de instrucciones
~~~

