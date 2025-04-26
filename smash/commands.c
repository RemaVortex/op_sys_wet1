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

int _pwd(Command* command, JobList* jobList){
    if(command->parsed.arg_count>0){
        fprintf(stderr, "smash error: pwd: expected 0 arguments\n");
        return SMASH_FAIL;
    }
    char cwd[CMD_LENGTH_MAX];
    if(getcwd(cwd, sizeof(cwd))==NULL){
        perror("smash error: getcwd failed");
        return SMASH_FAIL;//throw
    }
    if(command ->is_background){
        pid_t pid =fork();
        if(pid<0){
            perror("smash error: fork failed");
            exit(SMASH_FAIL);//throw
        }
        if(pid==0){
            setpgrp();
            printf("%s\n", cwd);
            exit(SMASH_SUCCESS);//throw
        }else{
            addJob(jobList, pid, command->full_input, false);//this command come from parent, so add his child ;)
            return SMASH_SUCCESS;
        }
    }
    printf("%s\n", cwd);
    return SMASH_SUCCESS;
}
static int compareJobsById(const void* a, const void* b) {
    const Job* jobA = *(const Job**)a;
    const Job* jobB = *(const Job**)b;
    return (jobA->job_id - jobB->job_id);
}

// Helper to calculate elapsed time for a job
static double _calculateElapsedTime(Job* job, time_t now) {
    if (job->is_stopped) {
        if (job->isFirst) {
            job->time_stoped = now;
            job->isFirst = false;
        }
        return difftime(job->time_stoped, job->time_added);
    }
    return difftime(now, job->time_added);
}

// Helper to print one job
static void _printJobDetails(Job* job, double elapsed) {
    printf("[%d] %s: %d %d secs %s\n",
           job->job_id,
           job->j_command,
           job->pid,
           (int)elapsed,
           job->is_stopped ? "(stopped)" : "");
}

int _printJobList(Command* command, JobList* jobList) {
    if (command->parsed.arg_count > 0) {
        fprintf(stderr, "smash error: jobs: expected 0 arguments\n");
        return INVALID_COMMAND;
    }

    if (!jobList || !jobList->head) {
        printf("\n");
        return SMASH_SUCCESS;
    }

    // Count jobs
    int count = 0;
    for (Job* temp = jobList->head; temp; temp = temp->next) {
        count++;
    }

    if (count == 0) {
        printf("\n");
        return SMASH_SUCCESS;
    }

    // Prepare array for sorting
    Job** jobs = MALLOC_VALIDATED(Job*, count);
    Job* temp = jobList->head;
    for (int i = 0; i < count; ++i) {
        jobs[i] = temp;
        temp = temp->next;
    }

    qsort(jobs, count, sizeof(Job*), compareJobsById);

    time_t now = time(NULL);

    for (int i = 0; i < count; ++i) {
        Job* job = jobs[i];
        double elapsed = _calculateElapsedTime(job, now);

        if (command->is_background) {
            pid_t pid = fork();
            if (pid < 0) {
                perror("smash error: fork failed");
                free(jobs);
                exit(SMASH_QUIT);
            }
            if (pid == 0) {
                setpgrp();
                _printJobDetails(job, elapsed);
                exit(SMASH_SUCCESS);
            } else {
                addJob(jobList, pid, command->parsed.name, false);
                free(jobs);
                return SMASH_SUCCESS;
            }
        } else {
            _printJobDetails(job, elapsed);
        }
    }

    free(jobs);
    return SMASH_SUCCESS;
}


// Helper: Validate and find the target job
static Job* _getFgTargetJob(Command* command, JobList* jobList) {
    if (command->is_background) {
        fprintf(stderr, "smash error: fg: cannot run in background\n");
        return NULL;
    }

    if (command->parsed.arg_count == 0) {
        if (!jobList || !jobList->head) {
            fprintf(stderr, "smash error: fg: jobs list is empty\n");
            return NULL;
        }
        return findJobWithMaxId(jobList, 0);
    }

    if (command->parsed.arg_count == 1) {
        char* endPtr;
        int jobId = strtol(command->parsed.args[0], &endPtr, 10);
        if (*endPtr != '\0') {
            fprintf(stderr, "smash error: fg: invalid arguments\n");
            return NULL;
        }
        Job* job = findJobById(jobList, jobId);
        if (!job) {
            fprintf(stderr, "smash error: fg: job id %d does not exist\n", jobId);
        }
        return job;
    }

    fprintf(stderr, "smash error: fg: invalid arguments\n");
    return NULL;
}

// Helper: Resume a stopped job
static int _resumeStoppedJob(Job* job) {
    if (job->is_stopped) {
        if (kill(job->pid, SIGCONT) == -1) {
            perror("smash error: kill failed");
            return SMASH_FAIL;
        }
        job->is_stopped = 0;
        job->isFirst = true;
    }
    return SMASH_SUCCESS;
}

