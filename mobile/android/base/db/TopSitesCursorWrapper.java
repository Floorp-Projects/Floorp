/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.db;

import java.util.HashMap;
import java.util.Map;

import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.TopSites;

import android.content.ContentResolver;
import android.database.CharArrayBuffer;
import android.database.ContentObserver;
import android.database.Cursor;
import android.database.DataSetObserver;
import android.net.Uri;
import android.os.Bundle;
import android.util.SparseBooleanArray;
import android.util.SparseIntArray;

/**
 * {@TopSitesCursorWrapper} is a cursor wrapper that merges
 * the top and pinned sites cursors into one. It ensures the
 * cursor will contain at least a given minimum number of
 * entries.
 */
public class TopSitesCursorWrapper implements Cursor {
    private enum RowType {
        UNKNOWN,
        BLANK,
        TOP,
        PINNED,
        SUGGESTED
    }

    private static final String[] columnNames = new String[] {
        TopSites._ID,
        TopSites.URL,
        TopSites.TITLE,
        TopSites.BOOKMARK_ID,
        TopSites.HISTORY_ID,
        TopSites.TYPE,
    };

    private static final Map<String, Integer> columnIndexes =
            new HashMap<String, Integer>(columnNames.length);

    static {
        for (int i = 0; i < columnNames.length; i++) {
            columnIndexes.put(columnNames[i], i);
        }
    }

    // Maps column indexes from the wrapper's to the cursor's.
    private SparseIntArray topIndexes;
    private SparseIntArray pinnedIndexes;
    private SparseIntArray suggestedIndexes;

    // Type of content in the current position.
    private RowType currentRowType;

    // Currently active cursor.
    private Cursor currentCursor;

    // The cursor for the top sites query. Never null.
    private final Cursor topCursor;

    // The cursor for the pinned sites query. Never null.
    private final Cursor pinnedCursor;

    // The cursor for the suggested sites query. Can be null.
    private final Cursor suggestedCursor;

    // Associates pinned sites and their respective positions.
    private final SparseBooleanArray pinnedPositions = new SparseBooleanArray();

    // Current position of the cursor.
    private int currentPosition = -1;

    // Number of pinned sites before the current position.
    private int pinnedBefore;

    // The size of the cursor wrapper.
    private int count;

    // The minimum size of the cursor wrapper.
    private final int minSize;

    public TopSitesCursorWrapper(Cursor pinnedCursor, Cursor topCursor, int minSize) {
        this(pinnedCursor, topCursor, null, minSize);
    }

    public TopSitesCursorWrapper(Cursor pinnedCursor, Cursor topCursor, Cursor suggestedCursor, int minSize) {
        currentRowType = RowType.UNKNOWN;

        this.minSize = minSize;

        // These must not be null.
        if (topCursor == null) {
            throw new IllegalArgumentException("topCursor is null.");
        }

        if (pinnedCursor == null) {
            throw new IllegalArgumentException("pinnedCursor is null.");
        }

        this.topCursor = topCursor;
        this.pinnedCursor = pinnedCursor;

        // Can be null.
        this.suggestedCursor = suggestedCursor;

        updateIndexMaps();
        updatePinnedPositions();
        updateCount();
    }

    private void updateIndexMaps() {
        topIndexes = new SparseIntArray(topCursor.getColumnCount());
        updateIndexMapFromCursor(topIndexes, topCursor);

        pinnedIndexes = new SparseIntArray(pinnedCursor.getColumnCount());
        updateIndexMapFromCursor(pinnedIndexes, pinnedCursor);

        if (suggestedCursor != null) {
            suggestedIndexes = new SparseIntArray(suggestedCursor.getColumnCount());
            updateIndexMapFromCursor(suggestedIndexes, suggestedCursor);
        }
    }

    private static void updateIndexMapFromCursor(SparseIntArray indexMap, Cursor c) {
        final int columnCount = c.getColumnCount();
        for (int i = 0; i < columnCount; i++) {
            final Integer index = columnIndexes.get(c.getColumnName(i));
            if (index != null) {
                indexMap.put(index, i);
            }
        }
    }

    private void updatePinnedPositions() {
        pinnedPositions.clear();

        pinnedCursor.moveToPosition(-1);
        while (pinnedCursor.moveToNext()) {
            int pos = pinnedCursor.getInt(pinnedCursor.getColumnIndex(Bookmarks.POSITION));
            pinnedPositions.put(pos, true);
        };
    }

    private void updateCount() {
        int sum = pinnedCursor.getCount() + topCursor.getCount();
        if (suggestedCursor != null) {
            sum += suggestedCursor.getCount();
        }

        count = Math.max(minSize, sum);
    }

