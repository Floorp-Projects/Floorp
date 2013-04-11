/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * cdbhdl.h - certificate database handle
 *   private to the certdb module
 *
 * $Id$
 */
#ifndef _CDBHDL_H_
#define _CDBHDL_H_

#include "nspr.h"
#include "mcom_db.h"
#include "pcertt.h"
#include "prtypes.h"

/*
 * Handle structure for open certificate databases
 */
struct NSSLOWCERTCertDBHandleStr {
    DB *permCertDB;
    PZMonitor *dbMon;
    PRBool dbVerify;
    PRInt32  ref; /* reference count */
};

#ifdef DBM_USING_NSPR
#define NO_RDONLY	PR_RDONLY
#define NO_RDWR		PR_RDWR
#define NO_CREATE	(PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE)
#else
#define NO_RDONLY	O_RDONLY
#define NO_RDWR		O_RDWR
#define NO_CREATE	(O_RDWR | O_CREAT | O_TRUNC)
#endif

typedef DB * (*rdbfunc)(const char *appName, const char *prefix, 
				const char *type, int flags);
typedef int (*rdbstatusfunc)(void);

#define RDB_FAIL 1
#define RDB_RETRY 2

DB * rdbopen(const char *appName, const char *prefix, 
				const char *type, int flags, int *status);

DB *dbsopen (const char *dbname , int flags, int mode, DBTYPE type, 
						const void * appData);
SECStatus db_Copy(DB *dest,DB *src);
int db_InitComplete(DB *db);

#endif
