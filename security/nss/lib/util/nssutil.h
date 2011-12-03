/*
 * NSS utility functions
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Network Security Services.
 *
 * The Initial Developer of the Original Code is
 * Red Hat Inc.
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#ifndef __nssutil_h_
#define __nssutil_h_

#ifndef RC_INVOKED
#include "seccomon.h"
#endif

/*
 * NSS utilities's major version, minor version, patch level, build number,
 * and whether this is a beta release.
 *
 * The format of the version string should be
 *     "<major version>.<minor version>[.<patch level>[.<build number>]][ <Beta>]"
 */
#define NSSUTIL_VERSION  "3.13.2.0 Beta"
#define NSSUTIL_VMAJOR   3
#define NSSUTIL_VMINOR   13
#define NSSUTIL_VPATCH   2
#define NSSUTIL_VBUILD   0
#define NSSUTIL_BETA     PR_TRUE

SEC_BEGIN_PROTOS

/*
 * Returns a const string of the UTIL library version.
 */
extern const char *NSSUTIL_GetVersion(void);

extern SECStatus
NSS_InitializePRErrorTable(void);

SEC_END_PROTOS

#endif /* __nssutil_h_ */
