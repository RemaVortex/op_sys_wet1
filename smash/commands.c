//commands.c
#include "commands.h"

//example function for printing errors from internal commands
void perrorSmash(const char* cmd, const char* msg)
{
    fprintf(stderr, "smash error:%s%s%s\n",
        cmd ? cmd : "",
        cmd ? ": " : "",
        msg);
}

//example function for parsing commands
int parseCmdExample(char* line)
{
	char* delimiters = " \t\n"; //parsing should be done by spaces, tabs or newlines
	char* cmd = strtok(line, delimiters); //read strtok documentation - parses string by delimiters
	if(!cmd)
		return INVALID_COMMAND; //this means no tokens were found, most like since command is invalid
	
	char* args[MAX_ARGS];
	int nargs = 0;
	args[0] = cmd; //first token before spaces/tabs/newlines should be command name
	for(int i = 1; i < MAX_ARGS; i++)
	{
		args[i] = strtok(NULL, delimiters); //first arg NULL -> keep tokenizing from previous call
		if(!args[i])
			break;
		nargs++;
	}
	/*
	At this point cmd contains the command string and the args array contains
	the arguments. You can return them via struct/class, for example in C:
		typedef struct {
			char* cmd;
			char* args[MAX_ARGS];
		} Command;
	Or maybe something more like this:
		typedef struct {
			bool bg;
			char** args;
			int nargs;
		} CmdArgs;
	*/
}



void initJobList(JobList* jobList){
    if (!jobList){
        ERROR_EXIT("jobList is NULL"); //I have to check if I have to print this error message
    }
    jobList->head = NULL;
}

Command* parseCommand(char* line){
    if (!line){
        return NULL;
    }

    char* clean_line = stripWhitespace(line); //clean the line
    if (strlen(clean_line) == 0){
        return NULL;
    }

    Command* command = MALLOC_VALIDATED(Command,1); //הקצאתי Command חדש

    command->full_input = strdup(clean_line); //a copy of the original command
    if (!command->full_input){
        free(command);
        //if I have to return error, I have to check which error to return
    }

    char* token = strtok(clean_line, " ");
    if (!token){
        free((command->full_input));
        free(command);
        return NULL;
    }

    command->parsed.name = strdup(token);
    if (!command->parsed.name){
        free(command->full_input);
        free(command);
        //if I have to return error, I have to check which error to return
    }

    command->parsed.arg_count = 0;
    command->is_background = false;

    int i=0;
    while ((token = strtok(NULL, " ")) != NULL && i < ARGS_NUM_MAX - 1){
        if (strcmp(token, "&") == 0){
            command->is_background = true;
            continue;
        }
        command->parsed.args[i] = strdup(token); //add the argument to the arr
        if (!command->parsed.args[i]){
            //if I have to return error, I have to check which error to return
        }
        i++;
    }

    command->parsed.args[i] = NULL;
    command->parsed.arg_count = i;

    return command;

}

char* stripWhitespace(char* str){
    if (!str){
        return str; //if the str is null we return null
    }

    while (isspace((unsigned char)*str)){ //while there is a spaces in the start of the line, skip them
        str++;
    }

    if (*str == 0){ //if all the line is spaces return null (all the line spaces not the same of null/empty line)
        return str;
    }

    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)){
        end--; //remove spaces from the last of the line
    }

    *(end+1) = '\0';
    return str;
}

Job* findJobById(JobList* jobList, int jobId){
    if (!jobList){
        return NULL;
    }

    Job* current = jobList->head;
    while (current != NULL){
        if (current->job_id == jobId){
            return current;
        }
        current = current->next;
    }
    return NULL;
}

