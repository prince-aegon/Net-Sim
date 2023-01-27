# A networking sim

A cellphone arch sim written in C using the Raylib library

1. Multiple UE's can be created with different starting points
2. Multiple BTS's can be created with different locations and radii
3. BTS are color-coded and named
4. Each UE connects to a unique BTS based on its distance to each BTS
5. Each BTS has info of number of UE's connected to it

Run cmd : `cc main.c pkg-config --libs --cflags raylib -o main; ./main`
