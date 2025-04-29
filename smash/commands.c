//commands.c

#define _POSIX_C_SOURCE 200112L

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // קודם כל
#include <signal.h>     // אחר כך
#include <string.h>     // אחר כך
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "commands.h"



//example function for printing errors from internal commands
void perrorSmash(const char* cmd, const char* msg)
{
    fprintf(stderr, "smash error:%s%s%s\n",
        cmd ? cmd : "",
        cmd ? ": " : "",
        msg);
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
    command->parsed.args[0] = strdup(token);

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

void removeJob(JobList* jobList, int jobId) {
    if (!jobList) {
        return;
    }

    Job* current = jobList->head;
    Job* prev = NULL;

    while (current != NULL) {
        if (current->job_id == jobId) {
            if (prev == NULL) {
                // We're deleting the head
                jobList->head = current->next;
            } else {
                prev->next = current->next;
            }
            free(current->j_command);
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}





static void handleChildExecution(Command* command) {
    setpgrp();

    if (!command || !command->parsed.name) {
        fprintf(stderr, "smash error: external: invalid command\n");
        _exit(INVALID_COMMAND);
    }

    char* args[ARGS_NUM_MAX + 2];
    args[0] = command->parsed.name;

    for (int i = 0; i < command->parsed.arg_count; ++i) {
        args[i + 1] = command->parsed.args[i];
    }
    args[command->parsed.arg_count + 1] = NULL; // Null-terminate

    execvp(command->parsed.name, args);

    // If execvp returns, it failed
    if (errno == ENOENT) {
        fprintf(stderr, "smash error: external: cannot find program\n");
    } else {
        fprintf(stderr, "smash error: external: invalid command\n");
    }
    _exit(INVALID_COMMAND);
}



static int waitForChild(pid_t pid) {
    int status = 0;

    if (waitpid(pid, &status, WUNTRACED) == -1) {
        if (errno == EINTR) {
            return SMASH_FAIL; // Interrupted by signal
        } else {
            perror("smash error: waitpid failed");
            exit(1);
        }
    }

    if (WIFSTOPPED(status)) {
        // התהליך נעצר (למשל ע"י Ctrl+Z) — smash צריך לחזור ל-prompt
        return SMASH_SUCCESS;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        fprintf(stderr, "smash error: external: invalid command\n");
        return SMASH_FAIL;
    }

    // במקרה רגיל — יציאה תקינה
    return SMASH_SUCCESS;
}


int _executeExternal(Command* command, JobList* jobList) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("smash error: fork failed");
        exit(1);
    }

    if (pid == 0) {
        handleChildExecution(command); // תהליך הילד מריץ execvp
    }

    fgProcessPid = pid;
    strncpy(fgProcessCmd, command->full_input, CMD_LENGTH_MAX - 1);
    fgProcessCmd[CMD_LENGTH_MAX - 1] = '\0'; // תמיד לוודא סיום

    if (command->is_background) {
        char cleaned_input[CMD_LENGTH_MAX] = {0};
        strncpy(cleaned_input, command->parsed.name, CMD_LENGTH_MAX - 1);

        for (int i = 1; i < command->parsed.arg_count; ++i) {
            strncat(cleaned_input, " ", CMD_LENGTH_MAX - strlen(cleaned_input) - 1);
            strncat(cleaned_input, command->parsed.args[i], CMD_LENGTH_MAX - strlen(cleaned_input) - 1);
        }

        // ניקוי & אם קיים (ליתר ביטחון)
        size_t len = strlen(cleaned_input);
        if (len > 0 && cleaned_input[len-1] == '&') {
            cleaned_input[len-1] = '\0';
            if (len > 1 && cleaned_input[len-2] == ' ') {
                cleaned_input[len-2] = '\0';
            }
        }

        addJob(jobList, pid, cleaned_input, false);
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
    newJob->is_stopped = isStopped;
    newJob->isFirst = isStopped; // <== הוספה חשובה!!
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



//Build in commands

int _showpid(Command* command, JobList* jobList, char** oldPwd){
    (void)jobList;
    (void )oldPwd;

    if (command->parsed.arg_count > 0){
        fprintf(stderr, "smash error: showpid: expected 0 arguments\n");
        return SMASH_FAIL;
    }

    printf("smash pid is %d\n", getpid());
    return SMASH_SUCCESS;
}


int _cd(Command* command, JobList* jobList, char** oldPwd){
    (void)jobList;

    if (command ->parsed.arg_count != 1){
        fprintf(stderr,"smash error: cd: expected 1 arguments\n");
        return SMASH_FAIL;
    }

    char* path = command->parsed.args[0];

    if (strcmp(path, "-") == 0){
        if (oldPwd == NULL || *oldPwd == NULL){
            fprintf(stderr, "smash error: cd: old pwd not set\n");
            return SMASH_FAIL;
        }
        path = *oldPwd;
        printf("%s\n", path);
    }
    char cwd[CMD_LENGTH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("smash error: getcwd failed");
        return SMASH_FAIL;
    }

    struct stat statbuf;
    if (stat(path, &statbuf) == -1) {
        fprintf(stderr, "smash error: cd: target directory does not exist\n");
        return SMASH_FAIL;
    }

    if (!S_ISDIR(statbuf.st_mode)) {
        fprintf(stderr, "smash error: cd: %s: not a directory\n", path);
        return SMASH_FAIL;
    }

    if (chdir(path) == -1) {
        perror("smash error: chdir failed");
        return SMASH_FAIL;
    }

    if (oldPwd != NULL) {
        free(*oldPwd);
        *oldPwd = strdup(cwd);
        if (!(*oldPwd)) {
            //if I have to return error, I have to check which error to return
        }
    }

    return SMASH_SUCCESS;
}

int _killJob(Command* command, JobList* jobList, char** oldPwd)
{
    (void)oldPwd;

    if (command->parsed.arg_count != 2) {
        fprintf(stderr, "smash error: kill: invalid arguments\n");
        return SMASH_FAIL;
    }

    char* sigStr = command->parsed.args[0];
    char* jobIdStr = command->parsed.args[1];

    int sig = atoi(sigStr);
    int jobId = atoi(jobIdStr);

    if (sig <= 0 || jobId <= 0) {
        fprintf(stderr, "smash error: kill: invalid arguments\n");
        return SMASH_FAIL;
    }

    Job* job = findJobById(jobList, jobId);
    if (!job) {
        fprintf(stderr, "smash error: kill: job id %d does not exist\n", jobId);
        return SMASH_FAIL;
    }

    if (kill(job->pid, sig) == -1) {
        perror("smash error: kill failed");
        return SMASH_FAIL;
    }

    printf("signal %d was sent to pid %d\n", sig, job->pid);
    return SMASH_SUCCESS;
}


int _bg(Command* command, JobList* jobList, char** oldPwd)
{
    (void)oldPwd;

    Job* job = NULL;

    if (command->parsed.arg_count == 0) {
        job = findJobWithMaxId(jobList, 1);
        if (!job) {
            fprintf(stderr, "smash error: bg: there are no stopped jobs to resume\n");
            return SMASH_FAIL;
        }
    } else if (command->parsed.arg_count == 1) {
        int jobId = atoi(command->parsed.args[0]);
        if (jobId == 0) {
            fprintf(stderr, "smash error: bg: invalid arguments\n");
            return SMASH_FAIL;
        }
        job = findJobById(jobList, jobId);
        if (!job) {
            fprintf(stderr, "smash error: bg: job id %d does not exist\n", jobId);
            return SMASH_FAIL;
        }
    } else {
        fprintf(stderr, "smash error: bg: invalid arguments\n");
        return SMASH_FAIL;
    }

    if (!job->is_stopped) {
        fprintf(stderr, "smash error: bg: job id %d is already in background\n", job->job_id);
        return SMASH_FAIL;
    }

    printf("%s\n", job->j_command);

    if (kill(job->pid, SIGCONT) == -1) {
        perror("smash error: kill(SIGCONT) failed");
        return SMASH_FAIL;
    }

    job->is_stopped = 0;
    return SMASH_SUCCESS;
}

int _diff(Command* command, JobList* jobList, char** oldPwd)
{
    (void)jobList;
    (void)oldPwd;

    if (command->parsed.arg_count != 2) {
        fprintf(stderr, "smash error: diff: expected 2 arguments\n");
        return SMASH_FAIL;
    }

    const char* file1 = command->parsed.args[0];
    const char* file2 = command->parsed.args[1];

    struct stat stat1, stat2;
    if (stat(file1, &stat1) == -1 || stat(file2, &stat2) == -1) {
        fprintf(stderr, "smash error: diff: expected valid paths for files\n");
        return SMASH_FAIL;
    }

    if (!S_ISREG(stat1.st_mode) || !S_ISREG(stat2.st_mode)) {
        fprintf(stderr, "smash error: diff: paths are not files\n");
        return SMASH_FAIL;
    }

    FILE* fp1 = fopen(file1, "r");
    FILE* fp2 = fopen(file2, "r");

    if (!fp1 || !fp2) {
        if (fp1) fclose(fp1);
        if (fp2) fclose(fp2);
        perror("smash error: fopen failed");
        return SMASH_FAIL;
    }

    int result = 0;
    int ch1, ch2;

    while (1) {
        ch1 = fgetc(fp1);
        ch2 = fgetc(fp2);

        if (ch1 != ch2) {
            result = 1;
            break;
        }
        if (ch1 == EOF || ch2 == EOF) {
            if (ch1 != ch2) result = 1;
            break;
        }
    }

    fclose(fp1);
    fclose(fp2);

    printf("%d\n", result);
    return SMASH_SUCCESS;
}

int _pwd(Command* command, JobList* jobList, char** oldPwd){
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

int _printJobList(Command* command, JobList* jobList, char** oldPwd) {
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
int _fg(Command* command, JobList* jobList, char** oldPwd) {
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
    while (jobList->head != NULL) {
        removeJob(jobList, jobList->head->job_id);
    }
}
/*
static void send_signal(pid_t pid, int signal, const char* signal_name) {
    printf("sending %s... ", signal_name);
    fflush(stdout);

    if (kill(pid, signal) == -1) {
        perror("\nsmash error: kill failed");
    } else {
        printf("done\n");
    }
}
*/
/*
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
            removeJob(jobList, job->job_id);
        }
        sleep(1);
    }

    // If still alive, send SIGKILL
    send_signal(job->pid, SIGKILL, "SIGKILL");
    removeJob(jobList, job->job_id);
}
*/

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
int _quit(Command* command, JobList* jobList, char** oldPwd) {
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
        // Terminate all jobs properly
        Job* current = jobList->head;
        while (current) {
            printf("[%d] %s - ", current->job_id, current->j_command);
            fflush(stdout);

            if (kill(current->pid, SIGTERM) == -1) {
                perror("smash error: kill failed");
            } else {
                printf("sending SIGTERM... ");
                fflush(stdout);

                int killed = 0;
                for (int i = 0; i < 50; i++) { // Total wait ~5 seconds
                    if (kill(current->pid, 0) == -1 && errno == ESRCH) {
                        killed = 1;
                        break;
                    }
                    usleep(100000); // Wait 100 ms
                }

                if (killed) {
                    printf("done\n");
                } else {
                    printf("sending SIGKILL... ");
                    fflush(stdout);
                    if (kill(current->pid, SIGKILL) == -1) {
                        perror("smash error: kill failed");
                    }
                    printf("done\n");
                }
            }

            current = current->next;
        }
    }

    // Free all jobs left (even if no 'kill' was requested)
    freeJobList(jobList);

    // Exit smash
    exit(0);
}





/*
 * #define _POSIX_C_SOURCE 200112L // לאפשר הרחבות POSIX

#define _GNU_SOURCE // הרחבות GNU

#include <stdio.h>      // הדפסת פלטים
#include <stdlib.h>     // זיכרון ודברים כלליים
#include <unistd.h>     // פעולות מערכת
#include <signal.h>     // ניהול סיגנלים
#include <string.h>     // מחרוזות
#include <sys/types.h>  // טיפוסי מערכת
#include <sys/wait.h>   // המתנה לתהליכים
#include <errno.h>      // קודי שגיאה

#include "commands.h"   // קובץ הפקודות שלנו

// פונקציה להדפסת הודעת שגיאה סטנדרטית של smash
void perrorSmash(const char* cmd, const char* msg)
{
    fprintf(stderr, "smash error:%s%s%s\n",
        cmd ? cmd : "",
        cmd ? ": " : "",
        msg);
}

// אתחול רשימת העבודות
void initJobList(JobList* jobList){
    if (!jobList){
        ERROR_EXIT("jobList is NULL");
    }
    jobList->head = NULL; // מתחילים מרשימה ריקה
}

// פירוק שורת פקודה לפקודה מנותחת
Command* parseCommand(char* line){
    if (!line){
        return NULL;
    }

    char* clean_line = stripWhitespace(line); // הסרת רווחים מההתחלה והסוף
    if (strlen(clean_line) == 0){
        return NULL;
    }

    Command* command = MALLOC_VALIDATED(Command,1); // הקצאת Command חדש בזיכרון

    command->full_input = strdup(clean_line); // שמירת העתק של הקלט המקורי
    if (!command->full_input){
        free(command);
        return NULL;
    }

    char* token = strtok(clean_line, " "); // חלוקה לפי רווח ראשון (שם הפקודה)
    if (!token){
        free((command->full_input));
        free(command);
        return NULL;
    }

    command->parsed.name = strdup(token); // שמירת שם הפקודה
    command->parsed.args[0] = strdup(token); // גם כארגומנט ראשון

    if (!command->parsed.name){
        free(command->full_input);
        free(command);
        return NULL;
    }

    command->parsed.arg_count = 0;
    command->is_background = false;

    int i=0;
    while ((token = strtok(NULL, " ")) != NULL && i < ARGS_NUM_MAX - 1){
        if (strcmp(token, "&") == 0){
            command->is_background = true; // סימון שפקודה צריכה לרוץ ברקע
            continue;
        }
        command->parsed.args[i] = strdup(token); // הוספת ארגומנט למערך
        i++;
    }

    command->parsed.args[i] = NULL; // סיום המערך
    command->parsed.arg_count = i;

    return command;
}

// פונקציה שמסירה רווחים משני קצוות מחרוזת
char* stripWhitespace(char* str){
    if (!str){
        return str;
    }

    while (isspace((unsigned char)*str)){
        str++;
    }

    if (*str == 0){
        return str;
    }

    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)){
        end--;
    }

    *(end+1) = '\0';
    return str;
}

// מציאת עבודה ברשימה לפי מזהה עבודה
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

// הסרת עבודה מרשימת העבודות לפי מזהה
void removeJob(JobList* jobList, int jobId) {
    if (!jobList) {
        return;
    }

    Job* current = jobList->head;
    Job* prev = NULL;

    while (current != NULL) {
        if (current->job_id == jobId) {
            if (prev == NULL) {
                jobList->head = current->next;
            } else {
                prev->next = current->next;
            }
            free(current->j_command);
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

// טיפול בתהליך הילד: הרצת execvp
static void handleChildExecution(Command* command) {
    setpgrp(); // פתיחת קבוצה חדשה

    if (!command || !command->parsed.name) {
        fprintf(stderr, "smash error: external: invalid command\n");
        _exit(INVALID_COMMAND);
    }

    char* args[ARGS_NUM_MAX + 2];
    args[0] = command->parsed.name;

    for (int i = 0; i < command->parsed.arg_count; ++i) {
        args[i + 1] = command->parsed.args[i];
    }
    args[command->parsed.arg_count + 1] = NULL; // סיום מערך

    execvp(command->parsed.name, args); // ניסיון הרצה

    // אם execvp נכשל
    if (errno == ENOENT) {
        fprintf(stderr, "smash error: external: cannot find program\n");
    } else {
        fprintf(stderr, "smash error: external: invalid command\n");
    }
    _exit(INVALID_COMMAND);
}

// המתנה לסיום הילד או עצירה (Ctrl+Z)
static int waitForChild(pid_t pid) {
    int status = 0;

    if (waitpid(pid, &status, WUNTRACED) == -1) {
        if (errno == EINTR) {
            return SMASH_FAIL; // הופסק על ידי סיגנל
        } else {
            perror("smash error: waitpid failed");
            exit(1);
        }
    }

    if (WIFSTOPPED(status)) {
        return SMASH_SUCCESS;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        fprintf(stderr, "smash error: external: invalid command\n");
        return SMASH_FAIL;
    }

    return SMASH_SUCCESS;
}

// הרצת פקודה חיצונית
int _executeExternal(Command* command, JobList* jobList) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("smash error: fork failed");
        exit(1);
    }

    if (pid == 0) {
        handleChildExecution(command); // תהליך הילד
    }

    fgProcessPid = pid;
    strncpy(fgProcessCmd, command->full_input, CMD_LENGTH_MAX - 1);
    fgProcessCmd[CMD_LENGTH_MAX - 1] = '\0';

    if (command->is_background) {
        char cleaned_input[CMD_LENGTH_MAX] = {0};
        strncpy(cleaned_input, command->parsed.name, CMD_LENGTH_MAX - 1);

        for (int i = 1; i < command->parsed.arg_count; ++i) {
            strncat(cleaned_input, " ", CMD_LENGTH_MAX - strlen(cleaned_input) - 1);
            strncat(cleaned_input, command->parsed.args[i], CMD_LENGTH_MAX - strlen(cleaned_input) - 1);
        }

        size_t len = strlen(cleaned_input);
        if (len > 0 && cleaned_input[len-1] == '&') {
            cleaned_input[len-1] = '\0';
            if (len > 1 && cleaned_input[len-2] == ' ') {
                cleaned_input[len-2] = '\0';
            }
        }

        addJob(jobList, pid, cleaned_input, false);
        return SMASH_SUCCESS;
    }

    return waitForChild(pid);
}

// חישוב ID הבא לעבודה חדשה
int getNextJobId(JobList* jobList) {
    bool ids[JOBS_NUM_MAX] = {false};

    Job* current = jobList->head;

    while (current) {
        if (current->job_id >= 1 && current->job_id < JOBS_NUM_MAX) {
            ids[current->job_id] = true;
        }
        current = current->next;
    }

    for (int i = 1; i < JOBS_NUM_MAX; ++i) {
        if (!ids[i]) {
            return i;
        }
    }

    return JOBS_NUM_MAX;
}

// הוספת עבודה חדשה לרשימה
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
    newJob->is_stopped = isStopped;
    newJob->isFirst = isStopped;
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

// חיפוש העבודה עם ה-ID הכי גבוה
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

// הסרת עבודות רקע שהסתיימו
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
            *currPtr = curr->next;
            free(curr->j_command);
            free(curr);
        } else {
            perror("smash error: waitpid failed");
            exit(1);
        }
    }
}

// טבלה של פקודות פנימיות
typedef int (*CommandFunc)(Command*, JobList*, char**);

typedef struct {
    const char* name;
    CommandFunc func;
    bool needs_oldpwd;
} CommandEntry;

// הרצת פקודה כלשהי
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

    return _executeExternal(command, jobList);
}

 // פקודת showpid - הדפסת ה-PID של smash עצמו
int _showpid(Command* command, JobList* jobList, char** oldPwd){
    (void)jobList;
    (void)oldPwd;

    if (command->parsed.arg_count > 0){
        fprintf(stderr, "smash error: showpid: expected 0 arguments\n");
        return SMASH_FAIL;
    }

    printf("smash pid is %d\n", getpid());
    return SMASH_SUCCESS;
}

// פקודת cd - שינוי תיקייה
int _cd(Command* command, JobList* jobList, char** oldPwd){
    (void)jobList;

    if (command->parsed.arg_count != 1){
        fprintf(stderr,"smash error: cd: expected 1 arguments\n");
        return SMASH_FAIL;
    }

    char* path = command->parsed.args[0];

    if (strcmp(path, "-") == 0){
        if (oldPwd == NULL || *oldPwd == NULL){
            fprintf(stderr, "smash error: cd: old pwd not set\n");
            return SMASH_FAIL;
        }
        path = *oldPwd;
        printf("%s\n", path);
    }

    char cwd[CMD_LENGTH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("smash error: getcwd failed");
        return SMASH_FAIL;
    }

    struct stat statbuf;
    if (stat(path, &statbuf) == -1) {
        fprintf(stderr, "smash error: cd: target directory does not exist\n");
        return SMASH_FAIL;
    }

    if (!S_ISDIR(statbuf.st_mode)) {
        fprintf(stderr, "smash error: cd: %s: not a directory\n", path);
        return SMASH_FAIL;
    }

    if (chdir(path) == -1) {
        perror("smash error: chdir failed");
        return SMASH_FAIL;
    }

    if (oldPwd != NULL) {
        free(*oldPwd);
        *oldPwd = strdup(cwd);
        if (!(*oldPwd)) {
            // שגיאת זיכרון - לא מוגדר מה להחזיר
        }
    }

    return SMASH_SUCCESS;
}

// פקודת kill - שליחת סיגנל לעבודה לפי ID
int _killJob(Command* command, JobList* jobList, char** oldPwd)
{
    (void)oldPwd;

    if (command->parsed.arg_count != 2) {
        fprintf(stderr, "smash error: kill: invalid arguments\n");
        return SMASH_FAIL;
    }

    char* sigStr = command->parsed.args[0];
    char* jobIdStr = command->parsed.args[1];

    int sig = atoi(sigStr);
    int jobId = atoi(jobIdStr);

    if (sig <= 0 || jobId <= 0) {
        fprintf(stderr, "smash error: kill: invalid arguments\n");
        return SMASH_FAIL;
    }

    Job* job = findJobById(jobList, jobId);
    if (!job) {
        fprintf(stderr, "smash error: kill: job id %d does not exist\n", jobId);
        return SMASH_FAIL;
    }

    if (kill(job->pid, sig) == -1) {
        perror("smash error: kill failed");
        return SMASH_FAIL;
    }

    printf("signal %d was sent to pid %d\n", sig, job->pid);
    return SMASH_SUCCESS;
}

// פקודת bg - החזרת עבודה מופסקת לרקע
int _bg(Command* command, JobList* jobList, char** oldPwd)
{
    (void)oldPwd;

    Job* job = NULL;

    if (command->parsed.arg_count == 0) {
        job = findJobWithMaxId(jobList, 1); // חיפוש עבודה מופסקת עם ה-ID הגבוה ביותר
        if (!job) {
            fprintf(stderr, "smash error: bg: there are no stopped jobs to resume\n");
            return SMASH_FAIL;
        }
    } else if (command->parsed.arg_count == 1) {
        int jobId = atoi(command->parsed.args[0]);
        if (jobId == 0) {
            fprintf(stderr, "smash error: bg: invalid arguments\n");
            return SMASH_FAIL;
        }
        job = findJobById(jobList, jobId);
        if (!job) {
            fprintf(stderr, "smash error: bg: job id %d does not exist\n", jobId);
            return SMASH_FAIL;
        }
    } else {
        fprintf(stderr, "smash error: bg: invalid arguments\n");
        return SMASH_FAIL;
    }

    if (!job->is_stopped) {
        fprintf(stderr, "smash error: bg: job id %d is already in background\n", job->job_id);
        return SMASH_FAIL;
    }

    printf("%s\n", job->j_command);

    if (kill(job->pid, SIGCONT) == -1) {
        perror("smash error: kill(SIGCONT) failed");
        return SMASH_FAIL;
    }

    job->is_stopped = 0;
    return SMASH_SUCCESS;
}

// פקודת diff - השוואת שני קבצים
int _diff(Command* command, JobList* jobList, char** oldPwd)
{
    (void)jobList;
    (void)oldPwd;

    if (command->parsed.arg_count != 2) {
        fprintf(stderr, "smash error: diff: expected 2 arguments\n");
        return SMASH_FAIL;
    }

    const char* file1 = command->parsed.args[0];
    const char* file2 = command->parsed.args[1];

    struct stat stat1, stat2;
    if (stat(file1, &stat1) == -1 || stat(file2, &stat2) == -1) {
        fprintf(stderr, "smash error: diff: expected valid paths for files\n");
        return SMASH_FAIL;
    }

    if (!S_ISREG(stat1.st_mode) || !S_ISREG(stat2.st_mode)) {
        fprintf(stderr, "smash error: diff: paths are not files\n");
        return SMASH_FAIL;
    }

    FILE* fp1 = fopen(file1, "r");
    FILE* fp2 = fopen(file2, "r");

    if (!fp1 || !fp2) {
        if (fp1) fclose(fp1);
        if (fp2) fclose(fp2);
        perror("smash error: fopen failed");
        return SMASH_FAIL;
    }

    int result = 0;
    int ch1, ch2;

    while (1) {
        ch1 = fgetc(fp1);
        ch2 = fgetc(fp2);

        if (ch1 != ch2) {
            result = 1;
            break;
        }
        if (ch1 == EOF || ch2 == EOF) {
            if (ch1 != ch2) result = 1;
            break;
        }
    }

    fclose(fp1);
    fclose(fp2);

    printf("%d\n", result);
    return SMASH_SUCCESS;
}

// פקודת pwd - הדפסת התיקייה הנוכחית
int _pwd(Command* command, JobList* jobList, char** oldPwd){
    if (command->parsed.arg_count > 0) {
        fprintf(stderr, "smash error: pwd: expected 0 arguments\n");
        return SMASH_FAIL;
    }

    char cwd[CMD_LENGTH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("smash error: getcwd failed");
        return SMASH_FAIL;
    }

    if (command->is_background) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("smash error: fork failed");
            exit(SMASH_FAIL);
        }
        if (pid == 0) {
            setpgrp();
            printf("%s\n", cwd);
            exit(SMASH_SUCCESS);
        } else {
            addJob(jobList, pid, command->full_input, false);
            return SMASH_SUCCESS;
        }
    }

    printf("%s\n", cwd);
    return SMASH_SUCCESS;
}

// השוואת עבודות לפי ID (למיון)
static int compareJobsById(const void* a, const void* b) {
    const Job* jobA = *(const Job**)a;
    const Job* jobB = *(const Job**)b;
    return (jobA->job_id - jobB->job_id);
}

// חישוב זמן שחלף עבור עבודה
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

// הדפסת פרטי עבודה אחת
static void _printJobDetails(Job* job, double elapsed) {
    printf("[%d] %s: %d %d secs %s\n",
           job->job_id,
           job->j_command,
           job->pid,
           (int)elapsed,
           job->is_stopped ? "(stopped)" : "");
}

 // הדפסת רשימת העבודות
int _printJobList(Command* command, JobList* jobList, char** oldPwd) {
    if (command->parsed.arg_count > 0) {
        fprintf(stderr, "smash error: jobs: expected 0 arguments\n");
        return INVALID_COMMAND;
    }

    if (!jobList || !jobList->head) {
        printf("\n");
        return SMASH_SUCCESS;
    }

    // ספירת העבודות
    int count = 0;
    for (Job* temp = jobList->head; temp; temp = temp->next) {
        count++;
    }

    if (count == 0) {
        printf("\n");
        return SMASH_SUCCESS;
    }

    // העתקת העבודות למערך לצורך מיון
    Job** jobs = MALLOC_VALIDATED(Job*, count);
    Job* temp = jobList->head;
    for (int i = 0; i < count; ++i) {
        jobs[i] = temp;
        temp = temp->next;
    }

    qsort(jobs, count, sizeof(Job*), compareJobsById); // מיון לפי מזהה עבודה

    time_t now = time(NULL);

    for (int i = 0; i < count; ++i) {
        Job* job = jobs[i];
        double elapsed = _calculateElapsedTime(job, now);

        if (command->is_background) { // אם רוצים לרוץ ברקע
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
            _printJobDetails(job, elapsed); // הדפסת עבודה
        }
    }

    free(jobs);
    return SMASH_SUCCESS;
}

// פונקציה עזר: מציאת עבודה ל-fg לפי טענות
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
        return findJobWithMaxId(jobList, 0); // מציאת העבודה עם ה-ID הגבוה ביותר
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

// פונקציה עזר: החזרת עבודה שהייתה עצורה
static int _resumeStoppedJob(Job* job) {
    if (job->is_stopped) {
        if (kill(job->pid, SIGCONT) == -1) {
            perror("smash error: kill failed");
            return SMASH_FAIL;
        }
        job->is_stopped = 0;
        job->isFirst = true; // נחשב זמן מחדש
    }
    return SMASH_SUCCESS;
}

// פונקציה עזר: המתנה לסיום תהליך הרץ בפרונט
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
                exit(1);
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

// מימוש סופי של פקודת fg
int _fg(Command* command, JobList* jobList, char** oldPwd) {
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
    fgProcessCmd[sizeof(fgProcessCmd) - 1] = '\0';

    return _waitForFgProcess(jobList, targetJob);
}

// פונקציה עזר: שחרור כל העבודות ברשימה
void freeJobList(JobList* jobList) {
    while (jobList->head != NULL) {
        removeJob(jobList, jobList->head->job_id);
    }
}

// פקודת quit - יציאה מ-smash, עם או בלי הריגת עבודות
int _quit(Command* command, JobList* jobList, char** oldPwd) {
    int argc = command->parsed.arg_count;
    char** argv = command->parsed.args;

    if (argc > 1) {
        fprintf(stderr, "smash error: quit: expected 0 or 1 arguments\n");
        return SMASH_FAIL;
    }
    if (argc == 1 && strcmp(argv[0], "kill") != 0) {
        fprintf(stderr, "smash error: quit: unexpected arguments\n");
        return SMASH_FAIL;
    }

    if (argc == 1 && strcmp(argv[0], "kill") == 0) {
        // הריגת כל העבודות החיות
        Job* current = jobList->head;
        while (current) {
            printf("[%d] %s - ", current->job_id, current->j_command);
            fflush(stdout);

            if (kill(current->pid, SIGTERM) == -1) {
                perror("smash error: kill failed");
            } else {
                printf("sending SIGTERM... ");
                fflush(stdout);

                int killed = 0;
                for (int i = 0; i < 50; i++) {
                    if (kill(current->pid, 0) == -1 && errno == ESRCH) {
                        killed = 1;
                        break;
                    }
                    usleep(100000); // 100 מילישניות
                }

                if (killed) {
                    printf("done\n");
                } else {
                    printf("sending SIGKILL... ");
                    fflush(stdout);
                    if (kill(current->pid, SIGKILL) == -1) {
                        perror("smash error: kill failed");
                    }
                    printf("done\n");
                }
            }

            current = current->next;
        }
    }

    // ניקוי רשימת העבודות בכל מקרה
    freeJobList(jobList);

    exit(0);
}


 */