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
 * The Original Code is command line utility function declarations.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian R. Bondy <netzen@gmail.com>
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

#ifndef MAR_CMDLINE_H__
#define MAR_CMDLINE_H__

/* We use NSPR here just to import the definition of PRUint32 */
#include "prtypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Determines if the MAR file is new or old.
 * 
 * @param path   The path of the MAR file to check.
 * @param oldMar An out parameter specifying if the MAR file is new or old.
 * @return A non-zero value if an error occurred and the information 
           cannot be determined.
 */
int is_old_mar(const char *path, int *oldMar);

/**
 * Verifies the embedded signature of the specified file path.
 * This is only used by the signmar program when used with arguments to verify 
 * a MAR. This should not be used to verify a MAR that will be extracted in the 
 * same operation by updater code. This function prints the error message if 
 * verification fails.
 * 
 * @param pathToMAR  The path of the MAR file who's signature should be checked
 * @param certData       The certificate file data.
 * @param sizeOfCertData The size of the cert data.
 * @param certName   Used only if compiled as NSS, specifies the certName
 * @return 0 on success
 *         a negative number if there was an error
 *         a positive number if the signature does not verify
 */
int mar_verify_signature(const char *pathToMAR, 
                         const char *certData,
                         PRUint32 sizeOfCertData,
                         const char *certName);

#ifdef __cplusplus
}
#endif

#endif  /* MAR_CMDLINE_H__ */
