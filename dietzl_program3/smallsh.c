// Filename: smallsh.c
// Author: Lawson Dietz
// Date: 2-9-21
// ASSIGNMENT 3 CS344

#include <stdio.h>
#include <string.h>
#include <stdlib.h> 
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include <fcntl.h>

#define MAXCHAR 2048 // Max char supported 
#define MAXARG 512 // Max args supported

// Global var to determine Foreground mode
// Did not know how to pass a var to (int signo)
int background_mode = 0;


// This function gets commands from a user and parses the command string finding special characters &, <, >, and $$.
// Parameters: char *arr[], char inFile[], char outFile[], int *background, int pid
// Returns: nothing
void getUserInput(char *arr[], char inFile[], char outFile[], int *background, int pid) {
	char userInput[MAXCHAR];
	int newline = 0;
	char intString[MAXCHAR];

	// Get user input
	printf(": ");
	fflush(stdout);
	fgets(userInput, MAXCHAR, stdin);	
	// Loop to remove newline from userInput, stops if i > MAXCHAR || newline != 0
	for(int i = 0; i < MAXCHAR && !newline; i++) {
		if(userInput[i] == '\n') {
			newline == 1; // Loop termination
			userInput[i] = '\0'; // Replace newline char
		}
	}
	// If input is blank
	if(!strcmp(userInput, "")) {
		arr[0] = strdup("");
		return;
	}	

	//CITATION: https://www.tutorialspoint.com/c_standard_library/c_function_strtok.htm
	const char space[2] = " ";
	char *token = strtok(userInput, space);
	
	// index
	int i = 0;
	// While token is not null
	// Parse string into tokens to find special characters
	while(token) {
		// Check token for & character
		if(!strcmp(token, "&")) {
			*background = 1;
		}
		// Check token for < character
		else if(!strcmp(token, "<")) {
			token = strtok(NULL, space);
			strcpy(inFile, token);
		}
		// Check token for > character
		else if(!strcmp(token, ">")) {
			token = strtok(NULL, space);
			strcpy(outFile, token);
		}
		else {
			// Copy command
			arr[i] = strdup(token);
			// Replace '$$' with pid
			for(int j = 0; j < MAXCHAR; j++) {
				// Find '$$'
				if(arr[i][j] == '$' && arr[i][j+1] == '$') {
					arr[i][j] = '\0';
					// Use snprintf to convert int to string
					sprintf(intString, "%d", pid);
					strcat(arr[i], intString);
				}
			}
		}
		// Next Token
		token = strtok(NULL, space);
		i++;
	}
}	

