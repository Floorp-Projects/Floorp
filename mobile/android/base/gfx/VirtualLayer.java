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
 *   Chris Lord <chrislord.net@gmail.com>
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

import android.graphics.Rect;

public class VirtualLayer extends Layer {
    public VirtualLayer(IntSize size) {
        super(size);
    }

    @Override
    public void draw(RenderContext context) {
        // No-op.
    }

    void setPositionAndResolution(Rect newPosition, float newResolution) {
        // This is an optimized version of the following code:
        // beginTransaction();
        // try {
        //     setPosition(newPosition);
        //     setResolution(newResolution);
        //     performUpdates(null);
        // } finally {
        //     endTransaction();
        // }

        // it is safe to drop the transaction lock in this instance (i.e. for the
        // VirtualLayer that is just a shadow of what gecko is painting) because
        // the position and resolution of this layer are never used for anything
        // meaningful.
        // XXX The above is not true any more; the compositor uses these values
        // in order to determine where to draw the checkerboard. The values are
        // also used in LayerController's convertViewPointToLayerPoint function.
        mPosition = newPosition;
        mResolution = newResolution;
    }

    @Override
    public void setDisplayPort(Rect displayPort) {
        // Similar to the above, this removes the lock from setDisplayPort. As
        // this is currently only called by the Compositor, and it is only
        // accessed by the composition thread, it is safe to remove the locks
        // from this call.
        mDisplayPort = displayPort;
    }
}
