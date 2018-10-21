#include <utility>
#include <fstream>
#include <iostream>
#include <time.h>
#include "opencv2/highgui.hpp"
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <cstdlib>
#include <regex>
#include <unordered_map>
#include <iomanip>

using namespace cv;

const int elejir_cada_n_frames = 5;
const int size_de_lado_vector_caracteristico = 15;
const long minima_distancia_para_reconocimiento = 5000;
const int alejamiento_maximo_para_cadena_mas_larga = 25;
const int distancia_maxima_de_frames_para_ser_parte_de_comercial = 4;
const double fps = 29.97;
const int puntaje_para_ser_un_comercial = 27;
const int puntaje_inicial = 7;
const bool WRITE_MODE = false;

/*
 * --------------------------------
 *           PARTE 1
 *   EXTRACCION DE CARACTERISTICAS
 * --------------------------------
 */


/*
 *  Extrae todos los nombres de los archivos de un directorio.
 *  Sacada de u-cursos (del profe)
 */
#if defined WIN32 || defined _WIN32
#define IS_WINDOWS 1
#endif

#if defined __linux || defined __linux__
#define IS_LINUX 1
#endif

void agregar_archivo(const std::string &dirname, const std::string &name, std::vector<std::string> &list) {
    std::string fullpath = dirname + "/" + name;
#if IS_WINDOWS
    struct stat64 st;
    int status = stat64(fullpath.c_str(), &st);
#elif IS_LINUX
    struct stat st{};
    int status = stat(fullpath.c_str(), &st);
#endif
    if (status == 0 && S_ISREG(st.st_mode)) {
        list.push_back(fullpath);
    }
}

std::vector<std::string> listar_archivos(const std::string &dirname) {
    std::vector<std::string> list;
#if IS_WINDOWS
    DIR *dp = opendir(dirname.c_str());
    if (dp == NULL) {
        std::cout << "error abriendo " << dirname << std::endl;
        return list;
    }
    struct dirent *dir_entry;
    while ((dir_entry = readdir(dp)) != NULL) {
        std::string name(dir_entry->d_name);
        agregar_archivo(dirname, name, list);
    }
    if (closedir(dp) != 0) {
        std::cout << "error cerrando " << dirname << std::endl;
    }
#elif IS_LINUX
    struct dirent **namelist = nullptr;
    int len = scandir(dirname.c_str(), &namelist, nullptr, nullptr);
    if (len < 0) {
        std::cout << "error abriendo " << dirname << std::endl;
        return list;
    }
    for (int i = 0; i < len; ++i) {
        std::string name(namelist[i]->d_name);
        agregar_archivo(dirname, name, list);
        free(namelist[i]);
    }
    free(namelist);
#endif
    std::sort(list.begin(), list.end());
    return list;
}


/*
 * Crea el vector característico de una imagen, reduciendo sus dimensiones y pasandola a gris
 */
std::vector<int> dar_vector_caracteristico(Mat frm) {
    // Achicar imagen
    resize(frm, frm, Size(size_de_lado_vector_caracteristico, size_de_lado_vector_caracteristico));
    // Gris
    cvtColor(frm, frm, COLOR_BGR2GRAY);
    // Iterar por la imagen y crear un vector caracteristico
    std::vector<int> vector_caracteristico(size_de_lado_vector_caracteristico * size_de_lado_vector_caracteristico);
    MatIterator_<uchar> it, end;
    int j = 0;
    for (it = frm.begin<uchar>(), end = frm.end<uchar>(); it != end; ++it) {
        vector_caracteristico[j] = *it;
        j++;
    }
    return vector_caracteristico;
}

/*
 *  Clase que contiene un vector de vectores caracteristicos y el nombre del video
 */
class Video {
public:
    std::string nombre;
    std::vector<std::vector<int>> vec_vec_car;

    Video() = default;

    Video(std::string nom, std::vector<std::vector<int>> vec) {
        nombre = std::move(nom);
        vec_vec_car = std::move(vec);
    }

