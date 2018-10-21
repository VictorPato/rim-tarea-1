Tarea 1 - RIM

--- Dependencias
La tarea fue resuelta para C++11, utilizando OpenCV 3.4.3.
Para la compilación, se requiere CMake version >= 3.10.
No se utiliza ninguna otra librería externa.

La tarea fue resuelta en Ubuntu 18.04.1 LTS. La parte de compilación y ejecutar fue redactada para GNU/Linux.

--- Compilación
Para compilar, abrir una terminal en el directorio donde se encuentra main.cpp y CMakeLists.txt
Ejecutar el comando:
$ cmake .
Una vez generados los build files, ejecutar el comando:
$ cmake --build .
Esto generará el ejecutable rim-tarea-1.

--- Ejecutar
El ejecutable recibe 2 parámetros, el video a analizar y la carpeta donde se encuentran los comerciales.
Si no recibe 2 parametros por la linea de comandos, los pedirá durante la ejecución.
Un ejemplo de ejecución, teniendo el video y la carpeta de comerciales en la misma carpeta que el ejecutable, es el siguiente:
$ ./rim-tarea-1 mega-2014_04_25.mp4 comerciales
El programa escribirá en consola su avance.

En mi computador (CPU Pentium N3700 @ 1.60GHz 2 Core, 4GB Ram) demora entre 10 y 12 minutos con la configuración final.

En caso de querer guardar los estados intermedios para hacer mucho más rapida la deteccion de apariciones, se debe cambiar la constante WRITE_MODE al inicio del código a true;
