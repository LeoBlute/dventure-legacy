mkdir -p ../build
gcc -O0 xlib_dventure.c -o ../build/dventure -lX11 -lGL -DDVENTURE_DEBUG -D'DVENTURE_CODE_PATH="/home/lag/Documents/Projects/dventure/code"'