    private static boolean cursorHasValidPosition(Cursor c) {
        return (c != null && !c.isBeforeFirst() && !c.isAfterLast());
    }

    private void updatePinnedBefore(int position) {
        pinnedBefore = 0;
        for (int i = 0; i < position; i++) {
            if (pinnedPositions.get(i)) {
                pinnedBefore++;
            }
        }
    }

    private void setCurrentCursor(Cursor cursor, int position, RowType rowType) {
        cursor.moveToPosition(position);
        currentRowType = rowType;
        currentCursor = cursor;
    }

    private boolean updateTopCursorPosition(int position) {
        final int index = position - pinnedBefore;
        if (index >= 0 && index < topCursor.getCount()) {
            setCurrentCursor(topCursor, index, RowType.TOP);
            return true;
        }

        return false;
    }

    private boolean updatePinnedCursorPosition(int position) {
        if (position >= minSize) {
            return false;
        }

        if (pinnedPositions.get(position)) {
            setCurrentCursor(pinnedCursor, pinnedPositions.indexOfKey(position), RowType.PINNED);
            return true;
        }

        return false;
    }

    private boolean updateSuggestedCursorPosition(int position) {
        if (suggestedCursor == null) {
            return false;
        }

        if (position >= minSize) {
            return false;
        }

        final int index = position - pinnedBefore - topCursor.getCount();
        if (index >= 0 && index < suggestedCursor.getCount()) {
            setCurrentCursor(suggestedCursor, index, RowType.SUGGESTED);
            return true;
        }

        return false;
    }

    private void assertValidColumnIndex(int columnIndex) {
        if (columnIndex < 0 || columnIndex > columnNames.length - 1) {
            throw new IllegalArgumentException("Column index is out of bounds: " + columnIndex);
        }
    }

    private void assertValidRowType() {
        if (currentRowType == RowType.UNKNOWN) {
            throw new IllegalStateException("No provided cursor holds data at this position");
        }
    }

    private int getColumnIndexForCurrentRowType(int columnIndex) {
        assertValidRowType();
        assertValidColumnIndex(columnIndex);

        SparseIntArray map = null;

        switch (currentRowType) {
            case TOP:
                map = topIndexes;
                break;

            case PINNED:
                map = pinnedIndexes;
                break;

            case SUGGESTED:
                map = suggestedIndexes;
                break;

            default:
                return -1;
        }

        if (map != null) {
            return map.get(columnIndex, -1);
        }

        return -1;
    }

    @Override
    public int getPosition() {
        return currentPosition;
    }

    @Override
    public int getCount() {
        return count;
    }

    @Override
    public boolean isAfterLast() {
        return (currentPosition >= count);
    }

    @Override
    public boolean isBeforeFirst() {
        return (currentPosition < 0);
    }

    @Override
    public boolean isFirst() {
        return (currentPosition == 0);
    }

    @Override
    public boolean isLast() {
        return (currentPosition == count - 1);
    }

    @Override
    public boolean moveToNext() {
        return moveToPosition(currentPosition + 1);
    }

    @Override
    public boolean moveToPrevious() {
        return moveToPosition(currentPosition - 1);
    }

    @Override
    public boolean moveToFirst() {
        return moveToPosition(0);
    }

    @Override
    public boolean moveToLast() {
        return moveToPosition(count - 1);
    }

    @Override
    public boolean move(int offset) {
        return moveToPosition(currentPosition + offset);
    }

    @Override
    public boolean moveToPosition(int position) {
        currentPosition = position;
        updatePinnedBefore(position);

        if (!updatePinnedCursorPosition(position) &&
            !updateTopCursorPosition(position) &&
            !updateSuggestedCursorPosition(position)) {

            if (position >= 0 && position < minSize) {
                currentRowType = RowType.BLANK;
            } else {
                currentRowType = RowType.UNKNOWN;
            }
            currentCursor = null;
        }

        return cursorHasValidPosition(this);
    }

    @Override
    public long getLong(int columnIndex) {
        final int index = getColumnIndexForCurrentRowType(columnIndex);
        if (index >= 0) {
            return currentCursor.getLong(index);
        }

        return 0;
    }

    @Override
    public int getInt(int columnIndex) {
        assertValidRowType();
        assertValidColumnIndex(columnIndex);

        if (columnNames[columnIndex].equals(TopSites.TYPE)) {
            switch (currentRowType) {
                case BLANK:
                    return TopSites.TYPE_BLANK;

                case TOP:
                    return TopSites.TYPE_TOP;

                case PINNED:
                    return TopSites.TYPE_PINNED;

                case SUGGESTED:
                    return TopSites.TYPE_SUGGESTED;

                default:
                    return -1;
            }
        }

        final int index = getColumnIndexForCurrentRowType(columnIndex);
        if (index >= 0) {
            return currentCursor.getInt(index);
        }

        return 0;
    }

