/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* yacc file for parsing PKCS #11 module installation instructions */
/*------------------------ Definition Section ---------------------------*/

%{ 
#define yyparse Pk11Install_yyparse
#define yylex Pk11Install_yylex
#define yyerror Pk11Install_yyerror
#define yychar Pk11Install_yychar
#define yyval Pk11Install_yyval
#define yylval Pk11Install_yylval
#define yydebug Pk11Install_yydebug
#define yynerrs Pk11Install_yynerrs
#define yyerrflag Pk11Install_yyerrflag
#define yyss Pk11Install_yyss
#define yyssp Pk11Install_yyssp
#define yyvs Pk11Install_yyvs
#define yyvsp Pk11Install_yyvsp
#define yylhs Pk11Install_yylhs
#define yylen Pk11Install_yylen
#define yydefred Pk11Install_yydefred
#define yydgoto Pk11Install_yydgoto
#define yysindex Pk11Install_yysindex
#define yyrindex Pk11Install_yyrindex
#define yygindex Pk11Install_yygindex
#define yytable Pk11Install_yytable
#define yycheck Pk11Install_yycheck
#define yyname Pk11Install_yyname
#define yyrule Pk11Install_yyrule

/* C Stuff */
#include "install-ds.h"
#include <prprf.h>

#define YYSTYPE Pk11Install_Pointer
extern char *Pk11Install_yytext;
char *Pk11Install_yyerrstr=NULL;

%}

/* Tokens */
%token OPENBRACE
%token CLOSEBRACE
%token STRING
%start toplist

%%

/*--------------------------- Productions -------------------------------*/

toplist		: valuelist	
{
	Pk11Install_valueList = $1.list;
}

valuelist	: value valuelist
{ 
	Pk11Install_ValueList_AddItem($2.list,$1.value);
	$$.list = $2.list; 
}
|
{ 
	$$.list = Pk11Install_ValueList_new(); 
};

value		: key_value_pair
{
	$$.value= Pk11Install_Value_new(PAIR_VALUE,$1);
}
| STRING
{
	$$.value= Pk11Install_Value_new(STRING_VALUE, $1);
};

key_value_pair	: key OPENBRACE valuelist CLOSEBRACE 
{
	$$.pair = Pk11Install_Pair_new($1.string,$3.list);
};

key			: STRING
{
	$$.string = $1.string;
};

%%
/*----------------------- Program Section --------------------------------*/

/*************************************************************************/
void
Pk11Install_yyerror(char *message)
{
	char *tmp;
	if(Pk11Install_yyerrstr) {
		tmp=PR_smprintf("%sline %d: %s\n", Pk11Install_yyerrstr,
			Pk11Install_yylinenum, message);
		PR_smprintf_free(Pk11Install_yyerrstr);
	} else {
		tmp = PR_smprintf("line %d: %s\n", Pk11Install_yylinenum, message);
	}
	Pk11Install_yyerrstr=tmp;
}
