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
/*
 * cdbhdl.h - certificate database handle
 *   private to the certdb module
 *
 * $Id: cdbhdl.h,v 1.10 2005/09/28 17:12:17 relyea%netscape.com Exp $
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

#define nsslowcert_reference(x) (PR_AtomicIncrement(&(x)->ref) , (x))

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
int db_BeginTransaction(DB *db);
int db_FinishTransaction(DB *db, PRBool abort);
int db_InitComplete(DB *db);

#endif