    /*
     * Procesar un archivo y crear Video
     */
    Video(std::string nom, const std::string &source) {
        nombre = std::move(nom);
        std::cout << "Se extraen caracteristicas de " << nombre << std::endl;
        VideoCapture cap(source);
        int current_frame = 0;
        while (cap.grab()) {
            if (current_frame % elejir_cada_n_frames) {
                current_frame++;
                continue;
            }
            Mat frame;
            cap.retrieve(frame);
            std::vector<int> vec_car = dar_vector_caracteristico(frame);
            vec_vec_car.push_back(vec_car);
            if (!(current_frame % 0x400) && current_frame != 0) {
                long seg = long(current_frame / fps);
                std::cout << "Tiempo procesado: " << seg / 60 << " minutos " << seg % 60 << " segundos" << std::endl;
            }
            current_frame++;
        }
    }

    /*
     * Constructor desde un archivo:
     */
    explicit Video(const std::string &nombreArchivo) {
        std::ifstream inputFile(nombreArchivo);
        inputFile >> nombre;
        std::string read;
        std::vector<int> vec;
        while (inputFile) {
            inputFile >> read;
            if (read == "(") {
                vec = std::vector<int>();
            } else if (read == ")") {
                vec_vec_car.push_back(vec);
            } else {
                vec.push_back(stoi(read));
            }
        }
        inputFile.close();
    }

    /*
     * Escribe un video a archivo compatible con el constructor
     */
    void aArchivo(const std::string &nombreArchivo) {
        std::ofstream outputFile;
        outputFile.open(nombreArchivo);
        outputFile << nombre << std::endl;
        for (std::vector<int> vec : vec_vec_car) {
            outputFile << "( ";
            for (int val : vec) {
                outputFile << val << ' ';
            }
            outputFile << ") ";
        }
        outputFile.close();
    }
};

/*
 * --------------------------------
 *           PARTE 2
 *    BUSQUEDA POR SIMILITUD
 * --------------------------------
 */


/*
 *  Clase para un frame de un video
 */
class Frame {
public:
    int numeroDeFrame;
    std::string nombreDePrograma;
    long distancia;

    Frame(int numeroDeFrame, std::string nombreDePrograma, long distancia) : numeroDeFrame(numeroDeFrame),
                                                                             nombreDePrograma(
                                                                                     std::move(nombreDePrograma)), distancia(distancia) {}
};

/*
 * Chequea si un archivo existe
 */
bool does_file_exist(const std::string &fileName) {
    std::ifstream infile(fileName);
    return infile.good();
}

/*
 * Da la distancia de manhattan entre 2 vectores del mismo largo
 * No saca raiz al final para hacer más rápido el cálculo
 */
long distacia_manhattan(const std::vector<int> &v1, const std::vector<int> &v2) {
    long ans{0};
    for (int i = 0; i < v1.size(); i++) {
        ans += std::abs(v1[i] - v2[i]);
    }
    return ans;
}

/*
 * Crea un vector de mismo largo que frames analizados en el video.
 * Contiene los Frame con nombre y numero de frame mas cercano al video analizado.
 */
std::vector<Frame> dar_frames_mas_cercanos(Video videoAAnalizar, std::vector<Video> comerciales) {
    std::vector<Frame> ans;
    // current_frame solo para imprimir progreso
    int current_frame = 0;
    for (const std::vector<int> &vec : videoAAnalizar.vec_vec_car) {
        if (!(current_frame % 0x400) && current_frame != 0) {
            std::cout << 100 * current_frame / videoAAnalizar.vec_vec_car.size() << "%" << std::endl;
        }
        current_frame++;
        long minDist = LONG_MAX;
        Frame frameMinimo = Frame(-1, "", minDist);
        for (Video vid : comerciales) {
            for (int i = 0; i < vid.vec_vec_car.size(); i++) {
                long dist = distacia_manhattan(vec, vid.vec_vec_car[i]);
                if (dist < minDist) {
                    minDist = dist;
                    frameMinimo = Frame(i, vid.nombre, minDist);
                }
            }
        }
        if (minDist > minima_distancia_para_reconocimiento) {
            Frame ningunFrame = Frame(-1, "x", LONG_MAX);
            ans.push_back(ningunFrame);
        } else {
            ans.push_back(frameMinimo);
        }
    }
    return ans;
}