void removeJob(JobList* jobList, int jobId){
    if (!jobList){
        //if I have to return error, I have to check which error to return
    }
    Job* current = jobList->head;
    Job* prev = jobList->head;

    while (current != NULL){
        if (current->job_id == jobId){
            if (prev){
                prev->next = current->next;
            } else{
                jobList->head = current->next;
            }
            free(current->j_command);
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}


typedef int (*CommandFunc)(Command*, JobList*, char**);

typedef struct {
    const char* name;
    CommandFunc func;
    bool needs_oldpwd;
} CommandEntry;

int executeCommand(Command* command, JobList* jobList, char** oldPwd) {
    static const CommandEntry built_in_commands[] = {
        {"showpid", (CommandFunc)_showpid, false},
        {"pwd",     (CommandFunc)_pwd,     false},
        {"cd",      (CommandFunc)_cd,      true},
        {"jobs",    (CommandFunc)_printJobList, false},
        {"kill",    (CommandFunc)_killJob, false},
        {"fg",      (CommandFunc)_fg,      false},
        {"bg",      (CommandFunc)_bg,      false},
        {"quit",    (CommandFunc)_quit,    false},
        {"diff",    (CommandFunc)_diff,    false}
    };

    const char* cmd = command->parsed.name;
    size_t num_commands = sizeof(built_in_commands) / sizeof(built_in_commands[0]);

    for (size_t i = 0; i < num_commands; ++i) {
        if (strcmp(cmd, built_in_commands[i].name) == 0) {
            if (built_in_commands[i].needs_oldpwd) {
                return built_in_commands[i].func(command, jobList, oldPwd);
            } else {
                return built_in_commands[i].func(command, jobList, NULL);
            }
        }
    }

    // If not built-in
    return _executeExternal(command, jobList);
}

static void handleChildExecution(Command* command) {
    setpgrp(); 

    char* args[ARGS_NUM_MAX + 2];
    args[0] = command->parsed.name;

    for (int i = 0; i < command->parsed.arg_count; ++i) {
        args[i + 1] = command->parsed.args[i];
    }
    args[command->parsed.arg_count + 1] = NULL; // Null-terminate the args array

    execvp(command->parsed.name, args);

    // If execvp returns, it failed
    if (errno == ENOENT) {
        fprintf(stderr, "smash error: external: cannot find program\n");
    } else {
        perror("smash error: execvp failed");
    }
    _exit(INVALID_COMMAND); // Exit child process
}

static int waitForChild(pid_t pid) {
    int status = 0;
    
    while (true) {
        if (waitpid(pid, &status, 0) == -1) {
            if (errno == EINTR) {
                return SMASH_FAIL; // Interrupted by signal
            } else {
                perror("smash error: waitpid failed");
                exit(1); // Critical system call failure
            }
        }
        break;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        fprintf(stderr, "smash error: external: invalid command\n");
        return SMASH_FAIL;
    }

    return SMASH_SUCCESS;
}

int _executeExternal(Command* command, JobList* jobList) {
    if (command->parsed.arg_count == 0) {
        fprintf(stderr, "smash error: external: invalid command\n");
        return INVALID_COMMAND;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("smash error: fork failed");
        exit(1);
    }

    if (pid == 0) {
        handleChildExecution(command);
    }

    fgProcessPid = pid;
    strncpy(fgProcessCmd, command->full_input, CMD_LENGTH_MAX - 1);
    fgProcessCmd[CMD_LENGTH_MAX - 1] = '\0'; // Ensure null-termination

    if (command->is_background) {
        addJob(jobList, pid, command->full_input, false);
        return SMASH_SUCCESS;
    }

    return waitForChild(pid);
}


int getNextJobId(JobList* jobList) {
    bool ids[JOBS_NUM_MAX] = {false}; // Track used job IDs

    Job* current = jobList->head;

    while (current) {
        if (current->job_id >= 1 && current->job_id < JOBS_NUM_MAX) {
            ids[current->job_id] = true;
        }
        current = current->next;
    }

    // Find the smallest unused ID starting from 1
    for (int i = 1; i < JOBS_NUM_MAX; ++i) {
        if (!ids[i]) {
            return i;
        }
    }

    return JOBS_NUM_MAX; // All IDs taken, dont forget to chaeck this state
}





// Add a new job to the job list
void addJob(JobList* jobList, pid_t pid, const char* user_input, bool isStopped) {
    Job* newJob = MALLOC_VALIDATED(Job, 1);
    newJob->job_id = getNextJobId(jobList);
    newJob->pid = pid;
    newJob->j_command = strdup(user_input);
    if (!newJob->j_command) {
        free(newJob);
        ERROR_EXIT("strdup");
    }
    newJob->time_added = time(NULL);
    newJob->seconds_elapsed = 0; // Will be updated when needed
    newJob->is_stopped = isStopped;
    newJob->next = NULL;

    if (!jobList->head) {
        jobList->head = newJob;
    } else {
        Job* curr = jobList->head;
        while (curr->next) {
            curr = curr->next;
        }
        curr->next = newJob;
    }
}

// Find the job with the maximum job ID
Job* findJobWithMaxId(JobList* jobList, int onlyStopped) {
    Job* current = jobList->head;
    Job* maxJob = NULL;
    int maxId = -1;

    while (current) {
        if (onlyStopped && !current->is_stopped) {
            current = current->next;
            continue;
        }
        if (current->job_id > maxId) {
            maxId = current->job_id;
            maxJob = current;
        }
        current = current->next;
    }
    return maxJob;
}

void removeBackgroundFinishedJobs(JobList* jobList) {
    if (!jobList) return;

    Job** currPtr = &(jobList->head);

    while (*currPtr) {
        Job* curr = *currPtr;
        int status;
        pid_t result = waitpid(curr->pid, &status, WNOHANG);

        if (result == 0) {
            currPtr = &(curr->next);
        } else if (result > 0 || (result == -1 && errno == ECHILD)) {
            // Remove current job
            *currPtr = curr->next;
            free(curr->j_command);
            free(curr);
        } else {
            perror("smash error: waitpid failed");
            exit(1);
        }
    }
}


