/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
