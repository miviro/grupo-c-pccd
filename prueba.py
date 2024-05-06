import subprocess
import matplotlib.pyplot as plt

# Ruta al ejecutable del programa en C
#ruta_ejecutable = "./prueba"
ruta_ejecutable = "./bueno"

# Ejecutar el programa y capturar la salida
subprocess.run([ruta_ejecutable])

# Leer los datos del archivo
datos_x = []
datos_y = []
with open("TiempoParaEntrarSC.txt", "r") as archivo:
    for linea in archivo:
        x, y = map(float, linea.strip().split(','))
        datos_x.append(x)
        datos_y.append(y)

datos_z = []
datos_w = []
with open("TiempoDentroSC.txt", "r") as archivo:
    for linea in archivo:
        w, z = map(float, linea.strip().split(','))
        datos_w.append(w)
        datos_z.append(z)

datos_nodo = []
datos_msg = []
with open("MensajesXNodo.txt", "r") as archivo:
    for linea in archivo:
        nodo, msg = map(float, linea.strip().split(','))
        datos_nodo.append(nodo)
        datos_msg.append(msg)


datos_proceso = []
ciclo_vida =[]
with open("TiempoDeVida.txt", "r") as archivo:
    for linea in archivo:
        proceso, vida = map(float, linea.strip().split(','))
        datos_proceso.append(proceso)
        ciclo_vida.append(vida)        
# Graficar los datos
#plt.plot(datos_x, datos_y)
#plt.fill_between(datos_x, datos_y)
#plt.bar(datos_w, datos_z)
#plt.xlabel('Nº del proceso')
#plt.ylabel('Tiempo desde que quiere hasta que entra en la seccion critica')
#plt.title('Gráfica de rendimiento')
#plt.grid(True)
#plt.show()

fig, axes = plt.subplots(nrows=2, ncols=2, figsize=(12, 8))

# Primer subgráfico
axes[0, 0].bar(datos_x, datos_y)
axes[0, 0].set_xlabel('Numero de nodo')
axes[0, 0].set_ylabel('Tiempo total que tarda en entrar en las SC')
axes[0, 0].set_title('Tiempo de espera para entrar en SC')

# Segundo subgráfico
axes[0, 1].bar(datos_w, datos_z)
axes[0, 1].set_xlabel('Numero de nodo')
axes[0, 1].set_ylabel('Tiempo total que esta dentro de la SC')
axes[0, 1].set_title('Tiempo en SC')

# Tercer subgráfico (puedes agregar más según sea necesario)
axes[1, 0].bar(datos_nodo, datos_msg)
axes[1, 0].set_xlabel('Número de nodo')
axes[1, 0].set_ylabel('Mensajes enviados')
axes[1, 0].set_title('Total de mensajes enviados por nodo')

# Cuarto subgráfico
#axes[1, 1].bar(datos_proceso, ciclo_vida)
#axes[1, 1].set_xlabel('Numero del proceso')
#axes[1, 1].set_ylabel('Tiempo de vida')
#axes[1, 1].set_title('Ciclo de vida de cada proceso')

# Ajustar el diseño automáticamente
plt.tight_layout()

# Mostrar las gráficas
plt.show()







# Crear una figura y dos subgráficos
#plt.figure(figsize=(10, 5))

# Primer subgráfico
#plt.subplot(1, 2, 1)  # 1 fila, 2 columnas, primer subgráfico
#plt.bar(datos_x, datos_y)
#plt.xlabel('Numero de nodo')
#plt.ylabel('Tiempo total que tarda en entrar en las SC')
#plt.title('Tiempo de espera para entrar en SC')

# Segundo subgráfico
#plt.subplot(1, 2, 2)  # 1 fila, 2 columnas, segundo subgráfico
#plt.bar(datos_w, datos_z)
#plt.xlabel('Numero de nodo')
#plt.ylabel('Tiempo total que esta dentro de la SC')
#plt.title('Tiempo en SC')

# Mostrar las gráficas
#plt.tight_layout()  # Ajustar el diseño automáticamente
#plt.show()

