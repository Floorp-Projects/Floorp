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

#ifndef PKI1T_H
#define PKI1T_H

#ifdef DEBUG
static const char PKI1T_CVS_ID[] = "@(#) $RCSfile: pki1t.h,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:16:25 $ $Name:  $";
#endif /* DEBUG */

/*
 * pki1t.h
 *
 * This file contains definitions for the types used in the PKIX part-1
 * code, but not available publicly.
 */

#ifndef BASET_H
#include "baset.h"
#endif /* BASET_H */

#ifndef NSSPKI1T_H
#include "nsspki1t.h"
#endif /* NSSPKI1T_H */

PR_BEGIN_EXTERN_C

/*
 * NSSOID
 *
 * This structure is used to hold our internal table of built-in OID
 * data.  The fields are as follows:
 *
 *  NSSItem     data -- this is the actual DER-encoded multinumber oid
 *  const char *expl -- this explains the derivation, and is checked
 *                      in a unit test.  While the field always exists,
 *                      it is only populated or used in debug builds.
 *
 */

struct NSSOIDStr {
#ifdef DEBUG
  const NSSUTF8 *tag;
  const NSSUTF8 *expl;
#endif /* DEBUG */
  NSSItem data;
};

/*
 * nssAttributeTypeAliasTable
 *
 * Attribute types are passed around as oids (at least in the X.500
 * and PKI worlds, as opposed to ldap).  However, when written as 
 * strings they usually have well-known aliases, e.g., "ou" or "c."
 *
 * This type defines a table, populated in the generated oiddata.c
 * file, of the aliases we recognize.
 *
 * The fields are as follows:
 *
 *  NSSUTF8 *alias -- a well-known string alias for an oid
 *  NSSOID  *oid   -- the oid to which the alias corresponds
 *
 */

struct nssAttributeTypeAliasTableStr {
  const NSSUTF8 *alias;
  const NSSOID **oid;
};
typedef struct nssAttributeTypeAliasTableStr nssAttributeTypeAliasTable;

PR_END_EXTERN_C

#endif /* PKI1T_H */
