%{
/* -*- Mode: C -*-
  ======================================================================
  FILE: icalsslexer.l
  CREATOR: eric 8 Aug 2000
  
  DESCRIPTION:
  
  $Id: icalsslexer.l,v 1.2 2001/12/21 18:56:39 mikep%oeone.com Exp $
  $Locker:  $

(C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

 The Original Code is eric. The Initial Developer of the Original
 Code is Eric Busboom

  ======================================================================*/

#include "icalssyacc.h"
#include "icalgaugeimpl.h"
#include "assert.h"

#include <string.h> /* For strdup() */

int icalparser_flex_input(char* buf, int max_size);
void icalparser_clear_flex_input();

#undef YY_INPUT
#define YY_INPUT(b,r,ms) ( r= icalparser_flex_input(b,ms))

#undef SS_FATAL_ERROR
#define SS_FATAL_ERROR(msg) sserror(msg)


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

%s sql string_value



%%

%{
%}


SELECT			{ return SELECT; }
FROM			{ return FROM; }
WHERE			{ return WHERE; }
,			{ return COMMA; }
"="  			{ return EQUALS; }
"=="  			{ return EQUALS; }
"!=" 			{ return NOTEQUALS; }
"<"  			{ return LESS; }
">"  			{ return GREATER; }
"<=" 			{ return LESSEQUALS; }
">=" 			{ return GREATEREQUALS; }
AND 			{ return AND; }
OR			{ return OR; }
\'                      { return QUOTE; }
[ \t\n\r]+ ;			
;			{ return EOL; }

\'[\*A-Za-z0-9\-\. ]+\' {
	int c = input();
	unput(c);
	if(c!='\''){
		sslval.v_string= icalmemory_tmp_copy(sstext);
		return STRING;
	} else {
		/*ssmore();*/
	}
}

[\*A-Za-z0-9\-\.]+  { 

        sslval.v_string= icalmemory_tmp_copy(sstext); 
        return STRING; 
}


.			{ return yytext[0]; }

%% 

int sswrap()
{
     return 1;
}

