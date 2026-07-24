#ifndef MATRIX_H
#define MATRIX_H


// Include the provided Tuple Space library header
#include "tshlib.h"
#include <netinet/in.h>



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/wait.h>

// Include the persistent Tuple Space library header
#include "tshlib.h" // Make sure this points to the updated header
// size of each block in your tiled algorithm
#define TILE_SIZE 2

// buffers for tuple-space keys/values
#define KEY_BUF  32
#define VAL_BUF  32


#endif // LAUNCH_H