// This function handles special characters found in getUserInput() and creates and terminates child processes used to execute bash commands
// Parameters: char *arr[], char inFile[], char outFile[], int *background, int *exitStatus, struct sigation, sig
// Returns: nothing
// CITATION: 4_fork_example.c 
void execCommand(char *arr[], char inFile[], char outFile[], int *background, int *exitStatus, struct sigaction sig) {
	int result;
	int sourceFD, targetFD;
	pid_t spawnPid = -5;
	
	// Fork
	spawnPid = fork();
	switch (spawnPid) {
		case -1:
			// Fork fails
			perror("Fork() failed! Hull Breach!\n");
			exit(1);
			break;
		case 0:
			// Child processes wil now terminate to ^C
			sig.sa_handler = SIG_DFL;
			sigaction(SIGINT, &sig, NULL);
			
			// CITATION: 5_4_sortViaFiles.c + Week 5 Lecture slides
			// If inFile is not blank
			if(strcmp(inFile, "")) {
				// Open source file
				sourceFD = open(inFile, O_RDONLY);
				if(sourceFD == -1) {
					perror("Unable to open input file\n");
					exit(1);
				}
				// Redirect to source file
				result = dup2(sourceFD, 0);
				if(result == -1) {
					perror("Unable to assign input file\n");
					exit(2);
				}
				
				// Close on exec
				fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
			}

			if(strcmp(outFile, "")) {
				targetFD = open(outFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
				if(targetFD == -1) {
					perror("Unable to open output file\n");
					exit(1);
				}		
				
				result = dup2(targetFD, 1);
				if(result == -1) {		
					perror("Unable to assign output file\n");
					exit(2);
				}

				// Close on exec
				fcntl(targetFD, F_SETFD, FD_CLOEXEC);
			}

			// Execution
			if(execvp(arr[0], (char* const*)arr)) {
				printf("%s: no such file or directory\n", arr[0]);
				fflush(stdout);
				exit(2);
			}
			break;

		default:
			// CITATION: https://repl.it/@cs344/42waitpidnohangc
			// Execute as background process if NOT in foreground mode and & is last char
			if(background_mode == 0 && *background == 1) {
				pid_t newPid = waitpid(spawnPid, exitStatus, WNOHANG);
				printf("backgound pid is %d\n", spawnPid);
				fflush(stdout);
			}
			// Execute normally
			else {
				pid_t newPid = waitpid(spawnPid, exitStatus, 0);
			}
		// Collect all children processes
		// Will return 0 when no more children
		while((spawnPid = waitpid(-1, exitStatus, WNOHANG)) > 0) {
			printf("child %d terminated\n", spawnPid);
			getExitStatus(*exitStatus);
			fflush(stdout);
		}
	}
}

// This function displays the exitStatus
// Parameters: int exitStatus
// Returns: nothing
// CITATION: 4_2_waitpid_exit.c
void getExitStatus(int exitStatus) {
		if(WIFEXITED(exitStatus)) {
			// If exited by status
			printf("exit value %d\n", WEXITSTATUS(exitStatus));
		}
		else {
			// If terminated by signal
			printf("terminated by signal %d\n", WTERMSIG(exitStatus));
		}
}

// This function handles SIGSTP, when it is called, the var background_mode changes accordingly to entering and exiting foreground mode
// Parameters: int signo
// Returns: nothing
void handle_SIGSTP(int signo) {
	
	// If background_mode is set to 0, Enter foreground-only mode
	if(background_mode == 0) {
		char* string = "Entering foreground-only mode (& is now ignored)\n";
		write(1, string, 49);
		fflush(stdout);
		background_mode = 1;
	}
	// If background_mode is set to 0, Exit foreground-only mode
	else if(background_mode == 1) {
		char* string = "Exiting foreground-only mode\n";
		write(1, string, 29);
		fflush(stdout);
		background_mode = 0;
	}	
}


int main() {
	int loop = 1; // Loop control, if loop == 0, break loop
	char *args[MAXARG]; // input limit is 512
	char inFile[MAXCHAR] = "", outFile[MAXCHAR] = "";
	int exitStatus = 0, background = 0;
	int pid = getpid(); // Get pid to pass to function
	int i = 0;
	
	// Initialize args array
	for(int i = 0; i < MAXARG; i++) {
		args[i] = NULL;
	}
	
	// Ignore CTRL C
	// CITATION: 5_3_signal_2.c
	struct sigaction SIGINT_action = {0};
	// Specify ignore signal
	SIGINT_action.sa_handler = SIG_IGN;
	sigfillset(&SIGINT_action.sa_mask); 
	SIGINT_action.sa_flags = 0;
	// Install signal handler
	sigaction(SIGINT, &SIGINT_action, NULL);

	// Allow ^Z to Enter and Exit forground mode
	struct sigaction sigstep = {0};
        sigstep.sa_handler = handle_SIGSTP;	
	sigfillset(&sigstep.sa_mask);
	sigstep.sa_flags = 0;
	sigaction(SIGTSTP, &sigstep, NULL);

	printf("$ smallsh\n");

	while(loop) {
		// Call getInput function which reads user input
		
		getUserInput(args, inFile, outFile, &background, pid);
		// If first char is # or \0 do nothing 
		if(args[0][0] == '#' || args[0][0] == '\0') {
			// Do nothing!
			continue;
		}

		// Determine if input is built in command
		// cd
		else if(strcmp(args[0], "cd") == 0) {
			// Check if something is there
			if(args[1]) {
				// If directory is not found
				if(chdir(args[1]) == -1) {
					printf("Directory not found\n");
				}
			}
			// Nothing is there
			else {
				// If directory is not listed, go to HOME directory (~)
				chdir(getenv("HOME"));
			}
		}
		// status
		else if(strcmp(args[0], "status") == 0) {
			getExitStatus(exitStatus);
		}
		// exit
		else if(strcmp(args[0], "exit") == 0) {
			// Set loop to false to exit loop
			loop = 0;			
		}
		// Anything other than built in commands
		else {
			execCommand(args, inFile, outFile, &background, &exitStatus, SIGINT_action);
		}
		
		// Clear input array
		while(args[i]) {
			args[i] = NULL;
			i++;
		}

		// Reset variables to read another line
		background = 0;
		inFile[0] = '\0';
		outFile[0] = '\0';
		i = 0;
	}

	return 0;
}
