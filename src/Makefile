# Compilador
CC = gcc

# Flags de compilación
CFLAGS = -Wall -g -I. -Wvla -Wshadow

# Librerías necesarias
LIBS = -lm

# Archivos fuente y objetos
SRC = sem_mutex.c 
OBJ = $(SRC:.c=.o)

# Regla por defecto: compila los dos ejecutables
all:  sem_mutex

sem_mutex: sem_mutex.o
	$(CC) $(CFLAGS) -o sem_mutex sem_mutex.o $(LIBS)

# Regla genérica para .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Limpiar archivos generados
clean:
	rm -f *.o  sem_mutex
