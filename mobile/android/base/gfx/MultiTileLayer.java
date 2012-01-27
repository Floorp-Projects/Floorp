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
 * Portions created by the Initial Developer are Copyright (C) 2011-2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

import org.mozilla.gecko.FloatUtils;
import org.mozilla.gecko.gfx.CairoImage;
import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.gfx.SingleTileLayer;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Region;
import android.util.Log;
import java.lang.Long;
import java.nio.ByteBuffer;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.ListIterator;
import javax.microedition.khronos.opengles.GL10;

/**
 * Encapsulates the logic needed to draw a layer made of multiple tiles.
 *
 * TODO: Support repeating.
 */
public class MultiTileLayer extends Layer {
    private static final String LOGTAG = "GeckoMultiTileLayer";

    private final CairoImage mImage;
    private final IntSize mTileSize;
    private IntSize mBufferSize;
    private Region mDirtyRegion;
    private Region mValidRegion;
    private final LinkedList<SubTile> mTiles;
    private final HashMap<Long, SubTile> mPositionHash;

    public MultiTileLayer(CairoImage image, IntSize tileSize) {
        super();

        mImage = image;
        mTileSize = tileSize;
        mBufferSize = new IntSize(0, 0);
        mDirtyRegion = new Region();
        mValidRegion = new Region();
        mTiles = new LinkedList<SubTile>();
        mPositionHash = new HashMap<Long, SubTile>();
    }

    /**
     * Invalidates a sub-region of the layer. Data will be uploaded from the
     * backing buffer over subsequent calls to update().
     * This method is only valid inside a transaction.
     */
    public void invalidate(Rect dirtyRect) {
        if (!inTransaction()) {
            throw new RuntimeException("invalidate() is only valid inside a transaction");
        }

        mDirtyRegion.union(dirtyRect);
        mValidRegion.union(dirtyRect);
    }

    /**
     * Invalidates the backing buffer. Data will not be uploaded from an invalid
     * backing buffer. This method is only valid inside a transaction.
     */
    public void invalidateBuffer() {
        if (!inTransaction()) {
            throw new RuntimeException("invalidateBuffer() is only valid inside a transaction");
        }

        mDirtyRegion.setEmpty();
        mValidRegion.setEmpty();
    }

    @Override
    public IntSize getSize() {
        return mImage.getSize();
    }

    /**
     * Makes sure there are enough tiles to accommodate the buffer image.
     */
    private void validateTiles() {
        IntSize size = getSize();

        if (size.equals(mBufferSize))
            return;

        mBufferSize = size;

        // Shrink/grow tile pool
        int nTiles = (Math.round(size.width / (float)mTileSize.width) + 1) *
                     (Math.round(size.height / (float)mTileSize.height) + 1);
        if (mTiles.size() < nTiles) {
            Log.i(LOGTAG, "Tile pool growing from " + mTiles.size() + " to " + nTiles);

            for (int i = 0; i < nTiles; i++) {
                mTiles.add(new SubTile(new SubImage(mImage, mTileSize)));
            }
        } else if (mTiles.size() > nTiles) {
            Log.i(LOGTAG, "Tile pool shrinking from " + mTiles.size() + " to " + nTiles);

            // Remove tiles from the beginning of the list, as these are
            // least recently used tiles
            for (int i = mTiles.size(); i > nTiles; i--) {
                SubTile tile = mTiles.get(0);
                if (tile.key != null) {
                    mPositionHash.remove(tile.key);
                }
                mTiles.remove(0);
            }
        }

        // A buffer size probably means a layout change, so invalidate all tiles.
        invalidateTiles();
    }

    /**
     * Returns a Long representing the given Point. Used for hashing.
     */
    private Long longFromPoint(Point point) {
        // Assign 32 bits for each dimension of the point.
        return new Long((((long)point.x) << 32) | point.y);
    }

    /**
     * Performs the necessary functions to update the specified properties of
     * a sub-tile.
     */
    private void updateTile(GL10 gl, RenderContext context, SubTile tile, Point tileOrigin, Rect dirtyRect, boolean reused) {
        tile.beginTransaction(null);
        try {
            if (reused) {
                // Invalidate any area that isn't represented in the current
                // buffer. This is done as SingleTileLayer always updates the
                // entire width, regardless of the dirty-rect's width, and so
                // can override existing data.
                Point origin = getOrigin();
                Rect validRect = tile.getValidTextureArea();
                validRect.offset(tileOrigin.x - origin.x, tileOrigin.y - origin.y);
                Region validRegion = new Region(validRect);
                validRegion.op(mValidRegion, Region.Op.INTERSECT);

                // SingleTileLayer can't draw complex regions, so in that case,
                // just invalidate the entire area.
                tile.invalidateTexture();
                if (!validRegion.isComplex()) {
                    validRect.set(validRegion.getBounds());
                    validRect.offset(origin.x - tileOrigin.x, origin.y - tileOrigin.y);
                }
            } else {
                // Update tile metrics
                tile.setOrigin(tileOrigin);
                tile.setResolution(getResolution());

                // Make sure that non-reused tiles are marked as invalid before
                // uploading new content.
                tile.invalidateTexture();

                // (Re)Place in the position hash for quick retrieval.
                if (tile.key != null) {
                    mPositionHash.remove(tile.key);
                }
                tile.key = longFromPoint(tileOrigin);
                mPositionHash.put(tile.key, tile);
            }

            // Invalidate the area we want to upload.
            tile.invalidate(dirtyRect);

            // Perform updates and mark texture as valid.
            if (!tile.performUpdates(gl, context)) {
                Log.e(LOGTAG, "Sub-tile failed to update fully");
            }
        } finally {
            tile.endTransaction();
        }
    }