// Helper: Handle waiting for the foreground process
static int _waitForFgProcess(JobList* jobList, Job* job) {
    int status;
    while (true) {
        if (waitpid(fgProcessPid, &status, 0) == -1) {
            if (errno == EINTR) {
                removeJob(jobList, job->job_id);
                return SMASH_FAIL;
            } else {
                perror("smash error: waitpid failed");
                fgProcessPid = -1;
                exit(1); // match your ERROR_EXIT style
            }
        }
        break;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        fprintf(stderr, "smash error: external: invalid command\n");
    }

    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        removeJob(jobList, job->job_id);
    }

    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? SMASH_SUCCESS : SMASH_FAIL;
}

// The final _fg implementation
int _fg(Command* command, JobList* jobList) {
    Job* targetJob = _getFgTargetJob(command, jobList);
    if (!targetJob) {
        return INVALID_COMMAND;
    }

    printf("[%d] %s\n", targetJob->job_id, targetJob->j_command);

    if (_resumeStoppedJob(targetJob) != SMASH_SUCCESS) {
        return SMASH_FAIL;
    }

    fgProcessPid = targetJob->pid;
    strncpy(fgProcessCmd, targetJob->j_command, sizeof(fgProcessCmd) - 1);
    fgProcessCmd[sizeof(fgProcessCmd) - 1] = '\0'; // always null-terminate

    return _waitForFgProcess(jobList, targetJob);
}
// Helper to free all jobs from the job list
void freeJobList(JobList* jobList) {
    Job* current = jobList->head;
    while (current) {
        Job* next = current->next;
        free(current->j_command);  // Assuming j_command was dynamically allocated
        free(current);
        current = next;
    }
    jobList->head = NULL;
}
static void send_signal(pid_t pid, int signal, const char* signal_name) {
    printf("sending %s... ", signal_name);
    fflush(stdout);

    if (kill(pid, signal) == -1) {
        perror("\nsmash error: kill failed");
    } else {
        printf("done\n");
    }
}

static void terminate_job(JobList* jobList, Job* job) {
    printf("[%d] %s - ", job->job_id, job->j_command);
    fflush(stdout);

    send_signal(job->pid, SIGTERM, "SIGTERM");

    // Check if process terminated within 5 seconds
    time_t start_time = time(NULL);
    int status;
    while (difftime(time(NULL), start_time) < 5) {
        pid_t result = waitpid(job->pid, &status, WNOHANG);
        if (result > 0) {
            return removeJob(jobList, job->job_id);
        }
        sleep(1);
    }

    // If still alive, send SIGKILL
    send_signal(job->pid, SIGKILL, "SIGKILL");
    removeJob(jobList, job->job_id);
}

// Helper function to terminate a single job
//static void terminate_job(JobList* jobList, Job* job) {
//    printf("[%d] %s - sending SIGTERM... ", job->job_id, job->j_command);
//    fflush(stdout);
//
//    if (kill(job->pid, SIGTERM) == -1) {
//        perror("\nsmash error: kill failed");
//        return;
//    }
//
//    int status = 0;
//    pid_t result = 0;
//    time_t start = time(NULL);
//
//    do {
//        result = waitpid(job->pid, &status, WNOHANG);
//        if (result > 0) break;
//        sleep(1);
//    } while (difftime(time(NULL), start) < 5);
//
//    if (result > 0) {
//        printf("done\n");
//    } else {
//        printf("sending SIGKILL... ");
//        fflush(stdout);
//        if (kill(job->pid, SIGKILL) == -1) {
//            perror("\nsmash error: kill failed");
//        } else {
//            printf("done\n");
//        }
//        printf("\n");
//    }
//
//    removeJob(jobList, job->job_id);
//}

// 9. quit command
int _quit(Command* command, JobList* jobList) {
    int argc = command->parsed.arg_count;
    char** argv = command->parsed.args;

    // Validate arguments
    if (argc > 1) {
        fprintf(stderr, "smash error: quit: expected 0 or 1 arguments\n");
        return SMASH_FAIL;
    }
    if (argc == 1 && strcmp(argv[0], "kill") != 0) {
        fprintf(stderr, "smash error: quit: unexpected arguments\n");
        return SMASH_FAIL;
    }

    if (argc == 1 && strcmp(argv[0], "kill") == 0) {
        // Terminate all jobs
        Job* current = jobList->head;
        while (current) {
            Job* next = current->next;  // Save next before modifying the list
            terminate_job(jobList, current);
            current = next;
        }
    }

    // Free all jobs left (in case no 'kill' was given, or after termination)
    freeJobList(jobList);

    // Exit smash
    exit(0);
}


