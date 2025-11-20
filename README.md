# Intérprete Interactivo de Comandos (Proyecto Bash)

**Estudiante:** Misael Cuenca  
**Materia:** Sistemas Operativos  
**Profesor:** Enrique Mafla  
**Fecha:** 19/11/2025  
**Universidad:** Escuela Politécnica Nacional  
**Paralelo:** GR1SW  

---

## 📌 Descripción del Proyecto

Este proyecto consiste en la implementación de un **intérprete interactivo de comandos** (un shell), similar a *bash*, desarrollado en lenguaje C y utilizando exclusivamente llamadas al sistema (*system calls*).  
El objetivo principal es comprender el funcionamiento interno de un shell, la administración de procesos y la interacción con el sistema operativo.

---

## 🚀 Funcionalidades Implementadas

### ✔ Comandos internos (built-ins)
El shell implementa los siguientes comandos sin necesidad de crear procesos nuevos:

- `cd`: Cambiar directorio actual  
- `pwd`: Mostrar el directorio actual  
- `ls`: Listar archivos y directorios  
- `mkdir`: Crear directorios  
- `rm`: Eliminar archivos y directorios vacíos  
- `cp`: Copiar archivos  
- `mv`: Mover o renombrar archivos  
- `cat`: Mostrar contenido de archivos  
- `history`: Mostrar el historial de comandos  
- `clear`: Limpiar la pantalla  

---

## 🔧 Comandos externos

El shell permite ejecutar cualquier comando disponible en el sistema, como por ejemplo:

- `whoami`
- `date`
- `sleep`
- `echo`
- `gcc`
- entre otros

Para esto se utiliza `fork()` y `execvp()` para crear procesos hijos.

---

## 📝 Características Avanzadas

### ▶ Redirección de entrada y salida
- `>` → redirección de salida  
- `>>` → redirección de salida en modo *append*  
- `<` → redirección de entrada  

Ejemplo:

```bash
ls > archivos.txt
echo "hola" >> archivos.txt
cat < archivos.txt



