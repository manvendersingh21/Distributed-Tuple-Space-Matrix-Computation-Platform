


#ifndef TSHLIB_H
#define TSHLIB_H
#include "synergy.h"
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>


/* Constant Definitions */
#define NAME_LEN 64
#define MAX_TUPLE_SIZE 4096  // Adjust this based on system constraints



/* Function Declarations */
void TupleConnect(u_short PORT,  int OP_NUM);
int OpPut(char* Key, int length, char *value);
int OpGet(char* key, char * outputBuffer);
void OpExit(void);
int connectTsh(u_short port);
void Opshell(char *input, char *buffer);


void ConnectPORT(u_short portnumber);
int TshPUT(char* Key, int length, char *value );
int TshREAD(char* key, char * outputBuffer );
int TshGET(char* key, char * outputBuffer );

#endif // TSHLIB_H




// /*.........................................................................*/
// /*                  TSHTEST.H ------> TSH test program                     */
// /*                  February '13, Oct '18 updated by Justin Y. Shi         */
// /*.........................................................................*/

// #include "synergy.h"

// char login[NAME_LEN];

// void OpPut(char*name, int length, char*tuple);
// void OpGet(char*name, char*buffer) ;
// void OpExit(/*void*/) ;
// void OpRetrieve(/*void*/) ;
// void Opshell(char *input, char*buffer);
// void OpshellTest(char * input, int p);
// int tmp = 1;

// int tshsock ;

// int connectTsh(u_short) ;
// u_short drawMenu(/*void*/) ;

