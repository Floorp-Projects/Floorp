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
 * The Original Code is endian helper macros.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
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

#ifndef MOZILLA_IMAGELIB_ENDIAN_H_
#define MOZILLA_IMAGELIB_ENDIAN_H_

#include "prtypes.h"

#if defined WORDS_BIGENDIAN || defined IS_BIG_ENDIAN
// We must ensure that the entity is unsigned
// otherwise, if it is signed/negative, the MSB will be
// propagated when we shift
#define LITTLE_TO_NATIVE16(x) (((((PRUint16) x) & 0xFF) << 8) | \
                               (((PRUint16) x) >> 8))
#define LITTLE_TO_NATIVE32(x) (((((PRUint32) x) & 0xFF) << 24) | \
                               (((((PRUint32) x) >> 8) & 0xFF) << 16) | \
                               (((((PRUint32) x) >> 16) & 0xFF) << 8) | \
                               (((PRUint32) x) >> 24))
#define NATIVE32_TO_LITTLE LITTLE_TO_NATIVE32
#define NATIVE16_TO_LITTLE LITTLE_TO_NATIVE16
#else
#define LITTLE_TO_NATIVE16(x) x
#define LITTLE_TO_NATIVE32(x) x
#define NATIVE32_TO_LITTLE(x) x
#define NATIVE16_TO_LITTLE(x) x
#endif

#endif
