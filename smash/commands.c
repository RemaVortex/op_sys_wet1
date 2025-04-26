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



