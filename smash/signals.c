// signals.c
#include "signals.h"
//#define _POSIX_C_SOURCE 200809L
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include "commands.h"


void setupSignal_Handler(int signal, void (*handler)(int)) {
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask); // Make sure the signal mask is empty
    sa.sa_flags = 0; // No specific flags
    sigaction(signal, &sa, NULL);
}

void setupSignalHandlers(void) {
    setupSignal_Handler(SIGINT, handle_SIGINIT);  // Handle SIGINT (Ctrl+C)
    setupSignal_Handler(SIGTSTP, handle_SIGTSTOP); // Handle SIGTSTP (Ctrl+Z)
}


void handle_SIGTSTOP(int signal){
    printf("smash: caught CTRL+Z\n");
    if (fgProcessPid <= 0) {
            return;
        }
    if (kill(fgProcessPid, SIGKILL) == -1) {
            perror("smash: kill failed");
        } else {
            printf("smash: process %d was killed\n", fgProcessPid);
            addJob(&jobList, fgProcessPid, fgProcessCmd, true);
        }
    fgProcessPid=-1;
}
void handle_SIGINIT(int signal){
    printf("smash: caught CTRL+C\n");
    if (fgProcessPid <= 0) {
            return;
        }
    if (kill(fgProcessPid, SIGKILL) == -1) {
            perror("smash: kill failed");
        } else {
            printf("smash: process %d was killed\n", fgProcessPid);
        }
    fgProcessPid=-1;
}
