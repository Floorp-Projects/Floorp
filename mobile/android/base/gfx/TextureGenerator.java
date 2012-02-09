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
 *   James Willcox <jwillcox@mozilla.com>
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

import android.opengl.GLES20;
import java.util.Stack;

public class TextureGenerator {
    private static final int MIN_TEXTURES = 5;

    private static TextureGenerator sSharedInstance;
    private Stack<Integer> mTextureIds;

    private TextureGenerator() { mTextureIds = new Stack<Integer>(); }

    public static TextureGenerator get() {
        if (sSharedInstance == null)
            sSharedInstance = new TextureGenerator();
        return sSharedInstance;
    }

    public synchronized int take() {
        if (mTextureIds.empty())
            return 0;

        return (int)mTextureIds.pop();
    }

    public synchronized void fill() {
        int[] textures = new int[1];
        while (mTextureIds.size() < MIN_TEXTURES) {
            GLES20.glGenTextures(1, textures, 0);
            mTextureIds.push(textures[0]);
        }
    }
}