    @Override
    protected boolean performUpdates(GL10 gl, RenderContext context) {
        super.performUpdates(gl, context);

        validateTiles();

        // Bail out early if we have nothing to do.
        if (mDirtyRegion.isEmpty() || mTiles.isEmpty()) {
            return true;
        }

        // Check that we're capable of updating from this origin.
        Point origin = getOrigin();
        if ((origin.x % mTileSize.width) != 0 || (origin.y % mTileSize.height) != 0) {
            Log.e(LOGTAG, "MultiTileLayer doesn't support non tile-aligned origins! (" +
                  origin.x + ", " + origin.y + ")");
            return true;
        }

        // Transform the viewport into tile-space so we can see what part of the
        // dirty region intersects with it.
        // We update any tiles intersecting with the screen before tiles
        // intersecting with the viewport.
        // XXX Maybe we want to to split this update even further to update
        //     checkerboard area before updating screen regions with old data.
        //     Note that this could provide inconsistent views, so we may not
        //     want to do this.
        Rect tilespaceViewport;
        float scaleFactor = getResolution() / context.zoomFactor;
        tilespaceViewport = RectUtils.roundOut(RectUtils.scale(context.viewport, scaleFactor));
        tilespaceViewport.offset(-origin.x, -origin.y);

        // Expand tile-space viewport to tile boundaries
        tilespaceViewport.left = (tilespaceViewport.left / mTileSize.width) * mTileSize.width;
        tilespaceViewport.right += mTileSize.width - 1;
        tilespaceViewport.right = (tilespaceViewport.right / mTileSize.width) * mTileSize.width;
        tilespaceViewport.top = (tilespaceViewport.top / mTileSize.height) * mTileSize.height;
        tilespaceViewport.bottom += mTileSize.height - 1;
        tilespaceViewport.bottom = (tilespaceViewport.bottom / mTileSize.height) * mTileSize.height;

        // Declare a region for storing the results of Region operations
        Region opRegion = new Region();

        // Test if the dirty region intersects with the screen
        boolean updateVisible = false;
        Region updateRegion = mDirtyRegion;
        if (opRegion.op(tilespaceViewport, mDirtyRegion, Region.Op.INTERSECT)) {
            updateVisible = true;
            updateRegion = new Region(opRegion);
        }

        // Invalidate any tiles that are due to be replaced if their resolution
        // doesn't match the parent layer resolution, and any tiles that are
        // off-screen and off-buffer, as we cannot guarantee their validity.
        //
        // Note that we also cannot guarantee the validity of on-screen,
        // off-buffer tiles, but this is a rare case that we allow for
        // optimisation purposes.
        //
        // XXX Ideally, we want to remove this second invalidation clause
        //     somehow. It may be possible to know if off-screen tiles are
        //     valid by monitoring reflows on the browser element, or
        //     something along these lines.
        LinkedList<SubTile> invalidTiles = new LinkedList<SubTile>();
        Rect bounds = mValidRegion.getBounds();
        for (ListIterator<SubTile> i = mTiles.listIterator(); i.hasNext();) {
            SubTile tile = i.next();

            if (tile.key == null) {
                continue;
            }

            RectF tileBounds = tile.getBounds(context, new FloatSize(tile.getSize()));
            Rect tilespaceTileBounds =
                RectUtils.round(RectUtils.scale(tileBounds, scaleFactor));
            tilespaceTileBounds.offset(-origin.x, -origin.y);

            // First bracketed clause: Invalidate off-screen, off-buffer tiles
            // Second: Invalidate visible tiles at the wrong resolution that have updates
            if ((!Rect.intersects(bounds, tilespaceTileBounds) &&
                 !Rect.intersects(tilespaceViewport, tilespaceTileBounds)) ||
                (!FloatUtils.fuzzyEquals(tile.getResolution(), getResolution()) &&
                 opRegion.op(tilespaceTileBounds, updateRegion, Region.Op.INTERSECT))) {
                tile.invalidateTexture();

                // Add to the list of invalid tiles and remove from the main list
                invalidTiles.add(tile);
                i.remove();

                // Remove from the position hash
                mPositionHash.remove(tile.key);
                tile.key = null;
            }
        }

        // Push invalid tiles to the head of the queue so they get used first
        mTiles.addAll(0, invalidTiles);

        // Update tiles
        // Note, it's <= as the buffer is over-allocated due to render-offsetting.
        for (int y = origin.y; y <= origin.y + mBufferSize.height; y += mTileSize.height) {
            for (int x = origin.x; x <= origin.x + mBufferSize.width; x += mTileSize.width) {
                // Does this tile intersect with the dirty region?
                Rect tilespaceTileRect = new Rect(x - origin.x, y - origin.y,
                                                  (x - origin.x) + mTileSize.width,
                                                  (y - origin.y) + mTileSize.height);
                if (!opRegion.op(tilespaceTileRect, updateRegion, Region.Op.INTERSECT)) {
                    continue;
                }

                // Dirty tile, find out if we already have this tile to reuse.
                boolean reusedTile = true;
                Point tileOrigin = new Point(x, y);
                SubTile tile = mPositionHash.get(longFromPoint(tileOrigin));

                // If we don't, get an unused tile (we store these at the head of the list).
                if (tile == null) {
                    tile = mTiles.removeFirst();
                    reusedTile = false;
                } else {
                    mTiles.remove(tile);
                }

                // Place tile at the end of the tile-list so it isn't re-used.
                mTiles.add(tile);

                // Work out the tile's invalid area in this tile's space.
                if (opRegion.isComplex()) {
                    Log.w(LOGTAG, "MultiTileLayer encountered complex dirty region");
                }
                Rect dirtyRect = opRegion.getBounds();
                dirtyRect.offset(origin.x - x, origin.y - y);

                // Update tile metrics and texture data
                tile.x = (x - origin.x) / mTileSize.width;
                tile.y = (y - origin.y) / mTileSize.height;
                updateTile(gl, context, tile, tileOrigin, dirtyRect, reusedTile);

                // If this update isn't visible, we only want to update one
                // tile at a time.
                if (!updateVisible) {
                    mDirtyRegion.op(opRegion, Region.Op.XOR);
                    return mDirtyRegion.isEmpty();
                }
            }
        }

        // Remove the update region from the dirty region
        mDirtyRegion.op(updateRegion, Region.Op.XOR);

        return mDirtyRegion.isEmpty();
    }

