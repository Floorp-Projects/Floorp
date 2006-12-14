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
 * The Original Code is the Netscape svrcore library.
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * user.c - SVRCORE module for reading PIN from the terminal
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <svrcore.h>
#ifdef _WIN32
#include <windows.h>
#endif /* _WIN32 */

/* ------------------------------------------------------------ */
/* I18N */
static const char retryWarning[] =
"Warning: Incorrect PIN may result in disabling the token";
static const char prompt[] = "Enter PIN for";
static const char nt_retryWarning[] =
"Warning: You entered an incorrect PIN. Incorrect PIN may result in disabling the token";

struct SVRCOREUserPinObj
{
  SVRCOREPinObj base;
  PRBool interactive;
};
static const struct SVRCOREPinMethods vtable;

#ifdef _WIN32
extern char* NT_PromptForPin(const char *tokenName);
#else
/* ------------------------------------------------------------ */
/* 
 * Support routines for changing terminal modes on UNIX
 */
#include <termios.h>
#include <unistd.h>
static void echoOff(int fd)
{
    if (isatty(fd)) {
        struct termios tio;
        tcgetattr(fd, &tio);
        tio.c_lflag &= ~ECHO;
        tcsetattr(fd, TCSAFLUSH, &tio);
    }
}
 
static void echoOn(int fd)
{
    if (isatty(fd)) {
        struct termios tio;
        tcgetattr(fd, &tio);
        tio.c_lflag |= ECHO;
        tcsetattr(fd, TCSAFLUSH, &tio);
    }
}
#endif /* _WIN32 */

/* ------------------------------------------------------------ */
SVRCOREError
SVRCORE_CreateUserPinObj(SVRCOREUserPinObj **out)
{
  SVRCOREError err = 0;
  SVRCOREUserPinObj *obj = 0;

  do {
    obj = (SVRCOREUserPinObj*)malloc(sizeof (SVRCOREUserPinObj));
    if (!obj) { err = 1; break; }

    obj->base.methods = &vtable;

    obj->interactive = PR_TRUE;
  } while(0);

  if (err)
  {
    SVRCORE_DestroyUserPinObj(obj);
    obj = 0;
  }

  *out = obj;
  return err;
}

void
SVRCORE_DestroyUserPinObj(SVRCOREUserPinObj *obj)
{
  if (obj) free(obj);
}

void
SVRCORE_SetUserPinInteractive(SVRCOREUserPinObj *obj, PRBool i)
{
  obj->interactive = i;
}

static void destroyObject(SVRCOREPinObj *obj)
{
  SVRCORE_DestroyUserPinObj((SVRCOREUserPinObj*)obj);
}


static char *getPin(SVRCOREPinObj *obj, const char *tokenName, PRBool retry)
{
  SVRCOREUserPinObj *tty = (SVRCOREUserPinObj*)obj;
  char line[128];
  char *res;

  /* If the program is not interactive then return no result */
  if (!tty->interactive) return 0;

#ifdef _WIN32 
  if (retry) {
        MessageBox(GetDesktopWindow(), nt_retryWarning,
                        "Netscape Server", MB_ICONEXCLAMATION | MB_OK);
  }
  return NT_PromptForPin(tokenName);
#else

  if (retry)
    fprintf(stdout, "%s\n", retryWarning);

  echoOff(fileno(stdin));

/***
  Please Note: the following printf statement was changed from fprintf(stdout,...) because
  of an odd problem with the Linux build. The issue is that libc.so has a symbol for stdout
  and libstdc++.so which we also reference has a symbol for stdout. Normally the libc.so version
  of stdout is resolved first and writing to stdout is no problem. Unfortunately something happens
  on Linux which allows the "other" stdout from libstdc++.so to get referenced so that when a call
  to fprintf(stdout,...) is made the new stdout which has never been initialized get's written
  to causing a sigsegv. At this point we can not easily remove libstdc++.so from the dependencies
  because other code which slapd uses happens to be C++ code which causes the reference of
  libstdc++.so .

  It was determined that the quickest way to resolve the issue for now was to change the fprintf
  calls to printf thereby fixing the crashes on a temp basis. Using printf seems to work because
  it references stdout internally which means it will use the one from libc.so . 
***/
  printf("%s %s: ", prompt, tokenName);
  fflush(stdout);

  /* Read input */
  res = fgets(line, sizeof line, stdin);

  echoOn(fileno(stdin));
  printf("\n");

  if (!res) return 0;

  /* Find and kill the newline */
  if ((res = strchr(line, '\n')) != NULL) *res = 0;

  /* Return no-response if user typed an empty line */
  if (line[0] == 0) return 0;

  return strdup(line);

#endif /* _WIN32 */

}

/*
 * VTable
 */
static const SVRCOREPinMethods vtable =
{ 0, 0, destroyObject, getPin };
