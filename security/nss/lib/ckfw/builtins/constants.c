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

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: constants.c,v $ $Revision: 1.3 $ $Date: 2000/09/06 22:24:00 $ $Name:  $";
#endif /* DEBUG */

/*
 * builtins/constants.c
 *
 * Identification and other constants, all collected here in one place.
 */

#ifndef NSSBASET_H
#include "nssbaset.h"
#endif /* NSSBASET_H */

#ifndef NSSCKT_H
#include "nssckt.h"
#endif /* NSSCKT_H */

NSS_IMPLEMENT_DATA const CK_VERSION
nss_builtins_CryptokiVersion = { 2, 1 };

NSS_IMPLEMENT_DATA const NSSUTF8 *
nss_builtins_ManufacturerID = (NSSUTF8 *) "Netscape Communications Corp.";

NSS_IMPLEMENT_DATA const NSSUTF8 *
nss_builtins_LibraryDescription = (NSSUTF8 *) "NSS Builtin Object Cryptoki Module";

NSS_IMPLEMENT_DATA const CK_VERSION
nss_builtins_LibraryVersion = { 1, 0 };

NSS_IMPLEMENT_DATA const NSSUTF8 *
nss_builtins_SlotDescription = "";

NSS_IMPLEMENT_DATA const CK_VERSION
nss_builtins_HardwareVersion = { 1, 0 };

NSS_IMPLEMENT_DATA const CK_VERSION
nss_builtins_FirmwareVersion = { 1, 0 };

NSS_IMPLEMENT_DATA const NSSUTF8 *
nss_builtins_TokenLabel = (NSSUTF8 *) "Builtin Object Token";

NSS_IMPLEMENT_DATA const NSSUTF8 *
nss_builtins_TokenModel = (NSSUTF8 *) "1";

/* should this be e.g. the certdata.txt RCS revision number? */
NSS_IMPLEMENT_DATA const NSSUTF8 *
nss_builtins_TokenSerialNumber = (NSSUTF8 *) "1";

