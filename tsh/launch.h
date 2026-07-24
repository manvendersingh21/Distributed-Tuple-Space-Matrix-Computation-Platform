/*.........................................................................*/
/*                  TSHTEST.H ------> TSH test program                     */
/*                  February '13, Oct '18 updated by Justin Y. Shi         */
/*.........................................................................*/

#ifndef LAUNCH_H
#define LAUNCH_H

#include "tshlib.h"

#define MAX_OUTPUT 4056  // Increased buffer size for shell output

// Launches a shell command using the tuple space shell (Opshell)
// 'input' is the shell command string
// 'PORT' is the TSH server's port
void launch_shell_command( char *input, int PORT);

#endif // LAUNCH_H
