#!/bin/zsh
cc main.c `pkg-config --libs --cflags raylib` -o main; ./main
