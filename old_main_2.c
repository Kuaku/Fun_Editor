#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "raylib.h"

#define MAX_LINE_LENGTH 512

int pointerX = 0, pointerY = 0;
int pointerWidth = 3, pointerOffset = 4;
int currentPointerX = 0, currentPointerY = 0;
double pointerSpeed = 10;
double pointerActionCooldown = 5, pointerActionTimer = 0; 
double pointerVelocityX = 0;

double deleteBaseCooldown = 5, deleteTimer = 0, deleteSpeed = 0; 

size_t lines = 0;
char* text_buffer;
char* swap_buffer;

int fontSize = 40;

char* get_line() {
    return text_buffer + pointerY * MAX_LINE_LENGTH;
}

int main(void) {
    const int screenWidth = 1200;
    const int screenHeight = 700;
    currentPointerX = pointerOffset;
    currentPointerY = pointerOffset;
    text_buffer = calloc(sizeof(char), MAX_LINE_LENGTH * lines);

    text_buffer = realloc(text_buffer, MAX_LINE_LENGTH * (lines + 1));
    strcpy((text_buffer + (MAX_LINE_LENGTH * lines)), "This is my Editor!");
    lines += 1;

    text_buffer = realloc(text_buffer, MAX_LINE_LENGTH * (lines + 1));
    strcpy((text_buffer + (MAX_LINE_LENGTH * lines)), "Cool jo!");
    lines += 1;

    InitWindow(screenWidth, screenHeight, "Fun Editor");

    SetTargetFPS(60);
    int lastTime;
    while (!WindowShouldClose()) {
        double time = GetTime();
        double delta = time - lastTime;
        lastTime = time;
        
        if (IsKeyPressed(KEY_DOWN)) {
            pointerY++;
        }
        if (IsKeyPressed(KEY_UP)) {
            pointerY--;
        }
        

        if (!IsKeyDown(KEY_LEFT) && !IsKeyDown(KEY_RIGHT)) {
            pointerVelocityX = 0;
            pointerActionTimer = 0;
        }
        if (pointerActionTimer > 0) {
            pointerActionTimer -= delta;
        } else {
            if (IsKeyDown(KEY_RIGHT)) {
                pointerX++;
                if (pointerX > strlen(get_line()) && pointerY < lines - 1) {
                    pointerX = 0;
                    pointerY++;
                }
                pointerVelocityX += 1;
                pointerActionTimer = pointerActionCooldown/fabs(pointerVelocityX);
            }
            if (IsKeyDown(KEY_LEFT)) {
                pointerX--;
                if (pointerX < 0 && pointerY > 0) {
                    pointerY--;
                    pointerX = strlen(get_line());
                }
                pointerVelocityX -= 1;
                pointerActionTimer = pointerActionCooldown/fabs(pointerVelocityX);
            }

            

        }
        
        if (pointerX < 0) {
            pointerX = 0;
        } else if (pointerX > strlen(get_line())) {
            pointerX = strlen(get_line());
        }
        
        if (pointerY < 0) {
            pointerY = 0;
        } else if (pointerY >= lines) {
            pointerY = lines - 1;
        }

        if (!IsKeyDown(KEY_BACKSPACE)) {
            deleteSpeed = 0;
            deleteTimer = 0;
        } else {
            if (deleteTimer > 0) {
                deleteTimer -= delta;
            } else {
                    if (pointerX > 0) {
                        char* line = text_buffer + (MAX_LINE_LENGTH * pointerY);
                        int len = strlen(line);
                        memmove(line + pointerX - 1, line + pointerX, len - pointerX + 1);
                        pointerX--;
                        deleteSpeed++;
                        deleteTimer = deleteBaseCooldown/fmin(deleteSpeed, 10);
                    } else {
                        // TODO: Need to remove new line!
                    }
            }
        }

        if (IsKeyPressed(KEY_ENTER)) {
            text_buffer = realloc(text_buffer, MAX_LINE_LENGTH * (lines + 1));

            char* src = text_buffer + MAX_LINE_LENGTH * pointerY + pointerX;
            char* dst = text_buffer + (pointerY + 1) * MAX_LINE_LENGTH;
            size_t move_size = MAX_LINE_LENGTH - pointerX;
            
            if (pointerY != lines - 1) {
                char* next_line_src = text_buffer + MAX_LINE_LENGTH * (pointerY + 1);
                char* next_line_dst = next_line_src + MAX_LINE_LENGTH;
                size_t next_line_move_size = MAX_LINE_LENGTH * lines - MAX_LINE_LENGTH * (pointerY + 1);
                memmove(next_line_dst, next_line_src, next_line_move_size);
            }
            
            memmove(dst, src, move_size);
            memset(src, 0, dst - src);
            
            lines += 1;
            pointerY++;
            pointerX = 0;
        }

        int key = GetCharPressed();
        while (key > 0) {

            if (key >= 32 && key <= 126) {
                char* line = text_buffer + (MAX_LINE_LENGTH * pointerY);
                int len = strlen(line);

                if (len + 1 < MAX_LINE_LENGTH) {
                    memmove(line + pointerX + 1, line + pointerX, len - pointerX + 1);
                    line[pointerX] = (char)key;
                    pointerX++;
                }
            }

            key = GetCharPressed();
        }
        BeginDrawing();
            ClearBackground(BLACK);
            for (size_t i = 0; i < lines; ++i) {
                if (i != pointerY) {
                    DrawText(text_buffer + (MAX_LINE_LENGTH * i), 0, fontSize * i, fontSize, WHITE);
                } else {
                    char temp[MAX_LINE_LENGTH] = {0};
                    strncpy(temp, text_buffer + (MAX_LINE_LENGTH * pointerY), pointerX);
                    int break_point = MeasureText(temp, fontSize);
                    DrawText(temp, 0, fontSize * i, fontSize, WHITE);
                    DrawText(text_buffer + (MAX_LINE_LENGTH * i) + pointerX, break_point + pointerWidth + pointerOffset * 2, fontSize * i, fontSize, WHITE);
                    int targetCursorX = break_point + pointerOffset;
                    int targetCursorY = fontSize * pointerY + pointerOffset;
                    double deltaX = targetCursorX - currentPointerX;
                    double deltaY = targetCursorY - currentPointerY;
                    double length_of_vector = sqrt(deltaX * deltaX + deltaY * deltaY);
                    deltaX /= length_of_vector;
                    deltaY /= length_of_vector;
                    deltaX *= pointerSpeed;
                    deltaY *= pointerSpeed;

                    if (length_of_vector <= pointerSpeed) {
                        currentPointerX = targetCursorX;
                        currentPointerY = targetCursorY;
                    } else {
                        currentPointerX += deltaX;
                        currentPointerY += deltaY;
                    }

                }
            }

            DrawRectangle(currentPointerX, currentPointerY, pointerWidth, fontSize - pointerOffset * 2, WHITE);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}