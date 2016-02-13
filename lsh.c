#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "parse.h"
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/prctl.h>

struct ChildInfo {
	int childPid;
	struct ChildInfo * next;
};

void PrintCommand(int, Command *);
void PrintPgm(Pgm *);
void stripwhite(char *);
void ExecuteCommands(Pgm *);
void addChildToList(int);
void removeLastChild();
void killChild();
struct ChildInfo * getLastChild();
void generatePipes(Pgm * pgm, int first, int readPipe, int writePipe);
int isLast(Pgm*);
void signIntHandler(int signal);
void closePipes(int readPipe, int writePipe);

int done = 0;               /* When non-zero, this global means the user is done using this program. */
int outputFile = 0;         /* Will save the file descriptor for the output file, if there is any. */
int inputFile = 0;          /* Will save the file descriptor for the input file, if there is any. */
int saved_stdout;           /* Will save the default file descriptor for the output. */
int saved_stdin;            /* Will save the default file descriptor for the input. */
struct ChildInfo *childList;/* An array of structs to save the childs forked, in order to keep track of them. */
int background;             /* When 0, no background process, parent will wait for child; */
							/* When 1, background process, parent won't wait for child. */

/*Gets the commands introduced in the terminal, order them in a structure and call the
 *right function to execute the commands.
 *input: void
 *output: int containing the exit value
 */
int main(void)
{
	Command cmd;
	Pgm *nextPgm;
	int n;
	if(signal(SIGINT, signIntHandler) == SIG_ERR) printf("Error handling the SIGINT\n");
	if(signal(SIGCHLD, signIntHandler) == SIG_ERR) printf("Error handling the SIGCHLD\n");
	while (!done) 
	{
		char *line;
		line = readline("> ");

		if (!line) {
			/* Encountered EOF at top level */
			done = 1;
		}
		else 
		{
			/*
			* Remove leading and trailing whitespace from the line
			* Then, if there is anything left, add it to the history list
			* and execute it.
			*/
			stripwhite(line);

			if(*line) 
			{
				add_history(line);
				/* Execute it */
				n = parse(line, &cmd);
				background=cmd.bakground;
				/* In case there is a determined output file, open it and save the file descriptor */
				if (cmd.rstdout != NULL)
                {
					outputFile = open(cmd.rstdout, O_CREAT | O_WRONLY, 0777);
				}
				else
				{
					outputFile=0;
				}
				/* In case there is a determined input file, open it and save the file descriptor */
				if (cmd.rstdin != NULL)
                {
					inputFile = open(cmd.rstdin, O_RDONLY, 0777);
				}
				else
				{
					inputFile=0;
				}
				nextPgm = cmd.pgm;
				ExecuteCommands(nextPgm);
				PrintCommand(n, &cmd);
			}
		}
        if(line) 
        {
			free(line); 
        }    
    }
    return 0;
}

/*Prints a Command structure as returned by parse on stdout.
 *input: int containing the parse value, Command containing the pointer to the structure where
 *the commands are stored
 *output: void
 */
void PrintCommand (int n, Command *cmd)
{
  printf("Parse returned %d:\n", n);
  printf("   stdin : %s\n", cmd->rstdin  ? cmd->rstdin  : "<none>" );
  printf("   stdout: %s\n", cmd->rstdout ? cmd->rstdout : "<none>" );
  printf("   bg    : %s\n", cmd->bakground ? "yes" : "no");
  PrintPgm(cmd->pgm);
}
/*Prints a list of Pgm:s
 *input: Pgm containing the pointer to the first Pgm
 *output: void
 */
void PrintPgm (Pgm *p)
{
	if (p == NULL) {
		return;
	}
	else {
		char **pl = p->pgmlist;
		/* The list is in reversed order so print
		 * it reversed to get right
		 */
		PrintPgm(p->next);
		printf("    [");
		while (*pl) {
			printf("%s ", *pl++);
		}
		printf("]\n");
	}
}
/*Strip whitespace from the start and end of the given string.
 *input: char * containing the pointer to the string
 *output: void
 */
void stripwhite (char *string)
{
	register int i = 0;

	while (whitespace( string[i] )) {
		i++;
	}

	if (i) {
		strcpy (string, string + i);
	}

	i = strlen( string ) - 1;
	while (i> 0 && whitespace (string[i])) {
		i--;
	}

	string [++i] = '\0';
}
/*Checks what type of commands are and executes the commands and calls the right functions to
 *execute the commands
 *input: char * containing the pointer to first command
 *output: void
 */
void ExecuteCommands(Pgm *pgm)
{
	if(strcmp(pgm->pgmlist[0],"cd")==0) //If the command is cd
	{
		if(pgm->pgmlist[1]==NULL) //If no arguments for cd, go to home
		{
			chdir(getenv("HOME"));
		}
		else //If arguments for cd, go to that directory
		{
			chdir(pgm->pgmlist[1]);
		}
	}
	else if(strcmp(pgm->pgmlist[0],"exit")==0) //If the command is exit
	{
		exit(0);
	}
	else //If other type of commands
	{
		if(isLast(pgm))//If it is only one command
		{
			int pid=fork();
			if(pid==0)
			{
				if(background)
				{
					signal(SIGINT, SIG_IGN);
				}
				if(inputFile!=0)
				{
					close(STDIN_FILENO);
					dup(inputFile);
					close(inputFile);
				}
				if(outputFile!=0)
				{
					close(STDOUT_FILENO);
					dup(outputFile);
					close(outputFile);
				}
				execvp(pgm->pgmlist[0], pgm->pgmlist);
				exit(0);
			}
			else
			{
				if(!background)
				{
					waitpid(pid,NULL,0);
				}
				else
				{
					addChildToList(pid);
				}
			}
		}
		else //If there are piped commands
		{
			generatePipes(pgm, 1, -1, -1);
		}
	}	
}
/*Adds the id of the child process to the ChildList
 *input: int containing the id of the child process to be added
 *output: void
 */
