/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _nsUnixColorPrintf_h_
#define _nsUnixColorPrintf_h_

#if defined(XP_UNIX) && defined(NS_DEBUG)

#define STARTGRAY       "\033[1;30m"
#define STARTRED        "\033[1;31m"
#define STARTGREEN      "\033[1;32m"
#define STARTYELLOW     "\033[1;33m"
#define STARTBLUE       "\033[1;34m"
#define STARTMAGENTA    "\033[1;35m"
#define STARTCYAN       "\033[1;36m"
#define STARTUNDERLINE  "\033[4m"
#define STARTREVERSE    "\033[7m"
#define ENDCOLOR        "\033[0m"

#define PRINTF_GRAY nsUnixColorPrintf __color_printf(STARTGREY)
#define PRINTF_RED  nsUnixColorPrintf __color_printf(STARTRED)
#define PRINTF_GREEN nsUnixColorPrintf __color_printf(STARTGREEN)
#define PRINTF_YELLOW nsUnixColorPrintf __color_printf(STARTYELLOW)
#define PRINTF_BLUE nsUnixColorPrintf __color_printf(STARTBLUE)
#define PRINTF_MAGENTA nsUnixColorPrintf __color_printf(STARTMAGENTA)
#define PRINTF_CYAN nsUnixColorPrintf __color_printf(STARTCYAN)
#define PRINTF_UNDERLINE nsUnixColorPrintf __color_printf(STARTUNDERLINE)
#define PRINTF_REVERSE nsUnixColorPrintf __color_printf(STARTREVERSE)

/* 
    The nsUnixColorPrintf is a handy set of color term codes to change
    the color of console texts for easiar spotting. As of now this is 
    Unix and Debug only. 

    Usage is simple. 
    
    See examples in 
        mozilla/netwerk/protocol/http/src/nsHTTPHandler.cpp

    -Gagan Saksena 11/01/99
*/

class nsUnixColorPrintf
{
public:
    nsUnixColorPrintf(const char* colorCode) 
    {
        printf("%s",colorCode);
    }
    ~nsUnixColorPrintf()
    {
        printf("%s",ENDCOLOR);
    }
};

#else // XP_UNIX

#define STARTGRAY       ""
#define STARTRED        ""
#define STARTGREEN      ""
#define STARTYELLOW     ""
#define STARTBLUE       ""
#define STARTMAGENTA    ""
#define STARTCYAN       ""
#define STARTUNDERLINE  ""
#define STARTREVERSE    ""
#define ENDCOLOR        ""

#define PRINTF_GRAY
#define PRINTF_RED
#define PRINTF_GREEN
#define PRINTF_YELLOW
#define PRINTF_BLUE
#define PRINTF_MAGENTA
#define PRINTF_CYAN
#define PRINTF_UNDERLINE
#define PRINTF_REVERSE

#endif // XP_UNIX
#endif // nsUnixColorPrintf

