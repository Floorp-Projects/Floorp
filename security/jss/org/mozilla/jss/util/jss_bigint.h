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
 * The Original Code is the Netscape Security Services for Java.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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

/* Need to include these first:
#include <jni.h>
#include <seccomon.h>
*/

#ifndef JSS_BIG_INT_H
#define JSS_BIG_INT_H

PR_BEGIN_EXTERN_C

/***********************************************************************
 *
 * J S S _ O c t e t S t r i n g T o B y t e A r r a y
 *
 * Converts a representation of an integer as a big-endian octet string
 * stored in a SECItem (as used by the low-level crypto functions) to a
 * representation of an integer as a big-endian Java byte array. Prepends
 * a zero byte to force it to be positive.
 */
jbyteArray
JSS_OctetStringToByteArray(JNIEnv *env, SECItem *item);

/***********************************************************************
 *
 * J S S _ B y t e A r r a y T o O c t e t S t r i n g
 *
 * Converts an integer represented as a big-endian Java byte array to
 * an integer represented as a big-endian octet string in a SECItem.
 *
 * INPUTS
 *      byteArray
 *          A Java byte array containing an integer represented in
 *          big-endian format.  Must not be NULL.
 *      item
 *          Pointer to a SECItem that will be filled with the integer
 *          from the byte array, in big-endian format.
 * RETURNS
 *      PR_SUCCESS if the operation was successful, PR_FAILURE if an exception
 *      was thrown.
 */
PRStatus
JSS_ByteArrayToOctetString(JNIEnv *env, jbyteArray byteArray, SECItem *item);

PR_END_EXTERN_C

#endif
