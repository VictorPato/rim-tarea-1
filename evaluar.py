import sys
import os.path

def get_videoname(filepath):
    name = filepath.lower()
    if name.endswith(".mp4") or name.endswith(".mpg"):
        return name[0:-4]
    return name

class Deteccion:
    def __init__(self, num_linea, linea):
        partes = linea.split("\t")
        if len(partes) != 4:
            raise Exception("formato incorrecto (debe ser 4 columnas separadas por tab)")
        television = get_videoname(partes[0])
        #los tiempos pueden ser con milisegundos
        desde = round(float(partes[1]), 3)
        largo = round(float(partes[2]), 3)
        comercial = get_videoname(partes[3])
        if television == "":
            raise Exception("nombre television invalido")
        if comercial == "":
            raise Exception("nombre comercial invalido")
        if desde < 0 or largo <= 0:
            raise Exception("limites incorrectos desde={} largo={}".format(desde, largo))
        self.num_linea = num_linea
        self.linea = linea
        self.television = television
        self.desde = desde
        self.largo = largo
        self.comercial = comercial

def leer_archivo_detecciones(filename):
    if not os.path.isfile(filename):
        raise Exception("no existe el archivo {}".format(filename))
    detecciones = []
    cont_lineas = 0
    with open(filename) as f:
        for linea in f:
            cont_lineas += 1
            try:
                linea = linea.rstrip("\r\n")
                if linea == "" or linea.startswith("#"):
                    continue
                det = Deteccion(cont_lineas, linea)
                detecciones.append(det)
            except Exception as ex:
                print ("Error {} (linea {}): {}".format(filename, cont_lineas, ex))
    return detecciones

def filtrar_gt(detecciones, lista_gt):
    videos_tv = set()
    for det in detecciones:
        videos_tv.add(det.television)
    lista = []
    for gt in lista_gt:
        if gt.television in videos_tv:
            lista.append(gt)
    return lista

def interseccion(deteccion1, deteccion2):
    ini1 = deteccion1.desde
    end1 = deteccion1.desde + deteccion1.largo
    ini2 = deteccion2.desde
    end2 = deteccion2.desde + deteccion2.largo
    inter = min(end1, end2) - max(ini1, ini2)
    union = max(end1, end2) - min(ini1, ini2)
    if inter <= 0 or union <= 0:
        return 0
    return inter / union

def buscar_gt(det, lista_gt):
    gt_found = None
    best_inter = 0
    for det_gt in lista_gt:
        if det.television != det_gt.television or det.comercial != det_gt.comercial:
            continue
        inter = interseccion(det, det_gt)
        if inter > best_inter:
            gt_found = det_gt
            best_inter = inter
    return gt_found, best_inter

class Correcta:
    def __init__(self, det, gt, inter):
        self.det = det
        self.gt = gt
        self.inter = inter

def evaluar_detecciones(detecciones, detecciones_gt):
    correctas = []
    repetidas = []
    incorrectas = []
    correctas_ids = set()
    for det in detecciones:
        gt_found, inter = buscar_gt(det, detecciones_gt)
        if gt_found is None:
            incorrectas.append(det)
        elif gt_found.num_linea in correctas_ids:
            repetidas.append(det)
        else:
            correctas.append(Correcta(det, gt_found, inter))
            correctas_ids.add(gt_found.num_linea)
    return correctas, repetidas, incorrectas


if len(sys.argv) < 2:
    print ("CC5213 - Evaluacion Tarea 1 (version 1)")
    print ("Uso: {} [archivo_detecciones.txt]".format(sys.argv[0]))
    sys.exit(1)

filename = sys.argv[1]

detecciones = leer_archivo_detecciones(filename)

print ("{} detecciones en archivo {}".format(len(detecciones), filename))

#cargar el ground-truth
detecciones_gt = leer_archivo_detecciones("gt.txt")

#seleccionar del ground-truth solo las detecciones que los videos de television
relevantes_gt = filtrar_gt(detecciones, detecciones_gt)

#evaluar, retorna las detecciones separadas por su resultado
correctas, repetidas, incorrectas = evaluar_detecciones(detecciones, relevantes_gt)

#imprimir las correctas
if len(correctas) > 0:
    print ("CORRECTAS={}".format(len(correctas)))
    for cor in correctas:
        print ("    #{}: {}    //Real: {} {} (IoU={}%)".format(cor.det.num_linea, cor.det.linea, cor.gt.desde, cor.gt.largo, round(100*cor.inter, 1)))
#imprimir las repetidas
if len(repetidas) > 0:
    print ("REPETIDAS={}".format(len(repetidas)))
    for det in repetidas:
        print ("    #{}: {}".format(det.num_linea, det.linea))
#imprimir las incorrectas
if len(incorrectas) > 0:
    print ("INCORRECTAS={}".format(len(incorrectas)))
    for det in incorrectas:
        print ("    #{}: {}".format(det.num_linea, det.linea))
#resumen
print ("Evaluacion:")
print ("  Comerciales detectados correctamente: {}".format(len(correctas)))
if len(correctas) > 0:
    suma = 0
    for cor in correctas:
        suma += cor.inter
    pct_inter = suma / len(correctas)
    print ("  Exactitud de las detecciones (promedio IoU): {}%".format(round(100*pct_inter, 1)))
print ("  Detecciones falsas: {}".format(len(incorrectas)))
real = max(0, len(correctas) - len(incorrectas))
print ("  Resultado final (correctas menos falsas): {}% ({} de {})".format(round(100*real/float(len(relevantes_gt)),1), real, len(relevantes_gt)))
