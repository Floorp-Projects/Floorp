/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    the color of console texts for easier spotting. As of now this is 
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

