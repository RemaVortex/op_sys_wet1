#ifndef SIGNALS_H
#define SIGNALS_H

/*=============================================================================
* includes, defines, usings
=============================================================================*/



#include <sys/wait.h>
#include <unistd.h>     // For kill
#include <sys/types.h>  // For pid_t
#include <signal.h>     // For sigaction, SA_RESTART, SIGKILL, SIGSTOP
#include <stdio.h>
#include "commands.h"

/*=============================================================================
* global functions
=============================================================================*/
void setupSignal_Handlers(void);
void handle_SIGTSTOP(int signal);
void handle_SIGINIT(int signal);
#endif //__SIGNALS_H__
