/***************************************************************************

    lexer.l (IDL lex scanner)

    Copyright (C) 1998, 1999 Andrew T. Veliath

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id: lexer.l,v 1.1 1999/05/06 15:03:49 beard%netscape.com Exp $

***************************************************************************/
%{
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "rename.h"
#include "util.h"
#include "parser.h"

#define YY_INPUT(buf,result,the_max_size)	do {				\
	if (__IDL_inputcb == NULL) {						\
		if ((result = fread (buf, 1, the_max_size, yyin)) == YY_NULL &&	\
		    ferror (yyin))						\
			YY_FATAL_ERROR ("input in scanner failed");		\
	} else {								\
		union IDL_input_data data;					\
										\
		data.fill.buffer = buf;						\
		data.fill.max_size = the_max_size;				\
		result = (*__IDL_inputcb) (IDL_INPUT_REASON_FILL, &data,	\
					   __IDL_inputcb_user_data);		\
		if (result < 0)							\
			YY_FATAL_ERROR ("input callback returned failure");	\
	}									\
} while (0)

#define tokreturn(token)			do {	\
	__IDL_prev_token_line = __IDL_cur_token_line;	\
	__IDL_cur_token_line = __IDL_cur_line;		\
	return token;					\
} while (0)

#define SELECT_START				\
	/* Parser driven start conditions */	\
	if (__IDL_flagsi & IDLFP_PROPERTIES)	\
		BEGIN (PROP);			\
	else if (__IDL_flagsi & IDLFP_NATIVE)	\
		BEGIN (NATIVE);			\
	/* Global syntax start conditions */	\
	else if (__IDL_flags & IDLF_XPIDL)	\
		BEGIN (XP);			\
	else if (__IDL_flags & IDLF_CODEFRAGS)	\
		BEGIN (CFRG);

#define SELECT_RESTART				\
	SELECT_START				\
	else					\
		BEGIN (INITIAL);

extern void		__IDL_do_pragma			(const char *s);
static int		count_nl			(const char *s);

#ifdef YYDEBUG
extern int				yydebug;
#endif
int					__IDL_prev_token_line;
int					__IDL_cur_token_line;
static int				warn_underscores;
static char *				codefrag_desc;
static GSList *				codefrag_list;
static GSList *				codefrag_list_tail;
%}

whitespace		[ \t\v\f\r]*
whitespacenl		[ \t\v\f\r\n]*
newline			\n
cpp_pragma		^{whitespace}#{whitespace}pragma{whitespace}.*
cpp_status		^{whitespace}#.*
b8_int			0[0-9]*
b10_int			-[1-9][0-9]*
b10_uint		[1-9][0-9]*
b16_int			0[xX][0-9A-Fa-f]+
float_lit		-?[0-9]*\.[0-9]+([eE]-?[0-9]+)?|-?[0-9]+\.?([eE]-?[0-9]+)?
fixed_lit		(-?[0-9]*\.[0-9]+|-?[0-9]+\.?[0-9]*)[dD]
declspec		__declspec{whitespacenl}\({whitespacenl}[A-Za-z]*{whitespacenl}\)
happy_ident		[A-Za-z][A-Za-z0-9]*
err1_ident		_[A-Za-z0-9_]+
warn1_ident		[A-Za-z][A-Za-z0-9_]*
prop_key		[A-Za-z][A-Za-z0-9_]*
prop_value		\([^\)]+\)
native_type		[^\)]+\)
sqstring		\'[^\'\n]*[\'\n]
dqstring		\"[^\"\n]*[\"\n]

%p 5000

%s XP

%x PROP
%x NATIVE

%s CFRG
%x CFRGX

%%

	SELECT_START;