    @Override
    public String getString(int columnIndex) {
        final int index = getColumnIndexForCurrentRowType(columnIndex);
        if (index >= 0) {
            return currentCursor.getString(index);
        }

        return "";
    }

    @Override
    public float getFloat(int columnIndex) {
        throw new UnsupportedOperationException();
    }

    @Override
    public double getDouble(int columnIndex) {
        throw new UnsupportedOperationException();
    }

    @Override
    public short getShort(int columnIndex) {
        throw new UnsupportedOperationException();
    }

    @Override
    public byte[] getBlob(int columnIndex) {
        throw new UnsupportedOperationException();
    }

    @Override
    public void copyStringToBuffer(int columnIndex, CharArrayBuffer buffer) {
        throw new UnsupportedOperationException();
    }

    @Override
    public boolean isNull(int columnIndex) {
        final int index = getColumnIndexForCurrentRowType(columnIndex);
        if (index >= 0) {
            return currentCursor.isNull(index);
        }

        return true;
    }

    @Override
    public int getType(int columnIndex) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int getColumnCount() {
        return columnNames.length;
    }

    @Override
    public int getColumnIndex(String columnName) {
        final Integer index = columnIndexes.get(columnName);
        if (index == null) {
            return -1;
        }

        return index;
    }

    @Override
    public int getColumnIndexOrThrow(String columnName) throws IllegalArgumentException {
        final int index = getColumnIndex(columnName);
        if (index < 0) {
            throw new IllegalArgumentException("Column index not found: " + columnName);
        }

        return index;
    }

    @Override
    public String getColumnName(int columnIndex) {
        return columnNames[columnIndex];
    }

    @Override
    public String[] getColumnNames() {
        return columnNames;
    }

    @SuppressWarnings("deprecation")
    @Override
    public boolean requery() {
        boolean result = topCursor.requery() && pinnedCursor.requery();
        if (suggestedCursor != null) {
            result &= suggestedCursor.requery();
        }

        updatePinnedPositions();
        updateCount();

        return result;
    }

    @Override
    public Bundle respond(Bundle extras) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Bundle getExtras() {
        throw new UnsupportedOperationException();
    }

    @Override
    public boolean getWantsAllOnMoveCalls() {
        return false;
    }

    @Override
    public Uri getNotificationUri() {
        // There's no single notification URI for the wrapper
        return null;
    }

    @Override
    public void setNotificationUri(ContentResolver cr, Uri uri) {
        // Keep the original notification URI for the
        // wrapped cursors so that we get proper change
        // notifications from the ContentResolver.
    }

    @Override
    public void registerContentObserver(ContentObserver observer) {
        topCursor.registerContentObserver(observer);
        pinnedCursor.registerContentObserver(observer);

        if (suggestedCursor != null) {
            suggestedCursor.registerContentObserver(observer);
        }
    }

    @Override
    public void unregisterContentObserver(ContentObserver observer) {
        topCursor.unregisterContentObserver(observer);
        pinnedCursor.unregisterContentObserver(observer);

        if (suggestedCursor != null) {
            suggestedCursor.unregisterContentObserver(observer);
        }
    }

    @Override
    public void registerDataSetObserver(DataSetObserver observer) {
        topCursor.registerDataSetObserver(observer);
        pinnedCursor.registerDataSetObserver(observer);

        if (suggestedCursor != null) {
            suggestedCursor.registerDataSetObserver(observer);
        }
    }

    @Override
    public void unregisterDataSetObserver(DataSetObserver observer) {
        topCursor.unregisterDataSetObserver(observer);
        pinnedCursor.unregisterDataSetObserver(observer);

        if (suggestedCursor != null) {
            suggestedCursor.unregisterDataSetObserver(observer);
        }
    }

    @SuppressWarnings("deprecation")
    @Override
    public void deactivate() {
        topCursor.deactivate();
        pinnedCursor.deactivate();

        if (suggestedCursor != null) {
            suggestedCursor.deactivate();
        }
    }

    @Override
    public boolean isClosed() {
        boolean result = topCursor.isClosed() && pinnedCursor.isClosed();

        if (suggestedCursor != null) {
            result &= suggestedCursor.isClosed();
        }

        return result;
    }

    @Override
    public void close() {
        topCursor.close();
        topIndexes = null;

        pinnedCursor.close();
        pinnedIndexes = null;
        pinnedPositions.clear();

        if (suggestedCursor != null) {
            suggestedCursor.close();
            suggestedIndexes = null;
        }
    }
}
