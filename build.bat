gcc -c main.c -o main.o -Iinclude
gcc main.o libraylib.a -o main.exe -lopengl32 -lgdi32 -lwinmm