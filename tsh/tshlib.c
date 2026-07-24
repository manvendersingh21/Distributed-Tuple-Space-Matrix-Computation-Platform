#include "tshlib.h"

int status;
int tshsock;
u_short portNum;
u_short this_op;

void TupleConnect(u_short PORT, int OP_NUM) {
    this_op = htons(OP_NUM);
    tshsock = connectTsh(PORT);
    // printf("connectTsh returned %d\n", tshsock);
    if (!writen(tshsock, (char *)&this_op, sizeof(this_op))) {
        perror("TupleConnect::writen");
        exit(1);
    }
}
void ConnectPORT(u_short portnumber){
    portNum = portnumber;

}

int TshPUT(char* Key, int length, char *value ){
    // printf("portNum %d", portNum);
    TupleConnect(portNum, 401);
    return OpPut(Key, length, value);

}
int TshGET(char* key, char * outputBuffer ){
    // printf("portNum %d", portNum);
    TupleConnect(portNum, 402);
    return OpGet(key, outputBuffer);
}
int TshREAD(char* key, char * outputBuffer ){
    // printf("portNum %d", portNum);
    TupleConnect(portNum, 403);
    return OpGet(key, outputBuffer);
}

int OpPut(char* Key, int length, char *value)
{
    tsh_put_it out ;
    tsh_put_ot in ;
    char *buff,*st ;

    // printf("TSH_OP_PUT") ;
  
                 /* obtain tuple name, priority, length, */
    strncpy(out.name, Key, NAME_LEN-1);
    out.name[NAME_LEN-1]='\0';
 
    out.priority = (u_short)1;
    out.length =length ;
    buff = (char *)malloc(out.length) ;
    strncpy(buff, value, out.length-1);
    buff[out.length-1] = '\0'; 
    //              /* print data sent to TSH */
    // printf("\n\nTo TSH :\n") ;
    // printf("\nname : %s", out.name) ;
    // printf("\npriority : %d", out.priority) ;
    // printf("\nlength : %d", out.length) ;
    // printf("\ntuple : %s\n", buff) ;
 
    out.priority = htons(out.priority) ;
    out.length = htonl(out.length) ;
                 /* send data to TSH */
    if (!writen(tshsock, (char *)&out, sizeof(out)))
     {
        perror("\nOpPut::writen\n") ;
        getchar() ;
        free(buff) ;
        return 0;
     }
                 /* send tuple to TSH */
    if (!writen(tshsock, buff, ntohl(out.length)))
     {
        perror("\nOpPut::writen\n") ;
        getchar() ;
        free(buff) ;
        return 0;
     }
                 /* read result */
    if (!readn(tshsock, (char *)&in, sizeof(in)))
     {
        perror("\nOpPut::readn\n") ;
        getchar() ;
        return 0;
     }
                 /* print result from TSH */
    // printf("\n\nFrom TSH :\n") ;
    // printf("\nstatus : %d", ntohs(in.status)) ;
    // printf("\nerror : %d\n", ntohs(in.error)) ;
    if (shutdown(tshsock, SHUT_RDWR) < 0) {
        perror("OpPut: shutdown");
        return 0;
    }

    if (close(tshsock) < 0) {
        perror("OpPut: close");
        return 0;
    }
    return 1;
    // getchar() ;
}

