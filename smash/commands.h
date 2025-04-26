#ifndef COMMANDS_H
#define COMMANDS_H
/*=============================================================================
* includes, defines, usings
=============================================================================*/
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
#include <sys/types.h>
#include <ctype.h>

#define CMD_LENGTH_MAX 80
#define ARGS_NUM_MAX 20
#define JOBS_NUM_MAX 100

/*=============================================================================
* error handling - some useful macros and examples of error handling,
* feel free to not use any of this
=============================================================================*/
#define ERROR_EXIT(msg) \
    do { \
        fprintf(stderr, "%s: %d\n%s", __FILE__, __LINE__, msg); \
        exit(1); \
    } while(0);

static inline void* _validatedMalloc(size_t size)
{
    void* ptr = malloc(size);
    if(!ptr) ERROR_EXIT("malloc");
    return ptr;
}

// example usage:
// char* bufffer = MALLOC_VALIDATED(char, MAX_LINE_SIZE);
// which automatically includes error handling
#define MALLOC_VALIDATED(type, size) \
    ((type*)_validatedMalloc((size)))


/*=============================================================================
* error definitions
=============================================================================*/
typedef enum  {
	INVALID_COMMAND = 0,
	//feel free to add morxe values here or delete this
} ParsingError;

typedef enum {
	SMASH_SUCCESS = 0,
	SMASH_QUIT,
	SMASH_FAIL
	//feel free to add more values here or delete this
} CommandResult;

/*=============================================================================
* global functions
=============================================================================*/



// typedef struct Command {
//     char* full_input;            //user_input
//     char* command_name;
//     char* arguments[ARGS_NUM_MAX];
//     int argument_count;
//     bool is_background;          //&
// } Command;
typedef struct Command {
    char* full_input;
    struct {
        char* name;
        char* args[ARGS_NUM_MAX];
        int arg_count;
    } parsed;//command.parsed.args[i]

    bool is_background;
} Command;


typedef struct Job {
    int job_id;
    char* j_command;
    pid_t pid;
    time_t time_added;
    time_t seconds_elapsed; //difftime(now, job->time_added);
    int is_stopped;
    struct Job* next;//linked list
} Job;

typedef struct JobList{
    Job* head;
} JobList;
extern JobList jobList; //global :)

int parseCommandExample(char* line);
void initJobList(JobList* jobList);


#endif //COMMANDS_H
