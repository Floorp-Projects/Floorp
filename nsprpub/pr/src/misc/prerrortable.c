/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */




/*

Copyright 1987, 1988 by the Student Information Processing Board
	of the Massachusetts Institute of Technology

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is
hereby granted, provided that the above copyright notice
appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation,
and that the names of M.I.T. and the M.I.T. S.I.P.B. not be
used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.
M.I.T. and the M.I.T. S.I.P.B. make no representations about
the suitability of this software for any purpose.  It is
provided "as is" without express or implied warranty.

*/

#include <string.h>
#ifdef SUNOS4
#include "md/sunos4.h"  /* for strerror */
#endif
#include <assert.h>
#include <errno.h>
#include "prmem.h"
#include "prerror.h"

#define	ERRCODE_RANGE	8	/* # of bits to shift table number */
#define	BITS_PER_CHAR	6	/* # bits to shift per character in name */

#ifdef NEED_SYS_ERRLIST
extern char const * const sys_errlist[];
extern const int sys_nerr;
#endif

/* List of error tables */
struct PRErrorTableList {
    struct PRErrorTableList *next;
    const struct PRErrorTable *table;
    struct PRErrorCallbackTablePrivate *table_private;
};
static struct PRErrorTableList * Table_List = (struct PRErrorTableList *) NULL;

/* Supported languages */
static const char * default_languages[] = { "i-default", "en", 0 };
static const char * const * callback_languages = default_languages;

/* Callback info */
static struct PRErrorCallbackPrivate *callback_private = 0;
static PRErrorCallbackLookupFn *callback_lookup = 0;
static PRErrorCallbackNewTableFn *callback_newtable = 0;


static const char char_set[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_";

static const char *
error_table_name (PRErrorCode num)
{
    static char buf[6];	/* only used if internal code problems exist */

    long ch;
    int i;
    char *p;

    /* num = aa aaa abb bbb bcc ccc cdd ddd d?? ??? ??? */
    p = buf;
    num >>= ERRCODE_RANGE;
    /* num = ?? ??? ??? aaa aaa bbb bbb ccc ccc ddd ddd */
    num &= 077777777;
    /* num = 00 000 000 aaa aaa bbb bbb ccc ccc ddd ddd */
    for (i = 4; i >= 0; i--) {
	ch = (num >> BITS_PER_CHAR * i) & ((1 << BITS_PER_CHAR) - 1);
	if (ch != 0)
	    *p++ = char_set[ch-1];
    }
    *p = '\0';
    return(buf);
}

PR_IMPLEMENT(const char *)
PR_ErrorToString(PRErrorCode code, PRLanguageCode language)
{
    /* static buffer only used if code is using inconsistent error message
     * numbers, so just ignore the possible thread contention
     */
    static char buffer[25];

    const char *msg;
    int offset;
    PRErrorCode table_num;
    struct PRErrorTableList *et;
    int started = 0;
    char *cp;

    for (et = Table_List; et; et = et->next) {
	if (et->table->base <= code &&
	    et->table->base + et->table->n_msgs > code) {
	    /* This is the right table */
	    if (callback_lookup) {
		msg = callback_lookup(code, language, et->table,
		    callback_private, et->table_private);
		if (msg) return msg;
	    }
    
	    return(et->table->msgs[code - et->table->base].en_text);
	}
    }

    if (code >= 0 && code < 256) {
	return strerror(code);
    }

    offset = (int) (code & ((1<<ERRCODE_RANGE)-1));
    table_num = code - offset;
    strcpy (buffer, "Unknown code ");
    if (table_num) {
	strcat(buffer, error_table_name (table_num));
	strcat(buffer, " ");
    }
    for (cp = buffer; *cp; cp++)
	;
    if (offset >= 100) {
	*cp++ = (char)('0' + offset / 100);
	offset %= 100;
	started++;
    }
    if (started || offset >= 10) {
	*cp++ = (char)('0' + offset / 10);
	offset %= 10;
    }
    *cp++ = (char)('0' + offset);
    *cp = '\0';
    return(buffer);
}

PR_IMPLEMENT(const char *)
PR_ErrorToName(PRErrorCode code)
{
    struct PRErrorTableList *et;

    for (et = Table_List; et; et = et->next) {
	if (et->table->base <= code &&
	    et->table->base + et->table->n_msgs > code) {
	    /* This is the right table */
	    return(et->table->msgs[code - et->table->base].name);
	}
    }

    return 0;
}

PR_IMPLEMENT(const char * const *)
PR_ErrorLanguages(void)
{
    return callback_languages;
}

PR_IMPLEMENT(PRErrorCode)
PR_ErrorInstallTable(const struct PRErrorTable *table)
{
    struct PRErrorTableList * new_et;

    new_et = (struct PRErrorTableList *)
					PR_Malloc(sizeof(struct PRErrorTableList));
    if (!new_et)
	return errno;	/* oops */
    new_et->table = table;
    if (callback_newtable) {
	new_et->table_private = callback_newtable(table, callback_private);
    } else {
	new_et->table_private = 0;
    }
    new_et->next = Table_List;
    Table_List = new_et;
    return 0;
}

PR_IMPLEMENT(void)
PR_ErrorInstallCallback(const char * const * languages,
		       PRErrorCallbackLookupFn *lookup, 
		       PRErrorCallbackNewTableFn *newtable,
		       struct PRErrorCallbackPrivate *cb_private)
{
    struct PRErrorTableList *et;

    assert(strcmp(languages[0], "i-default") == 0);
    assert(strcmp(languages[1], "en") == 0);
    
    callback_languages = languages;
    callback_lookup = lookup;
    callback_newtable = newtable;
    callback_private = cb_private;

    if (callback_newtable) {
	for (et = Table_List; et; et = et->next) {
	    et->table_private = callback_newtable(et->table, callback_private);
	}
    }
}
