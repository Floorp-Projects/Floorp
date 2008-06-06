/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