    @Override
    public void beginTransaction(LayerView aView) {
        super.beginTransaction(aView);

        for (SubTile layer : mTiles) {
            layer.beginTransaction(aView);
        }
    }

    @Override
    public void endTransaction() {
        for (SubTile layer : mTiles) {
            layer.endTransaction();
        }

        super.endTransaction();
    }

    @Override
    public void draw(RenderContext context) {
        for (SubTile layer : mTiles) {
            // Avoid work, only draw tiles that intersect with the viewport
            RectF layerBounds = layer.getBounds(context, new FloatSize(layer.getSize()));
            if (RectF.intersects(layerBounds, context.viewport))
                layer.draw(context);
        }
    }

    /**
     * Invalidates all sub-tiles. This should be called if the source backing
     * this layer has changed. This method is only valid inside a transaction.
     */
    public void invalidateTiles() {
        if (!inTransaction()) {
            throw new RuntimeException("invalidateTiles() is only valid inside a transaction");
        }

        for (SubTile tile : mTiles) {
            // Remove tile from position hash and mark it as invalid
            if (tile.key != null) {
                mPositionHash.remove(tile.key);
                tile.key = null;
            }
            tile.invalidateTexture();
        }
    }

    /**
     * A SingleTileLayer extension with fields for relevant tile data that
     * MultiTileLayer requires.
     */
    private static class SubTile extends SingleTileLayer {
        public int x;
        public int y;

        public Long key;

        public SubTile(SubImage aImage) {
            super(aImage);

            aImage.tile = this;
        }
    }

    /**
     * A CairoImage implementation that returns a tile from a parent CairoImage.
     * This assumes that the parent image has a size that is a multiple of the
     * tile size.
     */
    private static class SubImage extends CairoImage {
        public SubTile tile;

        private IntSize mTileSize;
        private CairoImage mImage;

        public SubImage(CairoImage image, IntSize tileSize) {
            mTileSize = tileSize;
            mImage = image;
        }

        @Override
        public ByteBuffer getBuffer() {
            // Create a ByteBuffer that shares the data of the original
            // buffer, but is positioned and limited so that only the
            // tile data is accessible.
            IntSize bufferSize = mImage.getSize();
            int bpp = CairoUtils.bitsPerPixelForCairoFormat(getFormat()) / 8;
            int index = (tile.y * (bufferSize.width / mTileSize.width + 1)) + tile.x;

            ByteBuffer buffer = mImage.getBuffer().slice();

            try {
                buffer.position(index * mTileSize.getArea() * bpp);
                buffer = buffer.slice();
                buffer.limit(mTileSize.getArea() * bpp);
            } catch (IllegalArgumentException e) {
                Log.e(LOGTAG, "Tile image-data out of bounds! Tile: (" +
                      tile.x + ", " + tile.y + "), image (" + bufferSize + ")");
                return null;
            }

            return buffer;
        }

        @Override
        public IntSize getSize() {
            return mTileSize;
        }

        @Override
        public int getFormat() {
            return mImage.getFormat();
        }
    }
}

