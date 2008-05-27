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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
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

#ifndef nsCPrefetchService_h__
#define nsCPrefetchService_h__

#include "nsIPrefetchService.h"

/**
 * nsPrefetchService : nsIPrefetchService
 */
#define NS_PREFETCHSERVICE_CLASSNAME \
    "nsPrefetchService"
#define NS_PREFETCHSERVICE_CONTRACTID \
    "@mozilla.org/prefetch-service;1"
#define NS_PREFETCHSERVICE_CID                       \
{ /* 6b8bdffc-3394-417d-be83-a81b7c0f63bf */         \
    0x6b8bdffc,                                      \
    0x3394,                                          \
    0x417d,                                          \
    {0xbe, 0x83, 0xa8, 0x1b, 0x7c, 0x0f, 0x63, 0xbf} \
}

/**
 * nsOfflineCacheUpdateService : nsIOfflineCacheUpdateService
 */

#define NS_OFFLINECACHEUPDATESERVICE_CLASSNAME \
  "nsOfflineCacheUpdateService"
#define NS_OFFLINECACHEUPDATESERVICE_CONTRACTID \
  "@mozilla.org/offlinecacheupdate-service;1"
#define NS_OFFLINECACHEUPDATESERVICE_CID             \
{ /* ec06f3fc-70db-4ecd-94e0-a6e91ca44d8a */         \
    0xec06f3fc,                                      \
    0x70db,                                          \
    0x4ecd ,                                         \
    {0x94, 0xe0, 0xa6, 0xe9, 0x1c, 0xa4, 0x4d, 0x8a} \
}

/**
 * nsOfflineCacheUpdate : nsIOfflineCacheUpdate
 */

#define NS_OFFLINECACHEUPDATE_CLASSNAME \
  "nsOfflineCacheUpdate"
#define NS_OFFLINECACHEUPDATE_CONTRACTID \
  "@mozilla.org/offlinecacheupdate;1"
#define NS_OFFLINECACHEUPDATE_CID                    \
{ /* e56f5e01-c7cc-4675-a9d7-b8f1e4127295 */         \
    0xe56f5e01,                                      \
    0xc7cc,                                          \
    0x4675,                                          \
    {0xa9, 0xd7, 0xb8, 0xf1, 0xe4, 0x12, 0x72, 0x95} \
}


#endif // !nsCPrefetchService_h__
