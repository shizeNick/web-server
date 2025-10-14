# Definieren des Compilers
CC = gcc

# Definieren der Compiler-Flags
# -Wall: Alle Warnungen aktivieren
# -Wextra: Zusätzliche Warnungen aktivieren
# -std=c99: C99-Standard verwenden
CFLAGS = -Wall -Wextra -std=c99 -pedantic

# Name des Zielprogramms (Executable)
TARGET = server

# Liste aller Quellcodedateien
SOURCES = httpd.c main.c

# Liste der Objektdateien, abgeleitet von den Quellcodedateien
OBJECTS = $(SOURCES:.c=.o)

# Standardziel: Baut das Programm
all: $(TARGET)

# Regel zum Erstellen des Executables (Linken)
# Abhängigkeit: Alle Objektdateien
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) && rm -f $(OBJECTS)

# Implizite Regel zur Kompilierung von .c zu .o
# (gcc -c <Dateiname>.c -o <Dateiname>.o)
# Diese Regel wird automatisch auf alle Dateien in $(OBJECTS) angewendet.
# Die Header-Dateien (.h) werden automatisch über die Inklusion in den .c-Dateien berücksichtigt.
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Bereinigungsziel: Entfernt generierte Dateien
# clean
clean:
	rm -f $(OBJECTS) $(TARGET)
# Wichtig: Markiert Ziele, die keine tatsächlichen Dateien sind (Best Practice)
.PHONY: all clean rebuild

