/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tiles;

/**
 * Metadata for a tile shown on the top sites grid.
 */
public class Tile {
    public final int id;
    public final boolean pinned;

    public Tile(int id, boolean pinned) {
        this.id = id;
        this.pinned = pinned;
    }
}