int OpGet(char* key, char * outputBuffer) {
   tsh_get_it out ;
   tsh_get_ot1 in1 ;
   tsh_get_ot2 in2 ;
   struct in_addr addr ;
   int sd, sock ;
   char *buff ;

//    printf("TSH_OP_GET") ;
//    printf("\n----------\n") ;

   strncpy(out.expr, key, NAME_LEN-1);
				/* obtain port for return data if tuple not available */
   // This line has to revise for clusters. out.host = gethostid() ;	
   out.host = inet_addr("127.0.0.1");
   if ((sd = get_socket()) == -1)
    {
       perror("\nOpGet::get_socket\n") ;
       getchar() ; getchar() ;
       return 0;
    }
   if (!(out.port = bind_socket(sd, 0)))
    {
       perror("\nOpGet::bind_socket\n") ;
       getchar() ; getchar() ;
       return 0;
    }
   addr.s_addr = out.host ;
				/* print data  sent to TSH */
//    printf("\n\nTo TSH :\n") ;
//    printf("\nexpr : %s", out.expr) ;
//    printf("\nhost : %s", inet_ntoa(addr)) ;
//    printf("\nport : %d\n", (out.port)) ;
				/* send data to TSH */
   if (!writen(tshsock, (char *)&out, sizeof(out)))
    {
       perror("\nOpGet::writen\n") ;
       getchar() ; getchar() ;
       close(sd) ;
       return 0;
    }
				/* find out if tuple available */
   if (!readn(tshsock, (char *)&in1, sizeof(in1)))
    {
       perror("\nOpGet::readn\n") ;
       getchar() ; getchar() ;
       close(sd) ;
       return 0;
    }
				/* print whether tuple available in TSH */
//    printf("\n\nFrom TSH :\n") ;
//    printf("\nstatus : %d", ntohs(in1.status)) ;
//    printf("\nerror : %d\n", ntohs(in1.error)) ;
				/* if tuple is available read from the same */
   if (ntohs(in1.status) == SUCCESS) /* socket */
      sock = tshsock ;
   else				/* get connection in the return port */
      sock = get_connection(sd, NULL) ;
				/* read tuple details from TSH */
    // printf("socket : %d", sd);
   if (!readn(sock, (char *)&in2, sizeof(in2)))
    {
       perror("\nOpGet::readn\n") ;
       getchar() ; getchar() ;
       close(sd) ;
       return 0;
    }				/* print tuple details from TSH */
//    printf("\nname : %s", in2.name) ;
//    printf("\npriority : %d", ntohs(in2.priority)) ;
//    printf("\nlength : %d", ntohl(in2.length)) ;
   buff = (char *)malloc(ntohl(in2.length)) ;
				/* read, print  tuple from TSH */
   if (!readn(sock, buff, ntohl(in2.length))){
      perror("\nOpGet::readn\n") ;
      return 0;
   }
   else {
    //   printf("\ntuple : %s\n", buff) ;
      strncpy(outputBuffer, buff, ntohl(in2.length));
   }
   close(sd) ;
   close(sock) ;
   free(buff) ;

   close(tshsock);
   return 1;
//    getchar() ; 
}

void OpExit(void) {
    tsh_exit_ot in ;
    this_op = htons(404);
    u_short x = 38768;
    tshsock = connectTsh(x) ;
    printf("connectTsh returned %d\n", tshsock);
    if (!writen(tshsock, (char *)&this_op, sizeof(this_op))) {
        perror("TupleConnect::writen");
        exit(1);
    }

    printf("TSH_OP_EXIT") ;
    printf("\n-----------\n") ;
                 /* read TSH response */
    if (!readn(tshsock, (char *)&in, sizeof(in)))
     {
        perror("\nOpExit::readn\n") ;
        getchar() ;  getchar() ;
        return ;
     }
                 /* print TSH response */
    printf("\n\nFrom TSH :\n") ;
    printf("\nstatus : %d", ntohs(in.status)) ;
    printf("\nerror : %d\n", ntohs(in.error)) ;
    close(tshsock) ;
    getchar() ;  

}

int connectTsh(u_short port) {
    u_long tsh_host = inet_addr("127.0.0.1");
    short tsh_port = htons(port);
    int sock = get_socket();
    if (sock == -1) return -1;
    if (!do_connect(sock, tsh_host, tsh_port)) {
        close(sock);
        return -1;
    }
    return sock;
}
void Opshell(char *input, char* buffer)
{
    tsh_shell_it out;
    tsh_shell_ot in;
    char *buff;

    sprintf(out.name, "shell_%d", 1);
    out.priority = 1;
    out.length = 4096;
    
    buff = (char *)malloc(out.length);
    if (!buff) {
        perror("Opshell: malloc");
        return;
    }
    
    // Copy the input command into buff
    strncpy(buff, input, out.length - 1);
    buff[out.length - 1] = '\0';  // Ensure null-termination

    // Convert to network order
    out.priority = htons(out.priority);
    out.length = htonl(out.length);

    /* Send tuple header */
    if (!writen(tshsock, (char *)&out, sizeof(out))) {
       perror("\nOpShell::writen");
       free(buff);
       return;
    }
    
    /* Send the shell command */
    if (!writen(tshsock, buff, ntohl(out.length))) {
       perror("\nOpShell::writen");
       free(buff);
       return;
    }
    
    /* Read the result */
    if (!readn(tshsock, (char *)&in, sizeof(in))) {
       perror("\nOpShell::readn");
       free(buff);
       return;
    }

    // Copy response into the provided buffer
    strcpy(buffer, in.response);


// 1) politely half-close (tell the server we’re done)
    if (shutdown(tshsock, SHUT_RDWR) < 0) {
        perror("Opshell: shutdown");
    }

    // 2) then fully close
    if (close(tshsock) < 0) {
        perror("Opshell: close");
    }
    tshsock = -1;

    // 3) clean up
    free(buff);
 
}