/*
 * --------------------------------
 *           PARTE 3
 *   DETECCION DE APARICIONES
 * --------------------------------
 */

/*
 * Da la cadena mas larga consecutiva de un tipo de comercial en los frames mas cercanos
 */
std::tuple<int, int> dar_indices_cadena_mas_larga(const std::vector<Frame> &frmsMasCercanos, const std::string &nombre, int indiceInicial) {
    int alejamiento = 0;
    int best_ini = indiceInicial;
    int best_fin = indiceInicial;
    int current_ini = indiceInicial;
    int current_fin = indiceInicial;
    for (int i = indiceInicial + 1; alejamiento <= alejamiento_maximo_para_cadena_mas_larga && i < frmsMasCercanos.size(); i++) {
        if (frmsMasCercanos[i].nombreDePrograma == nombre) {
            alejamiento = 0;
            if (frmsMasCercanos[i].numeroDeFrame == frmsMasCercanos[i - 1].numeroDeFrame + 1) {
                current_fin = i;
            } else {
                if (current_fin - current_ini > best_fin - best_ini) {
                    best_ini = current_ini;
                    best_fin = current_fin;
                }
                current_ini = i;
                current_fin = i;
            }
        } else {
            if (current_fin - current_ini > best_fin - best_ini) {
                best_ini = current_ini;
                best_fin = current_fin;
            }
            current_ini = i;
            current_fin = i;
            alejamiento++;
        }
    }
    return (std::tuple<int, int>(best_ini, best_fin));
}

/*
 * Detecta el punto de partida y fin de un comercial, a partir de la cadena mas larga consecutiva
 * Luego imprime el comercial a salida
 * Retorna la posicion del vector en cual termina el comercial
 */
int detectar_e_imprimir(const std::string &nombrevideo, const std::vector<Frame> &frmsMasCercanos, const std::tuple<int, int> &cadena,
                        std::ofstream &outputFile) {
    int inicio = std::get<0>(cadena);
    int fin = std::get<1>(cadena);
    std::string nombre = frmsMasCercanos[inicio].nombreDePrograma;
    // primero para atras
    int n = frmsMasCercanos[inicio].numeroDeFrame;
    int dist = 0;
    for (int i = inicio; n > 0 && i > 0 && dist < alejamiento_maximo_para_cadena_mas_larga; i--) {
        if (frmsMasCercanos[i].nombreDePrograma == nombre) {
            dist = 0;
            if (std::abs(n - frmsMasCercanos[i].numeroDeFrame) <= distancia_maxima_de_frames_para_ser_parte_de_comercial) {
                inicio = i;
            }
        } else {
            dist++;
        }
        n--;
    }
    // ahora para adelante
    n = frmsMasCercanos[fin].numeroDeFrame;
    dist = 0;
    for (int i = fin; i < frmsMasCercanos.size() - 1; i++) {
        if (frmsMasCercanos[i].nombreDePrograma == nombre) {
            dist = 0;
            if (std::abs(n - frmsMasCercanos[i].numeroDeFrame) <= distancia_maxima_de_frames_para_ser_parte_de_comercial) {
                fin = i;
            }
        } else {
            dist++;
        }
        n++;
    }
    // ahora que se tiene el inicio y fin del comercial, hay que escribir a la salida
    double tiempoInicio = inicio / (fps / elejir_cada_n_frames);
    double tiempoDuracion = (fin - inicio) / (fps / elejir_cada_n_frames);
    outputFile << nombrevideo << "\t" << tiempoInicio << "\t" << tiempoDuracion << "\t" << nombre << std::endl;
    return fin;
}

/*
 * Recorre completo el vector de frames mas cercanos
 * Utiliza una ventana para ir buscando y detectando los lugares donde hay comerciales, de forma de pasar por cada elemento solo 1 vez
 */
