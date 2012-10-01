/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/* lex file for analyzing PKCS #11 Module installation instructions */

/*----------------------------- Definitions ---------------------------*/
%{
#include <string.h>

#include "install-ds.h"		/* defines tokens and data structures */
#include "installparse.h"	/* produced by yacc -d */
#include <prprf.h>
static char *putSimpleString(char*);	/* return copy of string */
static char *putComplexString(char*);	/* strip out quotes, deal with */
											/* escaped characters */

void Pk11Install_yyerror(char *);

/* Overrides to use NSPR */
#define malloc PR_Malloc
#define realloc PR_Realloc
#define free PR_Free

int Pk11Install_yylinenum=1;
static char *err;

#define YY_NEVER_INTERACTIVE 1
#define yyunput Pkcs11Install_yyunput

/* This is the default YY_INPUT modified for NSPR */
#define YY_INPUT(buf,result,max_size) \
	if ( yy_current_buffer->yy_is_interactive ) { \
		char c; \
		int n; \
		for ( n = 0; n < max_size && \
		  PR_Read(Pk11Install_FD, &c, 1)==1 && c != '\n'; ++n ) { \
			buf[n] = c; \
		} \
        if ( c == '\n' ) { \
            buf[n++] = c; \
		} \
        result = n; \
	} else { \
		result = PR_Read(Pk11Install_FD, buf, max_size); \
	}

%}

/*** Regular expression definitions ***/
/* simple_string has no whitespace, quotes, or braces */
simple_string		[^ \t\r\n\""{""}"]+

/* complex_string is enclosed in quotes. Inside the quotes, quotes and
   backslashes must be backslash-escaped. No newlines or carriage returns
   are allowed inside the quotes. Otherwise, anything goes. */
complex_string		\"([^\"\\\r\n]|(\\\")|(\\\\))+\"

/* Standard whitespace */
whitespace			[ \t\r]+

other				.

/*---------------------------- Actions --------------------------------*/
%%

"{"					return OPENBRACE;
"}"					return CLOSEBRACE;
{simple_string}		{Pk11Install_yylval.string =
						putSimpleString(Pk11Install_yytext);
						return STRING;}
{complex_string}	{Pk11Install_yylval.string =
						putComplexString(Pk11Install_yytext);
						return STRING;}

"\n"				Pk11Install_yylinenum++;

{whitespace}		;

{other}				{err = PR_smprintf("Invalid lexeme: %s",Pk11Install_yytext);
						Pk11Install_yyerror(err);
						PR_smprintf_free(err);
						return 1;
					}

%%
/*------------------------ Program Section ----------------------------*/

PRFileDesc *Pk11Install_FD=NULL;

/*************************************************************************/
/* dummy function required by lex */
int Pk11Install_yywrap(void) { return 1;}

/*************************************************************************/
/* Return a copy of the given string */
static char*
putSimpleString(char *str)
{
	char *tmp = (char*) PR_Malloc(strlen(str)+1);
	strcpy(tmp, str);
	return tmp;
}

/*************************************************************************/
/* Strip out quotes, replace escaped characters with what they stand for.
   This function assumes that what is passed in is actually a complex
   string, so error checking is lax. */
static char*
putComplexString(char *str)
{
	int size, i,j;
	char *tmp;

	if(!str) {
		return NULL;
	}
	size = strlen(str);

	/* Allocate the new space.  This string will actually be too big, 
		since quotes and backslashes will be stripped out.  But that's ok. */
	tmp = (char*) PR_Malloc(size+1);

	/* Copy it over */
	for(i=0, j=0; i < size; i++) {
		if(str[i]=='\"') {
			continue;  /* skip un-escaped quotes */
		} else if(str[i]=='\\') {
			++i;       /* escaped character. skip the backslash */
		}
		tmp[j++] = str[i];
	}
	tmp[j] = '\0';

	return tmp;
}
