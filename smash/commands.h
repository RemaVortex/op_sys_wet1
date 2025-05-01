#ifndef COMMANDS_H
#define COMMANDS_H

/*=============================================================================
* Includes, Defines, Usings
=============================================================================*/
#include <sys/types.h>  // כדי להצהיר על pid_t

// אחר כך שאר הספריות
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>

/*=============================================================================
* Defines
=============================================================================*/
#define CMD_LENGTH_MAX 80
#define ARGS_NUM_MAX 20
#define JOBS_NUM_MAX 100

/*=============================================================================
* Error handling macros
=============================================================================*/
#define ERROR_EXIT(msg) \
    do { \
        fprintf(stderr, "%s: %d\n%s", __FILE__, __LINE__, msg); \
        exit(1); \
    } while(0)

static inline void* _validatedMalloc(size_t size)
{
    void* ptr = malloc(size);
    if (!ptr) ERROR_EXIT("malloc");
    return ptr;
}

#define MALLOC_VALIDATED(type, size) \
    ((type*)_validatedMalloc(sizeof(type) * (size)))

/*=============================================================================
* Error codes
=============================================================================*/
typedef enum {
    INVALID_COMMAND = 0,
    // Feel free to add more values
} ParsingError;

typedef enum {
    SMASH_SUCCESS = 0,
    SMASH_QUIT,
    SMASH_FAIL
    // Feel free to add more values
} CommandResult;

/*=============================================================================
* Structs Definitions
=============================================================================*/

// Represents a parsed command
typedef struct Command {
    char* full_input; // The full input from the user
    struct {
        char* name;                      // Command name
        char* args[ARGS_NUM_MAX];         // Arguments array
        int arg_count;                   // Number of arguments
    } parsed;
    bool is_background; // If true, should run in background
} Command;

// Represents a single job
typedef struct Job {
    int job_id;             // Job ID (internal)
    char* j_command;        // Command string
    pid_t pid;              // Process ID
    time_t time_added;      // Time the job was added
    time_t time_stoped;     // Time the job was stopped
    int is_stopped;         // 1 if stopped, 0 otherwise
    bool isFirst;           // Remember first time stopped (for elapsed time)
    struct Job* next;       // Next job in linked list
} Job;

// Represents the job list
typedef struct JobList {
    Job* head;
} JobList;

extern JobList jobList; // Global variable for the job list
extern pid_t fgProcessPid;
extern char fgProcessCmd[CMD_LENGTH_MAX];


/*=============================================================================
* Function Declarations
=============================================================================*/

// General command parsing and helpers
int parseCommandExample(char* line);
void initJobList(JobList* jobList);
Command* parseCommand(char* line);
char* stripWhitespace(char* str);

// Job list management
Job* findJobById(JobList* jobList, int jobId);
Job* findJobWithMaxId(JobList* jobList, int onlyStopped);
void removeJob(JobList* jobList, int jobId);
void removeBackgroundFinishedJobs(JobList* jobList);
int getNextJobId(JobList* jobList);
void addJob(JobList* jobList, pid_t pid, const char* user_input, bool isStopped);

// Command execution
int executeCommand(Command* command, JobList* jobList, char** oldPwd);
int _executeExternal(Command* command, JobList* jobList);


int _showpid(Command* command, JobList* jobList, char** oldPwd);
int _pwd(Command* command, JobList* jobList, char** oldPwd);
int _cd(Command* command, JobList* jobList, char** oldPwd);
int _jobs(Command* command, JobList* jobList, char** oldPwd);
int _killJob(Command* command, JobList* jobList, char** oldPwd);
int _fg(Command* command, JobList* jobList, char** oldPwd);
int _bg(Command* command, JobList* jobList, char** oldPwd);
int _quit(Command* command, JobList* jobList, char** oldPwd);
int _diff(Command* command, JobList* jobList, char** oldPwd);

#endif // COMMANDS_H


