/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.opengl.GLES20;
import java.util.ArrayList;

/** Manages a list of dead tiles, so we don't leak resources. */
public class TextureReaper {
    private static TextureReaper sSharedInstance;
    private ArrayList<Integer> mDeadTextureIDs;

    private TextureReaper() { mDeadTextureIDs = new ArrayList<Integer>(); }

    public static TextureReaper get() {
        if (sSharedInstance == null)
            sSharedInstance = new TextureReaper();
        return sSharedInstance;
    }

    public void add(int[] textureIDs) {
        for (int textureID : textureIDs)
            add(textureID);
    }

    public void add(int textureID) {
        mDeadTextureIDs.add(textureID);
    }

    public void reap() {
        int[] deadTextureIDs = new int[mDeadTextureIDs.size()];
        for (int i = 0; i < deadTextureIDs.length; i++)
            deadTextureIDs[i] = mDeadTextureIDs.get(i);
        mDeadTextureIDs.clear();

        GLES20.glDeleteTextures(deadTextureIDs.length, deadTextureIDs, 0);
    }
}


