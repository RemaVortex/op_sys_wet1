#define _POSIX_C_SOURCE 200809L

#include <stdio.h>       // בשביל printf ו-perror
#include <stdlib.h>      // בשביל exit
#include <sys/types.h>   // בשביל pid_t
#include <signal.h>      // בשביל sigaction, sigemptyset, SA_RESTART, SIGKILL, SIGINT
#include <unistd.h>      // בשביל kill ו-setpgrp

#include "commands.h"    // החיבור למשתנים גלובליים
#include "signals.h"     // אם יש לך כותרת משלך


void signalsHandler(int signal_num) {
    if (fgProcessPid <= 0) {
        // אין תהליך רץ בפרונטגרונד
        return;
    }

    if (signal_num == SIGINT) {
        printf("smash: caught CTRL+C\n");
        if (kill(fgProcessPid, SIGKILL) == -1) {
            perror("smash error: kill failed");
        } else {
            printf("smash: process %d was killed\n", fgProcessPid);
        }
        fgProcessPid = 0;
    }
    else if (signal_num == SIGTSTP) {
        printf("smash: caught CTRL+Z\n");
        if (kill(fgProcessPid, SIGSTOP) == -1) {
            perror("smash error: kill failed");
        } else {
            printf("smash: process %d was stopped\n", fgProcessPid);
            addJob(&jobList, fgProcessPid, fgProcessCmd, true); // True = stopped
        }
        fgProcessPid = 0;
    }
}


void initSignals() {
    struct sigaction sa;
    sa.sa_handler = signalsHandler; // מצביע לפונקציה שתטפל בסיגנלים
    sigemptyset(&sa.sa_mask);        // לא לחסום סיגנלים אחרים בזמן טיפול
    sa.sa_flags = SA_RESTART;        // להפעיל קריאות מערכת מחדש אם נפסקו

    // טיפול ב־SIGINT
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("smash error: sigaction failed (SIGINT)");
        exit(1);
    }

    // טיפול ב־SIGTSTP
    if (sigaction(SIGTSTP, &sa, NULL) == -1) {
        perror("smash error: sigaction failed (SIGTSTP)");
        exit(1);
    }
}


/*
 * // signals.c - טיפול בסיגנלים ב-Shell smash

#define _POSIX_C_SOURCE 200809L // כדי לאפשר פונקציות POSIX מתקדם כמו sigaction

#include <stdio.h>      // עבור printf ו-perror
#include <stdlib.h>     // עבור exit
#include <sys/types.h>  // עבור טיפוסי pid_t
#include <signal.h>     // עבור sigaction, kill, SIGINT, SIGTSTP וכו'
#include <unistd.h>     // עבור קריאות מערכת כמו kill, setpgrp

#include "commands.h"   // החיבור למשתנים גלובליים של jobs
#include "signals.h"    // קובץ הכותרת של signals

// פונקציה לטיפול באותות (signals)
void signalsHandler(int signal_num)
{
    if (fgProcessPid <= 0) {
        // אין תהליך בחזית, אין מה לעשות
        return;
    }

    if (signal_num == SIGINT) { // טיפול ב-CTRL+C
        printf("smash: caught CTRL+C\n");
        if (kill(fgProcessPid, SIGKILL) == -1) { // נסה להרוג את התהליך
            perror("smash error: kill failed");
        } else {
            printf("smash: process %d was killed\n", fgProcessPid);
        }
        fgProcessPid = 0; // אפס את מזהה התהליך
    }
    else if (signal_num == SIGTSTP) { // טיפול ב-CTRL+Z
        printf("smash: caught CTRL+Z\n");
        if (kill(fgProcessPid, SIGSTOP) == -1) { // נסה לעצור את התהליך
            perror("smash error: kill failed");
        } else {
            printf("smash: process %d was stopped\n", fgProcessPid);
            addJob(&jobList, fgProcessPid, fgProcessCmd, true); // הוספת התהליך לרשימת העבודות כעצור
        }
        fgProcessPid = 0; // אפס את מזהה התהליך
    }
}

// פונקציה לאתחול התנהגות הטיפול בסיגנלים
void initSignals()
{
    struct sigaction sa;
    sa.sa_handler = signalsHandler; // הצמדת הפונקציה שלנו כ-handler
    sigemptyset(&sa.sa_mask);        // בזמן הטיפול באות, לא נחסום אותות אחרים
    sa.sa_flags = SA_RESTART;        // קריאות מערכת יופעלו מחדש אוטומטית אם הופסקו

    // רישום טיפול ל-SIGINT (CTRL+C)
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("smash error: sigaction failed (SIGINT)");
        exit(1); // אם נכשל, יציאה מיידית
    }

    // רישום טיפול ל-SIGTSTP (CTRL+Z)
    if (sigaction(SIGTSTP, &sa, NULL) == -1) {
        perror("smash error: sigaction failed (SIGTSTP)");
        exit(1); // אם נכשל, יציאה מיידית
    }
}

 */