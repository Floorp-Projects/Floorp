/*
 * private.h - Private data structures for the software token library
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    PRLock *lock;
    PRInt32 ref;		/* reference count */
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
