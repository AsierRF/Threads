/* This file contains the code for parser used to parse the input
 * given to shell program. You shouldn't need to modify this 
 * file */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "parse.h"

#define PIPE  ('|')
#define BG    ('&')
#define RIN   ('<')
#define RUT   ('>')
#define IDCHARS "_-.,/~+"
#define ispipe(c) ((c) == PIPE)
#define isbg(c)   ((c) == BG)
#define isrin(c)  ((c) == RIN)
#define isrut(c)  ((c) == RUT)
#define isspec(c) (ispipe(c) || isbg(c) || isrin(c) || isrut(c))

static Pgm  cmdbuf[20], *cmds;
static char cbuf[256], *cp;
static char *pbuf[50], **pp;

/*Parses the commands given in the commandline and
 *saves them in Command structs starting in the given pointer
 *input: char * containing the pointer to the line given in the commandline,
 *       Command * containing the pointer to the first struct to save the parsed commands
 *output: int containing 1 if function worked properly; -1 if not
 */
int parse (char *buf, Command *c)
{
	int n;
	Pgm *cmd0;

	char *t = buf;
	char *tok;

	init();
	c->rstdin    = NULL;
	c->rstdout   = NULL;
	c->rstderr   = NULL;
	c->bakground = 0; /* false */
	c->pgm       = NULL;

	newcmd:
		if ((n = acmd(t, &cmd0)) <= 0) {
			return -1;
		}

		t += n;

		cmd0->next = c->pgm;
		c->pgm = cmd0;

	newtoken:
		n = nexttoken(t, &tok);
		if (n == 0) {
			return 1;
		}
		t += n;

	switch(*tok) 
	{
		case PIPE:
			goto newcmd;
			break;
		case BG:
			n = nexttoken(t, &tok);
			if (n == 0)
			{
				c->bakground = 1;
				return 1;
			}
			else
			{
				fprintf(stderr, "illegal bakgrounding\n");
				return -1;
			}
			break;
		case RIN:
			if (c->rstdin != NULL)
			{
				fprintf(stderr, "duplicate redirection of stdin\n");
				return -1;
			}
			if ((n = nexttoken(t, &(c->rstdin))) < 0) 
			{
				return -1;
			}
			if (!isidentifier(c->rstdin)) 
			{
				fprintf(stderr, "Illegal filename: \"%s\"\n", c->rstdin);
				return -1;
			}
			t += n;
			goto newtoken;
			break;
		case RUT:
			if (c->rstdout != NULL)
			{
				fprintf(stderr, "duplicate redirection of stdout\n");
				return -1;
			}
			if ((n = nexttoken(t, &(c->rstdout))) < 0)  
			{
				return -1;
			}
			if (!isidentifier(c->rstdout)) 
			{
				fprintf(stderr, "Illegal filename: \"%s\"\n", c->rstdout);
				return -1;
			}
			t += n;
			goto newtoken;
			break;
		default:
			return -1;
	}
	goto newcmd;
}
/*Initializes the variables
 *saves them in Command structs starting in the given pointer
 *input: void
 *output: void
 */
void init( void )
{
	int i;
	for (i=0;i<19;i++) {
		cmdbuf[i].next = &cmdbuf[i+1];
	}
	cmdbuf[19].next = NULL;
	cmds = cmdbuf;
	cp = cbuf;
	pp = pbuf;
}
/*Generates the next token value for the parse function
 *input: char * containing the string value of the command,
 *       char ** containing the pointer to the token
 *output: int containing the value of the new token
 */
int nexttoken( char *s, char **tok)
{
	char *s0 = s;
	char c;

	*tok = cp;
	while (isspace(c = *s++) && c);
	if (c == '\0') 
	{
		return 0;
	}
	if (isspec(c)) 
	{
		*cp++ = c;
		*cp++ = '\0';
	}
	else
	{
		*cp++ = c;
		do 
		{
			c = *cp++ = *s++;
		}
		while (!isspace(c) && !isspec(c) && (c != '\0'));
		--s;
		--cp;
		*cp++ = '\0';
	}
	return s - s0;
}
/*Saves the data of the command in the struct
 *input: char * containing the string value of the command,
 *       Pgm ** containing the pointer to the struct where the commands will be saved
 *output: int containing the value of the number of commands saved in the struct
 */
int acmd (char *s, Pgm **cmd)
{
	char *tok;
	int n, cnt = 0;
	Pgm *cmd0 = cmds;
	cmds = cmds->next;
	cmd0->next = NULL;
	cmd0->pgmlist = pp;

	next:
		n = nexttoken(s, &tok);
		if (n == 0 || isspec(*tok)) 
		{
			*cmd = cmd0;
			*pp++ = NULL;
			return cnt;
		}
		else
		{
			*pp++ = tok;
			cnt += n;
			s += n;
			goto next;
		}
}
/*Checkes whether the string containing the commands has a identifier to a input/output file
 *input: char * containing the string value of the command,
 *output: int containing 1 if true; 0 if not
 */
int isidentifier (char *s)
{
    while (*s) 
	{
		char *p = strrchr (IDCHARS,  *s);
		if (! isalnum(*s++) && (p == NULL))
			return 0;
    }
    return 1;
}
