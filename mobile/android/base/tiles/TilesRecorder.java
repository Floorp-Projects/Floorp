/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tiles;

import java.util.List;

import org.json.JSONArray;
import org.json.JSONObject;
import org.json.JSONException;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.Tab;

import android.util.Log;

public class TilesRecorder {
    public static final String ACTION_CLICK = "click";

    private static final String LOG_TAG = "GeckoTilesRecorder";
    private static final String EVENT_TILES_CLICK = "Tiles:Click";

    public void recordAction(Tab tab, String action, int index, List<Tile> tiles) {
        final Tile clickedTile = tiles.get(index);

        if (tab == null || clickedTile == null) {
            throw new IllegalArgumentException("Tab and tile cannot be null");
        }

        if (clickedTile.id == -1) {
            // User clicked a non-distribution tile, so we don't need to report it.
            return;
        }

        try {
            final JSONArray tilesJSON = new JSONArray();
            int clickedTileIndex = -1;
            int currentTileIndex = 0;

            for (int i = 0; i < tiles.size(); i++) {
                final Tile tile = tiles.get(i);
                if (tile == null) {
                    // Tiles may be null if there are pinned tiles with blank tiles in between.
                    continue;
                }

                // jsonForTile will return {} if the tile isn't tracked or pinned.  That's fine
                // as we still want to record that a tile exists (i.e., is not empty).
                tilesJSON.put(jsonForTile(tile, currentTileIndex, i));

                // The click index is relative to the tiles array we're building. That index is
                // incremented whenever we hit a non-null tile. When we find the tile that was
                // clicked, the index will match its position in the new array.
                if (clickedTile == tile) {
                    clickedTileIndex = currentTileIndex;
                }

                currentTileIndex++;
            }

            if (clickedTileIndex == -1) {
                throw new IllegalStateException("Clicked tile index not set");
            }

            final JSONObject payload = new JSONObject();
            payload.put(action, clickedTileIndex);
            payload.put("tiles", tilesJSON);

            final JSONObject data = new JSONObject();
            data.put("tabId", tab.getId());
            data.put("payload", payload.toString());

            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent(EVENT_TILES_CLICK, data.toString()));
        } catch (JSONException e) {
            Log.e(LOG_TAG, "JSON error", e);
        }
    }

    private JSONObject jsonForTile(Tile tile, int tileIndex, int viewIndex) throws JSONException {
        final JSONObject tileJSON = new JSONObject();

        // Set the ID only if it is a distribution tile with a tracking ID.
        if (tile.id != -1) {
            tileJSON.put("id", tile.id);
        }

        // Set pinned to true only if the tile is pinned.
        if (tile.pinned) {
            tileJSON.put("pin", true);
        }

        // If the tile's index in the new array does not match its index in the view grid, record
        // its position in the view grid. This can happen if there are pinned tiles with blank tiles
        // in between.
        if (tileIndex != viewIndex) {
            tileJSON.put("pos", viewIndex);
        }

        return tileJSON;
    }
}
