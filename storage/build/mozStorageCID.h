/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
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

#ifndef _MOZSTORAGECID_H_
#define _MOZSTORAGECID_H_

#define MOZ_STORAGE_CONTRACTID_PREFIX "@mozilla.org/storage"


/* b71a1f84-3a70-4d37-a348-f1ba0e27eead */
#define MOZ_STORAGE_CONNECTION_CID \
{ 0xb71a1f84, 0x3a70, 0x4d37, {0xa3, 0x48, 0xf1, 0xba, 0x0e, 0x27, 0xee, 0xad} }

#define MOZ_STORAGE_CONNECTION_CONTRACTID MOZ_STORAGE_CONTRACTID_PREFIX "/connection;1"

/* bbbb1d61-438f-4436-92ed-8308e5830fb0 */
#define MOZ_STORAGE_SERVICE_CID \
{ 0xbbbb1d61, 0x438f, 0x4436, {0x92, 0xed, 0x83, 0x08, 0xe5, 0x83, 0x0f, 0xb0} }

#define MOZ_STORAGE_SERVICE_CONTRACTID MOZ_STORAGE_CONTRACTID_PREFIX "/service;1"

#endif /* _MOZSTORAGECID_H_ */
