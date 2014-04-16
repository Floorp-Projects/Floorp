package org.mozilla.gecko.db;

import android.database.Cursor;
import android.database.CursorWrapper;
import android.util.SparseArray;

import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserDB.URLColumns;

/**
 * {@TopSitesCursorWrapper} is a cursor wrapper that merges
 * the top and pinned sites cursors into one. It ensures the
 * cursor will contain at least a given minimum number of
 * entries.
 */
public class TopSitesCursorWrapper extends CursorWrapper {

    private static class PinnedSite {
        public final String title;
        public final String url;

        public PinnedSite(String title, String url) {
            this.title = (title == null ? "" : title);
            this.url = (url == null ? "" : url);
        }
    }

    // The cursor for the top sites query
    private final Cursor topCursor;

    // Associates pinned sites and their respective positions
    private SparseArray<PinnedSite> pinnedSites;

    // Current position of the cursor
    private int currentPosition = -1;

    // The size of the cursor wrapper
    private final int count;

    public TopSitesCursorWrapper(Cursor pinnedCursor, Cursor topCursor, int minSize) {
        super(topCursor);

        setPinnedSites(pinnedCursor);
        this.topCursor = topCursor;

        count = Math.max(minSize, pinnedSites.size() + topCursor.getCount());
    }

    public void setPinnedSites(Cursor c) {
        pinnedSites = new SparseArray<PinnedSite>();

        if (c == null) {
            return;
        }

        try {
            if (c.getCount() <= 0) {
                return;
            }

            c.moveToPosition(0);
            do {
                final int pos = c.getInt(c.getColumnIndex(Bookmarks.POSITION));
                final String url = c.getString(c.getColumnIndex(URLColumns.URL));
                final String title = c.getString(c.getColumnIndex(URLColumns.TITLE));
                pinnedSites.put(pos, new PinnedSite(title, url));
            } while (c.moveToNext());
        } finally {
            c.close();
        }
    }

    public boolean hasPinnedSites() {
        return (pinnedSites != null && pinnedSites.size() > 0);
    }

    public PinnedSite getPinnedSite(int position) {
        if (!hasPinnedSites()) {
            return null;
        }

        return pinnedSites.get(position);
    }

    public boolean isPinned() {
        return (pinnedSites.get(currentPosition) != null);
    }

    private int getPinnedBefore(int position) {
        int numFound = 0;
        if (!hasPinnedSites()) {
            return numFound;
        }

        for (int i = 0; i < position; i++) {
            if (pinnedSites.get(i) != null) {
                numFound++;
            }
        }

        return numFound;
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
    public boolean move(int offset) {
        return moveToPosition(currentPosition + offset);
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
    public boolean moveToPosition(int position) {
        currentPosition = position;

        // Move the real cursor as if we were stepping through it to this position.
        // Account for pinned sites, and be careful to update its position to the
        // minimum or maximum position, even if we're moving beyond its bounds.
        final int before = getPinnedBefore(position);
        final int p2 = position - before;

        if (p2 <= -1) {
            super.moveToPosition(-1);
        } else if (p2 >= topCursor.getCount()) {
            super.moveToPosition(topCursor.getCount());
        } else {
            super.moveToPosition(p2);
        }

        return (!isBeforeFirst() && !isAfterLast());
    }

    @Override
    public long getLong(int columnIndex) {
        if (hasPinnedSites()) {
            final PinnedSite site = getPinnedSite(currentPosition);

            if (site != null) {
                return 0;
            }
        }

        if (!super.isBeforeFirst() && !super.isAfterLast()) {
            return super.getLong(columnIndex);
        }

        return 0;
    }

    @Override
    public int getInt(int columnIndex) {
        if (hasPinnedSites()) {
            final PinnedSite site = getPinnedSite(currentPosition);

            if (site != null) {
                return 0;
            }
        }

        if (!super.isBeforeFirst() && !super.isAfterLast()) {
            return super.getInt(columnIndex);
        }

        return 0;
    }

    @Override
    public String getString(int columnIndex) {
        if (hasPinnedSites()) {
            final PinnedSite site = getPinnedSite(currentPosition);

            if (site != null) {
                if (columnIndex == topCursor.getColumnIndex(URLColumns.URL)) {
                    return site.url;
                } else if (columnIndex == topCursor.getColumnIndex(URLColumns.TITLE)) {
                    return site.title;
                }

                return "";
            }
        }

        if (!super.isBeforeFirst() && !super.isAfterLast()) {
            return super.getString(columnIndex);
        }

        return "";
    }
}
