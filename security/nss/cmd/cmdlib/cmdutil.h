/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#ifndef _CMDUTIL_H_
#define _CMDUTIL_H_

#include <stdio.h>
#include "nspr.h"
#include "nssbase.h"

typedef int 
(* CMD_PPFunc)(PRFileDesc *out, NSSItem *item, char *msg, int level);


/*
 * Command Line Parsing routines
 *
 * The attempt here is to provide common functionality for command line
 * parsing across an array of tools.  The tools should obey the historical
 * rules of:
 *
 * (1) one command per line,
 * (2) the command should be uppercase,
 * (3) options should be lowercase,
 * (4) a short usage statement is presented in case of error,
 * (5) a long usage statement is given by -? or --help
 */

/* To aid in formatting usage output.  XXX Uh, why exposed? */
typedef struct cmdPrintStateStr cmdPrintState;

typedef enum {
    CMDArgReq = 0,
    CMDArgOpt,
    CMDNoArg
} CMDArg;

struct cmdCommandLineArgStr {
    char     c;      /* one-character alias for flag    */
    char    *s;      /* string alias for flag           */
    CMDArg   argUse; /* flag takes an argument          */
    char    *arg;    /* argument given for flag         */
    PRBool   on;     /* flag was issued at command-line */
    int      req;    /* required arguments for commands */
    int      opt;    /* optional arguments for commands */
};

struct cmdCommandLineOptStr {
    char     c;      /* one-character alias for flag    */
    char    *s;      /* string alias for flag           */
    CMDArg   argUse; /* flag takes an argument          */
    char    *arg;    /* argument given for flag         */
    PRBool   on;     /* flag was issued at command-line */
};

typedef struct cmdCommandLineArgStr cmdCommandLineArg;
typedef struct cmdCommandLineOptStr cmdCommandLineOpt;

struct cmdCommandStr {
    int ncmd;
    int nopt;
    cmdCommandLineArg *cmd;
    cmdCommandLineOpt *opt;
};

typedef struct cmdCommandStr cmdCommand;

int 
CMD_ParseCommandLine(int argc, char **argv, char *progName, cmdCommand *cmd);

typedef void 
(* cmdUsageCallback)(cmdPrintState *, int, PRBool, PRBool, PRBool);

#define CMDBIT(n) (1<<n)

void 
CMD_Usage(char *progName, cmdCommand *cmd);

void 
CMD_LongUsage(char *progName, cmdCommand *cmd, cmdUsageCallback use);

void 
CMD_PrintUsageString(cmdPrintState *ps, char *str);

#endif /* _CMDUTIL_H_ */
