/* vim:set ts=2 sw=2 et cindent: */
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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

#ifndef md4_h__
#define md4_h__

#ifdef __cplusplus
extern "C" {
#endif

#include "prtypes.h"

/**
 * md4sum - computes the MD4 sum over the input buffer per RFC 1320
 * 
 * @param input
 *        buffer containing input data
 * @param inputLen
 *        length of input buffer (number of bytes)
 * @param result
 *        16-byte buffer that will contain the MD4 sum upon return
 *
 * NOTE: MD4 is superceded by MD5.  do not use MD4 unless required by the
 * protocol you are implementing (e.g., NTLM requires MD4).
 *
 * NOTE: this interface is designed for relatively small buffers.  A streaming
 * interface would make more sense if that were a requirement.  Currently, this
 * is good enough for the applications we care about.
 */
void md4sum(const PRUint8 *input, PRUint32 inputLen, PRUint8 *result);

#ifdef __cplusplus
}
#endif

#endif /* md4_h__ */
