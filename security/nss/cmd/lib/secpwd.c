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
#include "secutil.h"

/*
 * NOTE:  The contents of this file are NOT used by the client.
 * (They are part of the security library as a whole, but they are
 * NOT USED BY THE CLIENT.)  Do not change things on behalf of the
 * client (like localizing strings), or add things that are only
 * for the client (put them elsewhere).  
 */


#ifdef XP_UNIX
#include <termios.h>
#endif

#if defined(XP_UNIX) || defined(XP_BEOS)
#include <unistd.h>  /* for isatty() */
#endif

#if( defined(_WINDOWS) && !defined(_WIN32_WCE)) || defined(XP_OS2_VACPP)
#include <conio.h>
#include <io.h>
#define QUIET_FGETS quiet_fgets
static char * quiet_fgets (char *buf, int length, FILE *input);
#else
#define QUIET_FGETS fgets
#endif

static void echoOff(int fd)
{
#if defined(XP_UNIX) && !defined(VMS)
    if (isatty(fd)) {
	struct termios tio;
	tcgetattr(fd, &tio);
	tio.c_lflag &= ~ECHO;
	tcsetattr(fd, TCSAFLUSH, &tio);
    }
#endif
}

static void echoOn(int fd)
{
#if defined(XP_UNIX) && !defined(VMS)
    if (isatty(fd)) {
	struct termios tio;
	tcgetattr(fd, &tio);
	tio.c_lflag |= ECHO;
	tcsetattr(fd, TCSAFLUSH, &tio);
    }
#endif
}

char *SEC_GetPassword(FILE *input, FILE *output, char *prompt,
			       PRBool (*ok)(char *))
{
#if defined(_WINDOWS)
    int isTTY = (input == stdin);
#define echoOn(x)
#define echoOff(x)
#else
    int infd  = fileno(input);
    int isTTY = isatty(infd);
#endif
    char phrase[200];

    for (;;) {
	/* Prompt for password */
	if (isTTY) {
	    fprintf(output, "%s", prompt);
            fflush (output);
	    echoOff(infd);
	}

	QUIET_FGETS ( phrase, sizeof(phrase), input);

	if (isTTY) {
	    fprintf(output, "\n");
	    echoOn(infd);
	}

	/* stomp on newline */
	phrase[PORT_Strlen(phrase)-1] = 0;

	/* Validate password */
	if (!(*ok)(phrase)) {
	    /* Not weird enough */
	    if (!isTTY) return 0;
	    fprintf(output, "Password must be at least 8 characters long with one or more\n");
	    fprintf(output, "non-alphabetic characters\n");
	    continue;
	}
	return (char*) PORT_Strdup(phrase);
    }
}



PRBool SEC_CheckPassword(char *cp)
{
    int len;
    char *end;

    len = PORT_Strlen(cp);
    if (len < 8) {
	return PR_FALSE;
    }
    end = cp + len;
    while (cp < end) {
	unsigned char ch = *cp++;
	if (!((ch >= 'A') && (ch <= 'Z')) &&
	    !((ch >= 'a') && (ch <= 'z'))) {
	    /* pass phrase has at least one non alphabetic in it */
	    return PR_TRUE;
	}
    }
    return PR_FALSE;
}

PRBool SEC_BlindCheckPassword(char *cp)
{
    if (cp != NULL) {
	return PR_TRUE;
    }
    return PR_FALSE;
}

/* Get a password from the input terminal, without echoing */

#if defined(_WINDOWS) || defined(XP_OS2_VACPP)
static char * quiet_fgets (char *buf, int length, FILE *input)
  {
  int c;
  char *end = buf;

  /* fflush (input); */
  memset (buf, 0, length);

#ifndef XP_OS2_VACPP
  if (input != stdin) {
     return fgets(buf,length,input);
  }
#else
  if (!isatty(fileno(input))) {
     return fgets(buf,length,input);
  }
#endif

  while (1)
    {
#if defined (_WIN32_WCE)
    c = getchar();	/* gets a character from stdin */
#else
    c = getch();	/* getch gets a character from the console */
#endif
    if (c == '\b')
      {
      if (end > buf)
        end--;
      }

    else if (--length > 0)
      *end++ = c;

    if (!c || c == '\n' || c == '\r')
      break;
    }

  return buf;
  }
#endif
