/* vim:set sw=4 sts=4 et cin: */
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
 * The Original Code is mozilla.org widget code.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#ifndef NSIMAGETOPIXBUF_H_
#define NSIMAGETOPIXBUF_H_

#include "nsIImageToPixbuf.h"

class gfxASurface;
class gfxPattern;
class gfxImageSurface;

class nsImageToPixbuf : public nsIImageToPixbuf {
    public:
        NS_DECL_ISUPPORTS
        NS_IMETHOD_(GdkPixbuf*) ConvertImageToPixbuf(imgIContainer* aImage);

        // Friendlier version of ConvertImageToPixbuf for callers inside of
        // widget
        static GdkPixbuf* ImageToPixbuf(imgIContainer * aImage);
        static GdkPixbuf* SurfaceToPixbuf(gfxASurface* aSurface,
                                          PRInt32 aWidth, PRInt32 aHeight);
    private:
        static GdkPixbuf* ImgSurfaceToPixbuf(gfxImageSurface* aImgSurface,
                                             PRInt32 aWidth, PRInt32 aHeight);
        ~nsImageToPixbuf() {}
};


// fc2389b8-c650-4093-9e42-b05e5f0685b7
#define NS_IMAGE_TO_PIXBUF_CID \
{ 0xfc2389b8, 0xc650, 0x4093, \
  { 0x9e, 0x42, 0xb0, 0x5e, 0x5f, 0x06, 0x85, 0xb7 } }

#endif
