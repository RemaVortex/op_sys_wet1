// smash.c
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "commands.h"
#include "signals.h"

// גלובליים עבור ניהול foreground process
pid_t fgProcessPid = -1;
char fgProcessCmd[CMD_LENGTH_MAX];

// גלובלי עבור רשימת עבודות
JobList jobList;

// גלובלי עבור שמירת תיקייה קודמת
char* oldPwd = NULL;

// buffer
char _line[CMD_LENGTH_MAX];

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    char _cmd[CMD_LENGTH_MAX];

    // אתחולים
    initJobList(&jobList);
    initSignals();

    while (1) {
        printf("smash > ");
        fflush(stdout); // להבטיח שהפלט יודפס

        if (fgets(_line, CMD_LENGTH_MAX, stdin) == NULL) {
            break; // Ctrl-D -> סיום
        }

        strcpy(_cmd, _line);

        Command* command = parseCommand(_cmd);
        if (command) {
            executeCommand(command, &jobList, &oldPwd);

        }


        // ניקוי buffers לקלט הבא
        _line[0] = '\0';
        _cmd[0] = '\0';
    }

    // במקרה שיציאה תקינה
    if (oldPwd) {
        free(oldPwd);
    }

    return 0;
}


/*
 * // smash.c - קובץ הראשי של ה-Shell

#include <stdlib.h>     // עבור malloc, free
#include <string.h>     // עבור strcpy, strlen וכו'
#include <stdio.h>      // עבור printf, fgets וכו'

#include "commands.h"   // ממשקים לפקודות
#include "signals.h"    // ממשקים לטיפול באותות (signals)

// משתנים גלובליים עבור ניהול foreground process
pid_t fgProcessPid = -1;                     // מזהה תהליך שרץ בחזית
char fgProcessCmd[CMD_LENGTH_MAX];            // שם הפקודה שרצה בחזית

// גלובלי עבור רשימת עבודות
JobList jobList;                              // רשימת עבודות (jobs) שמנוהלת

// גלובלי עבור שמירת התיקייה הקודמת (cd -)
char* oldPwd = NULL;

// buffer לקריאת שורה מהמשתמש
char _line[CMD_LENGTH_MAX];

int main(int argc, char* argv[])
{
    (void)argc; // משתיק אזהרת unused variable
    (void)argv;

    char _cmd[CMD_LENGTH_MAX]; // משתנה עזר לשכפול הפקודה

    // אתחול מבנים גלובליים
    initJobList(&jobList);   // אתחול רשימת עבודות
    initSignals();           // רישום handlers לאותות

    while (1) {
        printf("smash > "); // הדפסת prompt
        fflush(stdout);     // דואג שכל הפלט יודפס למסך

        if (fgets(_line, CMD_LENGTH_MAX, stdin) == NULL) {
            break; // אם המשתמש לחץ Ctrl-D -> יציאה מהלולאה
        }

        strcpy(_cmd, _line); // שמירה של השורה שהוזנה לפענוח

        Command* command = parseCommand(_cmd); // ניתוח הפקודה לקומנד
        if (command) {
            executeCommand(command, &jobList, &oldPwd); // הרצת הפקודה
        }

        // ניקוי המחרוזות לקראת הקלט הבא
        _line[0] = '\0';
        _cmd[0] = '\0';
    }

    // בסיום, שחרור זיכרון שהוקצה ל-oldPwd אם הוקצה
    if (oldPwd) {
        free(oldPwd);
    }

    return 0; // סיום תקין
}

 */