/*
 * #ifndef COMMANDS_H
#define COMMANDS_H

#include <sys/types.h>   // הגדרת טיפוסים כמו pid_t
#include <stdlib.h>      // עבור malloc, free, exit
#include <stdio.h>       // עבור printf, perror
#include <time.h>        // עבור ניהול זמנים עם time_t
#include <stdbool.h>     // עבור שימוש ב-bool
#include <string.h>      // עבור פונקציות מחרוזות כמו strcpy
#include <signal.h>      // לטיפול באותות (signals)
#include <errno.h>       // לטיפול בשגיאות
#include <sys/stat.h>    // לפעולות על קבצים
#include <unistd.h>      // עבור fork, exec, getpid וכו'
#include <sys/wait.h>    // עבור ניהול תהליכים עם waitpid
#include <ctype.h>       // עבור בדיקות תווים כמו isdigit

// הגדרות כלליות
#define CMD_LENGTH_MAX 80    // אורך מקסימלי של פקודת משתמש
#define ARGS_NUM_MAX 20      // מספר מקסימלי של ארגומנטים לפקודה
#define JOBS_NUM_MAX 100     // מספר מקסימלי של עבודות שניתן לנהל

// מקרו להדפסת הודעת שגיאה ויציאה מהתוכנית
#define ERROR_EXIT(msg) \
    do { \
        fprintf(stderr, "%s: %d\n%s", __FILE__, __LINE__, msg); \
        exit(1); \
    } while(0)

// פונקציית עזר להקצאת זיכרון עם בדיקה
static inline void* _validatedMalloc(size_t size)
{
    void* ptr = malloc(size);
    if (!ptr) ERROR_EXIT("malloc"); // אם נכשל, הדפס הודעה וצא
    return ptr;
}

// מקרו להקצאה בטוחה של מערך מטיפוס כלשהו
#define MALLOC_VALIDATED(type, size) \
    ((type*)_validatedMalloc(sizeof(type) * (size)))

// קוד שגיאה עבור ניתוח פקודות
typedef enum {
    INVALID_COMMAND = 0, // פקודה לא חוקית
} ParsingError;

// קודי תוצאה אפשריים לביצוע פקודה
typedef enum {
    SMASH_SUCCESS = 0, // הצלחה
    SMASH_QUIT,        // סיום ריצה (quit)
    SMASH_FAIL         // כישלון בביצוע
} CommandResult;

// מבנה לייצוג פקודה לאחר ניתוח
typedef struct Command {
    char* full_input; // הפקודה המלאה כפי שהוקלדה
    struct {
        char* name;                 // שם הפקודה
        char* args[ARGS_NUM_MAX];    // מערך ארגומנטים
        int arg_count;               // מספר הארגומנטים
    } parsed;
    bool is_background;              // האם הפקודה אמורה לרוץ ברקע (&)
} Command;

// מבנה לייצוג עבודה (תהליך מנוהל)
typedef struct Job {
    int job_id;            // מזהה עבודה פנימי (מתחיל ב-1 ועולה)
    char* j_command;        // טקסט הפקודה שהריצה את העבודה
    pid_t pid;              // מזהה תהליך (PID)
    time_t time_added;      // זמן הוספת העבודה
    time_t time_stoped;     // זמן עצירת העבודה (אם נעצרה)
    int is_stopped;         // 1 אם נעצרה, 0 אם עדיין רצה
    bool isFirst;           // האם זו הפעם הראשונה שעצרה
    struct Job* next;       // מצביע לעבודה הבאה (לרשימה מקושרת)
} Job;

// רשימת עבודות — מצביע לראש הרשימה
typedef struct JobList {
    Job* head; // ראש רשימת העבודות
} JobList;

// משתנים גלובליים
extern JobList jobList;                     // רשימת העבודות הראשית
extern pid_t fgProcessPid;                   // מזהה תהליך שרץ בפרונט
extern char fgProcessCmd[CMD_LENGTH_MAX];    // הפקודה שרצה כרגע בפרונט

// הצהרות על פונקציות
int parseCommandExample(char* line);                        // דוגמת פונקציה לפירוק פקודה
void initJobList(JobList* jobList);                         // אתחול רשימת העבודות
Command* parseCommand(char* line);                          // ניתוח מחרוזת פקודה
char* stripWhitespace(char* str);                           // הסרת רווחים מהתחלה וסוף

Job* findJobById(JobList* jobList, int jobId);               // מציאת עבודה לפי ID
Job* findJobWithMaxId(JobList* jobList, int onlyStopped);    // מציאת עבודה עם ID מקסימלי
void removeJob(JobList* jobList, int jobId);                 // הסרת עבודה לפי ID
void removeBackgroundFinishedJobs(JobList* jobList);         // הסרת עבודות רקע שהסתיימו
int getNextJobId(JobList* jobList);                          // קבלת ID הבא לרשימת עבודות
void addJob(JobList* jobList, pid_t pid, const char* user_input, bool isStopped); // הוספת עבודה חדשה

int executeCommand(Command* command, JobList* jobList, char** oldPwd);           // הרצת פקודה
int _executeExternal(Command* command, JobList* jobList);                        // הרצת פקודה חיצונית

// מימוש הפקודות הפנימיות
int _showpid(Command* command, JobList* jobList, char** oldPwd); // הדפסת PID
int _pwd(Command* command, JobList* jobList, char** oldPwd);     // הדפסת תקייה נוכחית
int _cd(Command* command, JobList* jobList, char** oldPwd);      // שינוי תקייה
int _printJobList(Command* command, JobList* jobList, char** oldPwd); // הדפסת רשימת עבודות
int _killJob(Command* command, JobList* jobList, char** oldPwd); // שליחת סיגנל להרוג עבודה
int _fg(Command* command, JobList* jobList, char** oldPwd);      // העברת עבודה לרקע לפרונט
int _bg(Command* command, JobList* jobList, char** oldPwd);      // החזרת עבודה מופסקת לרקע
int _quit(Command* command, JobList* jobList, char** oldPwd);    // יציאה מסודרת מהתוכנה
int _diff(Command* command, JobList* jobList, char** oldPwd);    // השוואת קבצים

#endif // COMMANDS_H

 */
