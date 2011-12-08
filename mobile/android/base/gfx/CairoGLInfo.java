/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
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
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick Walton <pcwalton@mozilla.com>
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

package org.mozilla.gecko.gfx;

import javax.microedition.khronos.opengles.GL10;

/** Information needed to render Cairo bitmaps using OpenGL ES. */
public class CairoGLInfo {
    public final int internalFormat;
    public final int format;
    public final int type;

    public CairoGLInfo(int cairoFormat) {
        switch (cairoFormat) {
        case CairoImage.FORMAT_ARGB32:
            internalFormat = format = GL10.GL_RGBA; type = GL10.GL_UNSIGNED_BYTE;
            break;
        case CairoImage.FORMAT_RGB24:
            internalFormat = format = GL10.GL_RGB; type = GL10.GL_UNSIGNED_BYTE;
            break;
        case CairoImage.FORMAT_RGB16_565:
            internalFormat = format = GL10.GL_RGB; type = GL10.GL_UNSIGNED_SHORT_5_6_5;
            break;
        case CairoImage.FORMAT_A8:
        case CairoImage.FORMAT_A1:
            throw new RuntimeException("Cairo FORMAT_A1 and FORMAT_A8 unsupported");
        default:
            throw new RuntimeException("Unknown Cairo format");
        }
    }
}

