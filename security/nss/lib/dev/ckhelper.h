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

/*
 * ckhelper.h
 *
 * This file contains some helper utilities for interaction with cryptoki.
 */

#ifndef CKHELPER_H
#define CKHELPER_H

#ifdef DEBUG
static const char CKHELPER_CVS_ID[] = "@(#) $RCSfile: ckhelper.h,v $ $Revision: 1.1 $ $Date: 2001/09/13 22:06:08 $ $Name:  $";
#endif /* DEBUG */

#ifndef NSSCKT_H
#include "nssckt.h"
#endif /* NSSCKT_H */

PR_BEGIN_EXTERN_C

/* Shortcut to cryptoki API functions. */
#define CKAPI(x) \
    ((CK_FUNCTION_LIST_PTR)((x)->epv))

/* Some globals to keep from constantly redeclaring common cryptoki
 * attribute types on the stack.
 */

/* Boolean values */
NSS_EXTERN_DATA const CK_BBOOL g_ck_true;
NSS_EXTERN_DATA const CK_BBOOL g_ck_false;

/* Object classes */
NSS_EXTERN_DATA const CK_OBJECT_CLASS g_ck_obj_class_cert;
NSS_EXTERN_DATA const CK_OBJECT_CLASS g_ck_obj_class_pubkey;
NSS_EXTERN_DATA const CK_OBJECT_CLASS g_ck_obj_class_privkey;

/*
 * Helpers for building templates.
 */

/*
 * NSS_SET_CK_ATTRIB_GLOBAL(cktmpl, i, ckattr, global_value)
 *
 * used to set one of the global attributes defined above.
 */
#define NSS_SET_CK_ATTRIB_GLOBAL(cktmpl, i, ckattr, global_value)      \
    (cktmpl)[i].type       = ckattr;                                   \
    (cktmpl)[i].pValue     = (CK_VOID_PTR)&(global_value);             \
    (cktmpl)[i].ulValueLen = (CK_ULONG)sizeof(global_value);

/*
 * NSS_SET_CK_ATTRIB_TRUE(cktmpl, i, ckattr)
 *
 * Set an attribute to CK_TRUE in a template
 */
#define NSS_SET_CK_ATTRIB_TRUE(cktmpl, i, ckattr)                      \
    NSS_SET_CK_ATTRIB_GLOBAL(cktmpl, i, ckattr, g_ck_true)

/*
 * NSS_SET_CK_ATTRIB_FALSE(cktmpl, i, ckattr)
 *
 * Set an attribute to CK_FALSE in a template
 */
#define NSS_SET_CK_ATTRIB_FALSE(cktmpl, i, ckattr)                     \
    NSS_SET_CK_ATTRIB_GLOBAL(cktmpl, i, ckattr, g_ck_false)

/*
 * NSS_SET_CK_ATTRIB_ITEM(cktmpl, i, ckattr, item)
 *
 * Set an attribute in a template to use data from an item 
 */
#define NSS_SET_CK_ATTRIB_ITEM(cktmpl, i, ckattr, item)                \
    (cktmpl)[i].type       = ckattr;                                   \
    (cktmpl)[i].pValue     = (CK_VOID_PTR)(item)->data;                \
    (cktmpl)[i].ulValueLen = (CK_ULONG)(item)->len;

/* Create Templates */

/* Certificate template 
 *
 * NSS_CK_CERTIFICATE_CREATE4(cktmpl, subject, id, der)
 *
 *  CKA_CLASS   = CKO_CERTIFICATE
 *  CKA_SUBJECT = subject->data
 *  CKA_ID      = id->data
 *  CKA_VALUE   = der->data
 */

#define NSS_CK_CERTIFICATE_CREATE4(cktmpl, subject, id, der)            \
    NSS_SET_CK_ATTRIB_GLOBAL(cktmpl, 0, CKA_CLASS, g_ck_obj_class_cert) \
    NSS_SET_CK_ATTRIB_ITEM(cktmpl, 1, CKA_SUBJECT, subject)             \
    NSS_SET_CK_ATTRIB_ITEM(cktmpl, 2, CKA_ID, id)                       \
    NSS_SET_CK_ATTRIB_ITEM(cktmpl, 3, CKA_VALUE, der)   

/* Search Templates */

/* NSS_CK_CERTIFICATE_SEARCH(cktmpl)
 *
 * Set up a search template for any cert object
 */
#define NSS_CK_CERTIFICATE_SEARCH(cktmpl)                               \
    NSS_SET_CK_ATTRIB_GLOBAL(cktmpl, 0, CKA_CLASS, g_ck_obj_class_cert)

/* NSS_CK_CERTIFICATE_SEARCH_LABEL2(cktmpl, label)
 *
 * Set up a search template for a cert using label->data
 */
#define NSS_CK_CERTIFICATE_SEARCH_LABEL2(cktmpl, label)                \
    NSS_CK_CERTIFICATE_SEARCH(cktmpl)                                  \
    NSS_SET_CK_ATTRIB_ITEM(cktmpl, 1, CKA_LABEL, label)

/* NSS_CK_CERTIFICATE_SEARCH_SUBJECT2(cktmpl, subject)
 *
 * Set up a search template for a cert using subject->data
 */
#define NSS_CK_CERTIFICATE_SEARCH_SUBJECT2(cktmpl, subject)            \
    NSS_CK_CERTIFICATE_SEARCH(cktmpl)                                  \
    NSS_SET_CK_ATTRIB_ITEM(cktmpl, 1, CKA_SUBJECT, subject)

/* NSS_CK_CERTIFICATE_SEARCH_ID2(cktmpl, id)
 *
 * Set up a search template for a cert using id->data
 */
#define NSS_CK_CERTIFICATE_SEARCH_ID2(cktmpl, id)                      \
    NSS_CK_CERTIFICATE_SEARCH(cktmpl)                                  \
    NSS_SET_CK_ATTRIB_ITEM(cktmpl, 1, CKA_ID, id)

/* NSS_CK_CERTIFICATE_SEARCH_DER2(cktmpl, der)
 *
 * Set up a search template for a cert using der->data
 */
#define NSS_CK_CERTIFICATE_SEARCH_DER2(cktmpl, der)                    \
    NSS_CK_CERTIFICATE_SEARCH(cktmpl)                                  \
    NSS_SET_CK_ATTRIB_ITEM(cktmpl, 1, CKA_VALUE, der)

/* NSS_CK_ATTRIBUTE_TO_ITEM(attrib, item)
 *
 * Convert a CK_ATTRIBUTE to an NSSItem.
 */
#define NSS_CK_ATTRIBUTE_TO_ITEM(attrib, item)                         \
    (item)->data = (void *)(attrib)->pValue;                           \
    (item)->size = (PRUint32)(attrib)->ulValueLen;                     \

/* Get an array of attributes from an object. */
NSS_EXTERN PRStatus 
NSSCKObject_GetAttributes
(
  CK_OBJECT_HANDLE object,
  CK_ATTRIBUTE_PTR obj_template,
  CK_ULONG count,
  NSSArena *arenaOpt,
  NSSSlot  *slot
);

PR_END_EXTERN_C

#endif /* CKHELPER_H */