<INITIAL,XP,CFRG>^%\{.*					{
	char *s = yytext + 2;

	while (isspace (*s)) ++s;
	codefrag_desc = g_strdup (s);
	codefrag_list = codefrag_list_tail = NULL;

	if (!(__IDL_flags & IDLF_XPIDL || __IDL_flags & IDLF_CODEFRAGS))
		yyerror ("Code fragment syntax not enabled");
	else
		BEGIN (CFRGX);
}
<CFRGX>^%\}.*						{
	yylval.tree = IDL_codefrag_new (codefrag_desc, codefrag_list);
	tokreturn (TOK_CODEFRAG);
}
<CFRGX>.*						{
	char *s;
	GSList *slist;

	s = g_strdup (yytext);
	slist = g_slist_alloc ();
	slist->data = s;

	if (codefrag_list == NULL) {
		codefrag_list = slist;
		codefrag_list_tail = slist;
	} else {
		codefrag_list_tail->next = slist;
		codefrag_list_tail = slist;
	}
}
<INITIAL,XP,PROP,NATIVE,CFRG,CFRGX>{cpp_pragma}		{
	int n;
	char *p = yytext;
	char *s;

	while (isspace (*p) || *p == '#') ++p;
	s = p;
	sscanf (p, "%*6s%n", &n); s += n;
	while (isspace (*s)) ++s;

	__IDL_do_pragma (s);
}
<INITIAL,XP,PROP,NATIVE,CFRG,CFRGX>{cpp_status}		{
	char *starttext;
	char *filename;
	char *filename2;
	int line;

	starttext = yytext;
	while (isspace (*starttext)) ++starttext;
	filename = g_malloc (strlen (starttext) + 1);

	sscanf (starttext, "# %d %s", &line, filename);

	if (*filename == '"') {
		filename2 = g_strdup (filename + 1);
		filename2[strlen (filename) - 2] = 0;
		g_free (filename);
	} else
		filename2 = filename;

	IDL_file_set (filename2, line);

	g_free (filename2);
}
<INITIAL,XP,PROP,NATIVE,CFRG,CFRGX>{whitespace}
{b8_int}						{
	sscanf (yytext, "%" IDL_LL "o", &yylval.integer);
	tokreturn (TOK_INTEGER);
}
{b10_int}						{
	sscanf (yytext, "%" IDL_LL "d", &yylval.integer);
	tokreturn (TOK_INTEGER);
}
{b10_uint}						{
	sscanf (yytext, "%" IDL_LL "u", &yylval.integer);
	tokreturn (TOK_INTEGER);
}
{b16_int}						{
	sscanf (yytext + 2, "%" IDL_LL "x", &yylval.integer);
	tokreturn (TOK_INTEGER);
}
{fixed_lit}						{
	yylval.str = g_strdup (yytext);
	tokreturn (TOK_FIXEDP);
}
{float_lit}						{
	yylval.floatp = atof (yytext);
	tokreturn (TOK_FLOATP);
}
FALSE			tokreturn (TOK_FALSE);
Object			tokreturn (TOK_OBJECT);
TRUE			tokreturn (TOK_TRUE);
any			tokreturn (TOK_ANY);
attribute		tokreturn (TOK_ATTRIBUTE);
boolean			tokreturn (TOK_BOOLEAN);
case			tokreturn (TOK_CASE);
char			tokreturn (TOK_CHAR);
const			tokreturn (TOK_CONST);
context			tokreturn (TOK_CONTEXT);
default			tokreturn (TOK_DEFAULT);
double			tokreturn (TOK_DOUBLE);
enum			tokreturn (TOK_ENUM);
exception		tokreturn (TOK_EXCEPTION);
fixed			tokreturn (TOK_FIXED);
float			tokreturn (TOK_FLOAT);
in			tokreturn (TOK_IN);
inout			tokreturn (TOK_INOUT);
interface		tokreturn (TOK_INTERFACE);
long			tokreturn (TOK_LONG);
module			tokreturn (TOK_MODULE);
native			tokreturn (TOK_NATIVE);
<XP>noscript		tokreturn (TOK_NOSCRIPT);
octet			tokreturn (TOK_OCTET);
oneway			tokreturn (TOK_ONEWAY);
out			tokreturn (TOK_OUT);
raises			tokreturn (TOK_RAISES);
readonly		tokreturn (TOK_READONLY);
sequence		tokreturn (TOK_SEQUENCE);
short			tokreturn (TOK_SHORT);
string			tokreturn (TOK_STRING);
struct			tokreturn (TOK_STRUCT);
switch			tokreturn (TOK_SWITCH);
typedef			tokreturn (TOK_TYPEDEF);
union			tokreturn (TOK_UNION);
unsigned		tokreturn (TOK_UNSIGNED);
<XP>\.\.\.		tokreturn (TOK_VARARGS);
void			tokreturn (TOK_VOID);
wchar			tokreturn (TOK_WCHAR);
wstring			tokreturn (TOK_WSTRING);
::			tokreturn (TOK_OP_SCOPE);
\>\>			tokreturn (TOK_OP_SHR);
\<\<			tokreturn (TOK_OP_SHL);
{declspec}						{
	char *s = g_strdup (yytext);

	/* Get the parenthesized expression (ignoring whitespace) */
	sscanf (yytext, "__declspec %*[(] %[A-Za-z_] %*[)]", s);
	yylval.str = s;
	__IDL_cur_line += count_nl (yytext);
	tokreturn (TOK_DECLSPEC);
}
{happy_ident}						{
	if (__IDL_flags & IDLF_TYPECODES && strcmp (yytext, "TypeCode") == 0)
		tokreturn (TOK_TYPECODE);
	yylval.str = g_strdup (yytext);
	tokreturn (TOK_IDENT);
}
{err1_ident}						{
	yyerrorv ("`%s' is not a valid identifier in IDL", yytext);
	yyerror ("(Identifiers cannot start with an underscore)");
	yylval.str = g_strdup (yytext);
	tokreturn (TOK_IDENT);
}
{warn1_ident}						{
	if (!warn_underscores) {
		yywarningv (IDL_WARNING2,
			   "`%s' underscores within identifiers are discouraged for use "
			   "with C-language IDL mappings", yytext);
		warn_underscores = 1;
	}
	yylval.str = g_strdup (yytext);
	tokreturn (TOK_IDENT);
}
<PROP>]							{
	__IDL_flagsi &= ~IDLFP_PROPERTIES;
	SELECT_RESTART;
	tokreturn (yytext[0]);
}
<PROP>{prop_key}					{
	yylval.str = g_strdup (yytext);
	tokreturn (TOK_PROP_KEY);
}
<PROP>{prop_value}					{
	yylval.str = g_strdup (yytext + 1);
	yylval.str[strlen (yylval.str) - 1] = 0;
	tokreturn (TOK_PROP_VALUE);
}
<NATIVE>{native_type}					{
	__IDL_flagsi &= ~IDLFP_NATIVE;
	yylval.str = g_strdup (yytext);
	yylval.str[strlen (yylval.str) - 1] = 0;
	tokreturn (TOK_NATIVE_TYPE);
}
{sqstring}						{
	yylval.str = g_strdup (yytext + 1);
	yylval.str[strlen (yytext) - 2] = 0;
	tokreturn (TOK_SQSTRING);
}
{dqstring}						{
	yylval.str = g_strdup (yytext + 1);
	yylval.str[strlen (yytext) - 2] = 0;
	tokreturn (TOK_DQSTRING);
}
<INITIAL,XP,PROP,NATIVE,CFRG,CFRGX>{newline}		++__IDL_cur_line;
<INITIAL,XP,PROP,NATIVE,CFRG,CFRGX>\/\/.*
<INITIAL,XP,PROP,NATIVE,CFRG,CFRGX>.			tokreturn (yytext[0]);

%%

void __IDL_lex_init (void)
{
	__IDL_inputcb = NULL;
	__IDL_cur_line = 1;
	__IDL_cur_token_line = 0;
	__IDL_prev_token_line = 0;
	__IDL_cur_filename = NULL;
	__IDL_cur_fileinfo = NULL;
	warn_underscores = 0;
}

void __IDL_lex_cleanup (void)
{
	__IDL_cur_filename = NULL;
}

int yywrap (void)
{
	return 1;
}

static int count_nl (const char *s)
{
	int i;

	for (i = 0;
	     (s = strchr (s, '\n')) != NULL;
	     ++s, ++i) ;

	return i;
}

/*
 * Local variables:
 * mode: C
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: t
 * End:
 */