void recorrer_frames_mas_cercanos(const std::string &nombrevideo, std::vector<Frame> &frmsMasCercanos, std::ofstream &outputFile) {
    // el mapa contiene el nombre de los comerciales en la ventana
    // y una tupla que contiene 0: el "puntaje" actual del comercial 1: numero del ultimo frame visto del comercial 2: numero del primer frame
    std::unordered_map<std::string, std::tuple<int, int, int>> ventana;
    /*
     * Cada vez que avanza el indice se recalcula el puntaje de cada comercial
     * Las reglas para el puntaje seran las siguientes (no excluyentes):
     * +1 punto al nombre del frame que se incorpora
     * +1 punto extra al nombre del frame que se incorpora si el frame que se incorpora es mayor que el anterior
     * -1 punto a cada comercial
     */
    for (int i = 1; i < frmsMasCercanos.size(); i++) {
        // calcular el nuevo puntaje del frame actual
        std::tuple<int, int, int> newTuple;
        std::string nombre = frmsMasCercanos[i].nombreDePrograma;
        if (nombre != "x") {
            if (ventana.count(nombre)) {
                std::tuple<int, int, int> oldTuple = ventana[nombre];
                int nuevoPuntaje = std::get<0>(oldTuple);
                if (std::get<1>(oldTuple) < frmsMasCercanos[i].numeroDeFrame) {
                    nuevoPuntaje++;
                }
                newTuple = std::tuple<int, int, int>(nuevoPuntaje, frmsMasCercanos[i].numeroDeFrame, std::get<2>(oldTuple));
            } else {
                newTuple = std::tuple<int, int, int>(puntaje_inicial, frmsMasCercanos[i].numeroDeFrame, i);
            }
            ventana[nombre] = newTuple;
        }
        // si el frame visto logra pasar el umbral y se detecta un comercial
        if (nombre != "x" && std::get<0>(newTuple) >= puntaje_para_ser_un_comercial) {
            std::tuple<int, int> inicioYFin = dar_indices_cadena_mas_larga(frmsMasCercanos, nombre, std::get<2>(newTuple));
            int finalComercialDetectado = detectar_e_imprimir(nombrevideo, frmsMasCercanos, inicioYFin, outputFile);
            i = finalComercialDetectado;
            ventana.clear();
            continue;
        } else { // si no, se resta 1 a todos los puntajes y se suma al frame actual
            auto it = ventana.begin();
            while (it != ventana.end()) {
                if (it->first != nombre) {
                    std::get<0>(it->second)--;
                    if (std::get<0>(it->second) == 0) {
                        it = ventana.erase(it);
                    }
                } else {
                    it++;
                }
            }
            for (auto &com  : ventana) {
                if (!(com.first == nombre)) {
                    std::get<0>(com.second)--;
                    if (std::get<0>(com.second) == 0) {
                        ventana.erase(com.first);
                    }
                }
            }
        }
    }
}

/*
 * Pasa el vector de frames caracteristicos a archivo
 */
void vec_de_frames_a_archivo(std::vector<Frame> vec, const std::string &nombreArchivo) {
    std::ofstream outputFile;
    outputFile.open(nombreArchivo);
    for (const Frame &frm : vec) {
        outputFile << frm.numeroDeFrame << " " << frm.nombreDePrograma << " $ " << frm.distancia << " ";
    }
    outputFile.close();
}

/*
 * De un archivo construye el vector de frames caracteristicos
 */
std::vector<Frame> archivo_a_vec_de_frames(const std::string &nombreArchivo) {
    std::ifstream inputFile(nombreArchivo);
    std::string numero;
    std::string nombre = "";
    std::string distancia;
    std::vector<Frame> ans;
    int state = 0;
    while (inputFile) {
        if (state == 0) {
            inputFile >> numero;
            state = 1;
        } else if (state == 1) {
            std::string read;
            inputFile >> read;
            if (read == "$") {
                nombre = nombre.substr(0, nombre.length() - 1);
                state = 2;
            } else {
                nombre += read + " ";
            }
        } else {
            inputFile >> distancia;
            state = 0;
            Frame frameAAgregar(stoi(numero), nombre, stol(distancia));
            nombre = "";
            ans.push_back(frameAAgregar);
        }

    }
    return ans;
}


