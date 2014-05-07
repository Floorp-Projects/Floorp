package org.mozilla.gecko.db;

import android.content.ContentResolver;
import android.database.CharArrayBuffer;
import android.database.ContentObserver;
import android.database.Cursor;
import android.database.DataSetObserver;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.util.ArrayMap;
import android.util.SparseBooleanArray;
import android.util.SparseIntArray;

import java.util.ArrayList;
import java.util.List;

import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.TopSites;
import org.mozilla.gecko.db.BrowserDB.URLColumns;

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
        PINNED
    }

    private static final String[] columnNames = new String[] {
        TopSites._ID,
        TopSites.URL,
        TopSites.TITLE,
        TopSites.BOOKMARK_ID,
        TopSites.HISTORY_ID,
        TopSites.DISPLAY,
        TopSites.TYPE
    };

    private static final ArrayMap<String, Integer> columnIndexes =
            new ArrayMap<String, Integer>(columnNames.length) {{
        for (int i = 0; i < columnNames.length; i++) {
            put(columnNames[i], i);
        }
    }};

    // Maps column indexes from the wrapper to the cursor's.
    private SparseIntArray topIndexes;
    private SparseIntArray pinnedIndexes;

    // Type of content in the current position
    private RowType currentRowType;

    // Currently active cursor
    private Cursor currentCursor;

    // The cursor for the top sites query
    private final Cursor topCursor;

    // The cursor for the pinned sites query
    private final Cursor pinnedCursor;

    // Associates pinned sites and their respective positions
    private SparseBooleanArray pinnedPositions;

    // Current position of the cursor
    private int currentPosition = -1;

    // The size of the cursor wrapper
    private int count;

    // The minimum size of the cursor wrapper
    private final int minSize;

    public TopSitesCursorWrapper(Cursor pinnedCursor, Cursor topCursor, int minSize) {
        currentRowType = RowType.UNKNOWN;

        this.minSize = minSize;
        this.topCursor = topCursor;
        this.pinnedCursor = pinnedCursor;

        updateIndexMaps();
        updatePinnedPositions();
        updateCount();
    }

    private void updateIndexMaps() {
        topIndexes = new SparseIntArray(topCursor.getColumnCount());
        updateIndexMapFromCursor(topIndexes, topCursor);

        pinnedIndexes = new SparseIntArray(pinnedCursor.getColumnCount());
        updateIndexMapFromCursor(pinnedIndexes, pinnedCursor);
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
        if (pinnedPositions == null) {
            pinnedPositions = new SparseBooleanArray();
        } else {
            pinnedPositions.clear();
        }

        pinnedCursor.moveToPosition(-1);
        while (pinnedCursor.moveToNext()) {
            int pos = pinnedCursor.getInt(pinnedCursor.getColumnIndex(Bookmarks.POSITION));
            pinnedPositions.put(pos, true);
        };
    }

    private void updateCount() {
        count = Math.max(minSize, pinnedCursor.getCount() + topCursor.getCount());
    }

    private static boolean cursorHasValidPosition(Cursor c) {
        return (!c.isBeforeFirst() && !c.isAfterLast());
    }

    private void updateRowState() {
        if (cursorHasValidPosition(pinnedCursor)) {
            currentRowType = RowType.PINNED;
            currentCursor = pinnedCursor;
        } else if (cursorHasValidPosition(topCursor)) {
            currentRowType = RowType.TOP;
            currentCursor = topCursor;
        } else if (currentPosition >= 0 && currentPosition < minSize) {
            currentRowType = RowType.BLANK;
            currentCursor = null;
        } else {
            currentRowType = RowType.UNKNOWN;
            currentCursor = null;
        }
    }

    private int getPinnedBefore(int position) {
        int numFound = 0;
        for (int i = 0; i < position; i++) {
            if (pinnedPositions.get(i)) {
                numFound++;
            }
        }

        return numFound;
    }

    private void updateTopCursorPosition(int position) {
        // Move the real cursor as if we were stepping through it to this position.
        // Account for pinned sites, and be careful to update its position to the
        // minimum or maximum position, even if we're moving beyond its bounds.
        final int pinnedBefore = getPinnedBefore(position);
        final int actualPosition = position - pinnedBefore;

        if (actualPosition <= -1) {
            topCursor.moveToPosition(-1);
        } else if (actualPosition >= topCursor.getCount()) {
            topCursor.moveToPosition(topCursor.getCount());
        } else {
            topCursor.moveToPosition(actualPosition);
        }
    }

    private void updatePinnedCursorPosition(int position) {
        if (pinnedPositions.get(position)) {
            pinnedCursor.moveToPosition(pinnedPositions.indexOfKey(position));
        } else {
            pinnedCursor.moveToPosition(-1);
        }
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

        updatePinnedCursorPosition(position);
        updateTopCursorPosition(position);
        updateRowState();

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

    @Override
    public boolean requery() {
        boolean result = topCursor.requery() && pinnedCursor.requery();

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
    public void setNotificationUri(ContentResolver cr, Uri uri) {
        // Keep the original notification URI for the
        // wrapped cursors so that we get proper change
        // notifications from the ContentResolver.
    }

    @Override
    public void registerContentObserver(ContentObserver observer) {
        topCursor.registerContentObserver(observer);
        pinnedCursor.registerContentObserver(observer);
    }

    @Override
    public void unregisterContentObserver(ContentObserver observer) {
        topCursor.unregisterContentObserver(observer);
        pinnedCursor.unregisterContentObserver(observer);
    }

    @Override
    public void registerDataSetObserver(DataSetObserver observer) {
        topCursor.registerDataSetObserver(observer);
        pinnedCursor.registerDataSetObserver(observer);
    }

    @Override
    public void unregisterDataSetObserver(DataSetObserver observer) {
        topCursor.unregisterDataSetObserver(observer);
        pinnedCursor.unregisterDataSetObserver(observer);
    }

    @Override
    public void deactivate() {
        topCursor.deactivate();
        pinnedCursor.deactivate();
    }

    @Override
    public boolean isClosed() {
        return topCursor.isClosed() && pinnedCursor.isClosed();
    }

    @Override
    public void close() {
        topCursor.close();
        topIndexes = null;

        pinnedCursor.close();
        pinnedIndexes = null;
        pinnedPositions = null;
    }
}
