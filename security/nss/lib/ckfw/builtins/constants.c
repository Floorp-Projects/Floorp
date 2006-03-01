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

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: constants.c,v $ $Revision: 1.11 $ $Date: 2006/03/01 19:44:24 $";
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

#ifndef NSSCKBI_H
#include "nssckbi.h"
#endif /* NSSCKBI_H */

NSS_IMPLEMENT_DATA const CK_VERSION
nss_builtins_CryptokiVersion =  {
		NSS_BUILTINS_CRYPTOKI_VERSION_MAJOR,
		NSS_BUILTINS_CRYPTOKI_VERSION_MINOR };

NSS_IMPLEMENT_DATA const NSSUTF8 *
nss_builtins_ManufacturerID = (NSSUTF8 *) "Mozilla Foundation";

NSS_IMPLEMENT_DATA const NSSUTF8 *
nss_builtins_LibraryDescription = (NSSUTF8 *) "NSS Builtin Object Cryptoki Module";

NSS_IMPLEMENT_DATA const CK_VERSION
nss_builtins_LibraryVersion = {
	NSS_BUILTINS_LIBRARY_VERSION_MAJOR,
	NSS_BUILTINS_LIBRARY_VERSION_MINOR};

NSS_IMPLEMENT_DATA const NSSUTF8 *
nss_builtins_SlotDescription = (NSSUTF8 *) "";

NSS_IMPLEMENT_DATA const CK_VERSION
nss_builtins_HardwareVersion = { 
	NSS_BUILTINS_HARDWARE_VERSION_MAJOR,
	NSS_BUILTINS_HARDWARE_VERSION_MINOR };

NSS_IMPLEMENT_DATA const CK_VERSION
nss_builtins_FirmwareVersion = { 
	NSS_BUILTINS_FIRMWARE_VERSION_MAJOR,
	NSS_BUILTINS_FIRMWARE_VERSION_MINOR };

NSS_IMPLEMENT_DATA const NSSUTF8 *
nss_builtins_TokenLabel = (NSSUTF8 *) "Builtin Object Token";

NSS_IMPLEMENT_DATA const NSSUTF8 *
nss_builtins_TokenModel = (NSSUTF8 *) "1";

/* should this be e.g. the certdata.txt RCS revision number? */
NSS_IMPLEMENT_DATA const NSSUTF8 *
nss_builtins_TokenSerialNumber = (NSSUTF8 *) "1";

