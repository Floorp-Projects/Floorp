%{
/* -*- Mode: C -*-
  ======================================================================
  FILE: icallexer.l
  CREATOR: eric 10 June 1999
  
  DESCRIPTION:
  
  $Id: icallexer.l,v 1.2 2001/12/21 18:56:22 mikep%oeone.com Exp $
  $Locker:  $

  (C) COPYRIGHT 1999 Eric Busboom 
  http://www.softwarestudio.org

  The contents of this file are subject to the Mozilla Public License
  Version 1.0 (the "License"); you may not use this file except in
  compliance with the License. You may obtain a copy of the License at
  http://www.mozilla.org/MPL/
 
  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
  the License for the specific language governing rights and
  limitations under the License.

  The original author is Eric Busboom
  The original code is icalitip.y



  ======================================================================*/
#include "icalparser.h"
#include "icalenums.h"
#include "icalmemory.h"
#include "assert.h"
#include "icalyacc.h"

#include <string.h> /* For strdup() */

int icalparser_flex_input(char* buf, int max_size);
void icalparser_clear_flex_input(void);


#define ICAL_MAX_STR_CONST 1024

#undef YY_INPUT
#define YY_INPUT(b,r,ms) ( r= icalparser_flex_input(b,ms))
#undef yywrap

#undef YY_FATAL_ERROR
#define YY_FATAL_ERROR(msg) ical_yyerror(msg)

icalvalue_kind value_kind=ICAL_NO_VALUE;
void set_parser_value_state(icalvalue_kind kind);
extern int yydebug; 

void ical_yyerror(char *s);

void init_str_buf(void);

int last_state;

char *str_buf;
char *str_buf_p;
size_t buf_sz; /* = ICAL_MAX_STR_CONST;*/

%}

crlf		\x0D?\x0A
space		[ ]
qsafechar	[^\x00-\x1F\"]
safechar	[^\x00-\x1F\"\:\;\,]
tsafechar	[\x20-\x21\x23-\x2B\x2D-\x39\x3C-\x5B\x5D-\x7E]
valuechar	[^\x00-\x08\x10-\x1F]
xname		X-[a-zA-Z0-9\-]+
xname2          [a-zA-Z0-9\-\ ]
paramtext	{safechar}+
value		{valuechar}+
quotedstring	\"{qsafechar}+\"
digit		[0-9]

%array /* Make yytext an array. Slow, but handy. HACK */

%option caseless

%s quoted_string
%s binary_value boolean_value uri_value time_value duration_value number_value period_value recur_value text_value utcoffset_value
%s enum_param_value string_param_value stringlist_param_value keyword line_start component seperator parameter end_of_value paramtext



%%

%{
%}



<time_value>{
{digit}+		 { ical_yylval.v_string =icalmemory_tmp_copy(yytext) ;
                           return DIGITS; }
T			 { return TIME_CHAR; }
Z			 { return UTC_CHAR; }
[\/\+\-PWHMSD]		 { return yytext[0]; }
{crlf}                   { return EOL;}

}

<utcoffset_value>{
{crlf}                   { return EOL;}
\-|\+			 { return yytext[0]; }
{digit}{digit}		 { ical_yylval.v_int=atoi(yytext); return INTNUMBER; }

}

<enum_param_value>{
.			 { return CHARACTER; }
{crlf}                   { return EOL;}

}

<seperator>{
,       { BEGIN(last_state); return COMMA; } 
}


%% 

int yywrap()
{
     return 1;
}


void set_parser_value_state(icalvalue_kind kind)
{

    switch (kind){

	case ICAL_UTCOFFSET_VALUE:
	    {BEGIN(utcoffset_value);break;}

	case ICAL_DATETIMEPERIOD_VALUE:
	case ICAL_DURATION_VALUE:
	case ICAL_PERIOD_VALUE:
	    {BEGIN(time_value);break;}

	default:
	{
	   assert(1==0);
	}
    }
}

void init_str_buf(void)
{
   str_buf = icalmemory_tmp_buffer(ICAL_MAX_STR_CONST);
   str_buf_p = str_buf;
   buf_sz = ICAL_MAX_STR_CONST;


}