void addChildToList(int id)
{
	struct ChildInfo * new = malloc (sizeof (struct ChildInfo));;
	struct ChildInfo * last=getLastChild();
    new->childPid=id;
	new->next=NULL;
	if(last==NULL) //If it is the first child to be added
	{
		childList=new;
	}
	else //If there is at least one child already in the list
	{
		last->next=new;
	}
}
/*Removes the last of the children processes ids from the ChildList
 *input: int containing the id of the child process to be removed
 *output: void
 */
void removeLastChild()
{
	struct ChildInfo *aux1, *aux2;
	aux1=childList;
	aux2=childList->next;
	if(aux2==NULL) //If it is only one child in the list
	{
		aux1=NULL;
	}
	else //If there are more than one child in the list
	{
		while(aux2->next!=NULL)
		{
			aux1=aux2;
			aux2=aux2->next;
		}
		free(aux2);
		aux1->next=NULL;
	}
}
/*Sends a kill signal to the last child in the list and waits for it to end
 *input: void
 *output: void
 */
void killChild()
{
	struct ChildInfo * last = getLastChild();
	if(last!=NULL) //If there is one child in the list
	{
		kill(last->childPid, SIGHUP);
		wait(last->childPid);
		removeLastChild();
	}
}
/*Returns the pointer to the last child in the child list if there is at least one child.
 *If no child in child list, returns NULL
 *input: void
 *output: ChildInfo * containing the pointer to the struct of the last child process on the list.
 *It may be NULL if there is no child process in the list
 */
struct ChildInfo * getLastChild()
{
	struct ChildInfo * ret = childList;
	if(ret==NULL)//If there is no child in the child list
	{
		return NULL;
	}
	while(ret->next!=NULL) ret=ret->next;
	return ret;
}
/*Generates the pipes and child processes needed to execute the commands and executes those commands
 *input: Pgm * containing the pointer to the next command,
 *       int containing 1 if first command; 0 if not first command, 
 *       int containing the file descriptor of the read end of the pipe
 *       int containing the file descriptor of the write end of the pipe
 *output: void
 */
void generatePipes(Pgm * pgm, int first, int readPipe, int writePipe)
{
	int pipes[2];
	if(first)//If it is the first command (The last to be executed)
	{
		pipe(pipes);
		generatePipes(pgm->next, 0, pipes[0], pipes[1]);
		int pid=fork();
		if(pid==0)
		{
			if(background)
			{
				signal(SIGINT, SIG_IGN);
			}
			close(STDIN_FILENO);
			dup(pipes[0]);
			if(outputFile!=0)
			{
				close(STDOUT_FILENO);
				dup(outputFile);
				close(outputFile);
			}
			closePipes(pipes[0],pipes[1]);
			execvp(pgm->pgmlist[0], pgm->pgmlist);
			exit(0);
		}
		else
		{
			closePipes(pipes[0],pipes[1]);
			if(!background)
			{
				waitpid(pid,NULL,0);
			}
			else
			{
				addChildToList(pid);
			}
		}
	}
	else if(isLast(pgm))//If it is the last command (The fisrt to be executed)
	{
		int pid=fork();
		if(pid==0)
		{
			if(background)
			{
				signal(SIGINT, SIG_IGN);
			}
			if(inputFile!=0)
			{	
				close(STDIN_FILENO);
				dup2(inputFile, STDIN_FILENO);
				close(inputFile);
			}
			close(STDOUT_FILENO);
			dup(writePipe);
			close(writePipe);
			execvp(pgm->pgmlist[0], pgm->pgmlist);
			exit(0);
		}
		else
		{
			close(writePipe);
			if(!background)
			{
				waitpid(pid,NULL,0);
			}
			else
			{
				addChildToList(pid);
			}
		}
	}
	else//If it is a command in the middle
	{
		pipe(pipes);
		generatePipes(pgm->next, 0, pipes[0], pipes[1]);
		int pid=fork();
		if(pid==0)
		{
			if(background)
			{
				signal(SIGINT, SIG_IGN);
			}
			close(STDIN_FILENO);
			dup(pipes[0]);
			close(STDOUT_FILENO);
			dup(writePipe);
			closePipes(pipes[0],pipes[1]);
			close(writePipe);
			execvp(pgm->pgmlist[0], pgm->pgmlist);
			exit(0);
		}
		else
		{
			closePipes(pipes[0],pipes[1]);
			close(writePipe);
			if(!background)
			{
				waitpid(pid,NULL,0);
			}
			else
			{
				addChildToList(pid);
			}
		}
	}
}
/*Checks whether the command is the last or not, returns 1 if yes and 0 if no
 *input: Pgm * containing the pointer to the command
 *output: int containing 1 if true; 0 if false
 */
int isLast(Pgm * var)
{
	if(var->next==NULL)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}
/*Handles the signals received
 *input: int containing the descriptor value for the signal to be handled
 *output: void
 */
void signIntHandler(int signal)
{
	if(signal==SIGINT){
		killChild();
	}
	else if(signal==SIGCHLD)
	{
		wait();
	}
}
/*Closes two received pipe's ends
 *input: int containing the file descriptor to the read end of the pipe,
 *       int containing the file descriptor to the write end of the pipe,
 *output: void
 */
void closePipes(int readPipe, int writePipe)
{
	close(readPipe);
	close(writePipe);
}