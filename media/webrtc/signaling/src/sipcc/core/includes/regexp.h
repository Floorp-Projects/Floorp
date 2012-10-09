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
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
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

#ifndef __REGEXP_H__
#define __REGEXP_H__

/*
 * Definitions etc. for regexp(3) routines.
 *
 * Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
 * not the System V one.
 */
#define NSUBEXP  10
struct regexp_ {
    char *startp[NSUBEXP];
    char *endp[NSUBEXP];
    char regflag;       /* Flags used for pattern matching */
    char regstart;      /* Internal use only. */
    char reganch;       /* Internal use only. */
    char regatom;       /* Internal use only. */
    char *regmust;      /* Internal use only. */
    int regmlen;        /* Internal use only. */
    char program[1];    /* Unwarranted chumminess with compiler. */
};

typedef struct regexp_ regexp;


/*
 * The first byte of the regexp internal "program" is actually this magic
 * number; the start node begins in the second byte.
 */
#define MAGIC    0234

/*
 * The parameters for regflag
 */
#define REG_NOSUB 0x02   /* Don't do sub-string matches */

/* Return Values from regexecstring()
 */
typedef enum {
    REG_NO_MATCH = 0,
    REG_MATCHED,
    REG_MAYBE
} regval;


/***********************************************************************
 *
 *                      Externs and Prototypes
 *
 ***********************************************************************/

/*
 * regexp.c
 */
extern char *reg(int, int *);
extern char *regatom(int *);
extern char *regbranch(int *);
extern char *regnode(char);
extern char *regpiece(int *);
extern char *regprop(char *);
extern int regexec(regexp *, char *);
extern int regmatch(char *);
extern int regrepeat(char *);
extern int regtry(regexp *, char *);
extern regexp *regcomp(char *, char);
extern void regc(char);
extern void regdump(regexp *);
extern void regerror(char *);
extern void reginsert(char, char *);
extern void regoptail(char *, char *);
extern void regtail(char *, char *);
extern regval regexecstring(regexp *, char *);

/*
 * regsub.c
 */
extern int regsub(regexp *, char *, char *, int);

#endif /* __REGEXP_H__ */
