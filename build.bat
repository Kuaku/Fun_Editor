@echo off

set CC=gcc
set CFLAGS=-Iinclude -Isrc
set LDFLAGS=-lopengl32 -lgdi32 -lwinmm

set SRC=^
 src\main.c^
 src\platform\platform.c^
 src\statistics\statistics.c^
 src\filesystem\filesystem.c^
 src\text_buffer\text_buffer.c^
 src\command\command.c^
 src\input\input.c^
 src\modal\modal.c^
 src\modal\modals\statistics_modal.c^
 src\modal\modals\buffer_list.c^
 src\modal\modals\string_input.c^
 src\modal\modals\file_explorer.c^
 src\editor\editor.c^
 src\render\render.c

%CC% %CFLAGS% %SRC% libraylib.a -o main.exe %LDFLAGS%
