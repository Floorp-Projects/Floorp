/*
 * private.h - Private data structures for the software token library
 *
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
 *
 * $Id: keydbi.h,v 1.4 2002/08/02 21:41:01 relyea%netscape.com Exp $
 */

#ifndef _KEYDBI_H_
#define _KEYDBI_H_

#include "nspr.h"
#include "seccomon.h"
#include "mcom_db.h"

/*
 * Handle structure for open key databases
 */
struct NSSLOWKEYDBHandleStr {
    DB *db;
    DB *updatedb;		/* used when updating an old version */
    SECItem *global_salt;	/* password hashing salt for this db */
    int version;		/* version of the database */
    char *appname;		/* multiaccess app name */
    char *dbname;		/* name of the openned DB */
    PRBool readOnly;		/* is the DB read only */
};

/*
** Typedef for callback for traversing key database.
**      "key" is the key used to index the data in the database (nickname)
**      "data" is the key data
**      "pdata" is the user's data 
*/
typedef SECStatus (* NSSLOWKEYTraverseKeysFunc)(DBT *key, DBT *data, void *pdata);


SEC_BEGIN_PROTOS

/*
** Traverse the entire key database, and pass the nicknames and keys to a 
** user supplied function.
**      "f" is the user function to call for each key
**      "udata" is the user's data, which is passed through to "f"
*/
extern SECStatus nsslowkey_TraverseKeys(NSSLOWKEYDBHandle *handle, 
				NSSLOWKEYTraverseKeysFunc f,
				void *udata);

SEC_END_PROTOS

#endif /* _KEYDBI_H_ */