int main(int argc, char **argv) {

    /*
     *  Primero, extraer nombre del video a analizar y procesarlo para obtener vectores caracteristicos
     */
    std::string nombreVideo;
    std::string carpetaComerciales;
    if (argc != 3) {
        std::cout << "Ingrese el nombre del video a analizar:" << std::endl;
        std::cin >> nombreVideo;
        std::cout << "Ingrese el nombre de la carpeta de comerciales" << std::endl;
        std::cin >> carpetaComerciales;
    } else {
        nombreVideo = argv[1];
        carpetaComerciales = argv[2];
    }
    time_t start, end;
    time(&start);
    Video videoATrabajar;
    if (does_file_exist(nombreVideo + ".data")) {
        std::cout << "El video ya fue procesado, se carga desde " << nombreVideo << ".data" << std::endl;
        videoATrabajar = Video(nombreVideo + ".data");
    } else if (does_file_exist(nombreVideo)) {
        videoATrabajar = Video(nombreVideo, nombreVideo);
        if (WRITE_MODE) {
            std::cout << "Se escriben las caracteristicas a " << nombreVideo + ".data" << std::endl;
            videoATrabajar.aArchivo(nombreVideo + ".data");
        }
    } else {
        std::cout << "No se pudo abrir archivo!" << std::endl;
        return (1);
    }
    time(&end);
    double dif = difftime(end, start);
    printf("Se han extraido caracteristicas en %.0lf seg.\n", dif);

    std::vector<Frame> framesMasCercanos;

    /*
     * Si no existe la data del vector del frame, se procesa
     */
    if (!does_file_exist(nombreVideo + ".data2")) {
        /*
         * Cargar los comerciales que se utilizarán para comparar.
         */
        std::vector<std::string> potencialesNombresDeComerciales = listar_archivos(carpetaComerciales);
        std::vector<std::string> nombresDeComerciales;
        std::vector<Video> comerciales;
        for (const std::string &fullpath : potencialesNombresDeComerciales) {
            if (std::regex_match(fullpath, std::regex(".*\\.mpg$|.*\\.mp4$"))) {
                std::string nombreComercial = fullpath.substr(fullpath.find('/') + 1);
                nombreComercial = nombreComercial.substr(0, nombreComercial.length() - 4);
                comerciales.emplace_back(nombreComercial, fullpath);
            }
        }

        /*
         * Se hace la busqueda por similitud, obtienendo un vector de frames mas cercanos
         */
        std::cout << "--------------------\nSe hace busqueda por similitud" << std::endl;
        time(&start);
        framesMasCercanos = dar_frames_mas_cercanos(videoATrabajar, comerciales);
        time(&end);
        dif = difftime(end, start);
        if (WRITE_MODE) {
            std::cout << "Se escribe el resultado a " << nombreVideo + ".data2" << std::endl;
            vec_de_frames_a_archivo(framesMasCercanos, nombreVideo + ".data2");
        }
        printf("Se termina la busqueda por similitud en %.0lf seg.\n", dif);
    } else {
        framesMasCercanos = archivo_a_vec_de_frames(nombreVideo + ".data2");
    }

    /*
     * Finalmente se hace la deteccion de apariciones de comerciales. Como es solo una pasada por el vector anterior es bien rapido esto
     */
    std::cout << "--------------------\nSe hace detección de apariciones" << std::endl;
    time(&start);
    std::ofstream outputFile;
    outputFile << std::fixed << std::setprecision(2);
    outputFile.open("detecciones.txt");
    std::string nombreSinPath = nombreVideo;
    if (nombreVideo.find('/') != std::string::npos) {
        nombreSinPath = nombreSinPath.substr(nombreSinPath.rfind('/') + 1);
    }
    recorrer_frames_mas_cercanos(nombreSinPath, framesMasCercanos, outputFile);
    time(&end);
    dif = difftime(end, start);
    printf("Se detectan apariciones en %.0lf seg.\n", dif);
    outputFile.close();
    printf("Listo!\n");
    return 0;
}