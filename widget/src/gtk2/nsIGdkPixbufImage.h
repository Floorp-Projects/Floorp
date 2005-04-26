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
 * The Original Code is Image->GdkPixbuf conversion code.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#ifndef NSIGDKPIXBUFIMAGE_H_
#define NSIGDKPIXBUFIMAGE_H_

#include "nsIImage.h"

// {348A4834-9D5B-d911-87C6-000244212BCB}
#define NSIGDKPIXBUFIMAGE_IID \
{ 0x348a4834, 0x9d5b, 0xd911, { 0x87, 0xc6, 0x0, 0x2, 0x44, 0x21, 0x2b, 0xcb } }

typedef struct _GdkPixbuf GdkPixbuf;

/**
 * An interface that allows getting a GdkPixbuf*.
 */
class nsIGdkPixbufImage : public nsIImage {
  public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NSIGDKPIXBUFIMAGE_IID)

    /**
     * Get a GdkPixbuf* for this image. Returns NULL on failure.
     * The caller must g_object_unref it when done using it.
     */
    NS_IMETHOD_(GdkPixbuf*) GetGdkPixbuf() = 0;
};

#endif
