/*.........................................................................*/
/*                  TSHTEST.C ------> TSH test program                     */
/*                                                                         */
/*                  By N. Isaac Rajkumar [April '93]                       */
/*                  February '13, updated by Justin Y. Shi                 */
/*.........................................................................*/


#include "launch.h"
   // Adjust based on expected response size

void launch_shell_command( char *input, int PORT) {
    char buffer[MAX_OUTPUT];


    ConnectPORT(PORT);
    TshPUT("MY_TUPLE",11,"A_1_1");
    char x[50];
    TshREAD("MY",buffer);
    printf("%s",buffer);
    char y[50];
    TshGET("MY",y);
    printf("%s",y);




    // Print the result
 

}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <PORT> \"command\"\n", argv[0]);
        return 1;
    }

    int PORT = atoi(argv[1]); 
    char *input = argv[2];

    launch_shell_command(input, PORT);

    return 0;
}
