package org.mozilla.gecko.db;

import android.database.Cursor;
import android.database.CursorWrapper;
import android.util.SparseArray;

import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserDB.URLColumns;

/* Cursor wrapper that forces top sites to contain at least
 * mNumberOfTopSites entries. For rows outside the wrapped cursor
 * will return empty strings and zero.
 */
public class TopSitesCursorWrapper extends CursorWrapper {
    public static class PinnedSite {
        public String title = "";
        public String url = "";

        public PinnedSite(String aTitle, String aUrl) {
            title = aTitle;
            url = aUrl;
        }
    }

    int mIndex = -1; // Current position of the cursor
    Cursor mCursor = null;
    int mSize = 0;
    private SparseArray<PinnedSite> mPinnedSites = null;

    public TopSitesCursorWrapper(Cursor pinnedCursor, Cursor normalCursor, int minSize) {
        super(normalCursor);

        setPinnedSites(pinnedCursor);
        mCursor = normalCursor;
        mSize = Math.max(minSize, mPinnedSites.size() + mCursor.getCount());
    }

    public void setPinnedSites(Cursor c) {
        mPinnedSites = new SparseArray<PinnedSite>();

        if (c == null) {
            return;
        }

        try {
            if (c.getCount() <= 0) {
                return;
            }
            c.moveToPosition(0);
            do {
                int pos = c.getInt(c.getColumnIndex(Bookmarks.POSITION));
                String url = c.getString(c.getColumnIndex(URLColumns.URL));
                String title = c.getString(c.getColumnIndex(URLColumns.TITLE));
                mPinnedSites.put(pos, new PinnedSite(title, url));
            } while (c.moveToNext());
        } finally {
            c.close();
        }
    }

    public boolean hasPinnedSites() {
        return mPinnedSites != null && mPinnedSites.size() > 0;
    }

    public PinnedSite getPinnedSite(int position) {
        if (!hasPinnedSites()) {
            return null;
        }
        return mPinnedSites.get(position);
    }

    public boolean isPinned() {
        return mPinnedSites.get(mIndex) != null;
    }

    private int getPinnedBefore(int position) {
        int numFound = 0;
        if (!hasPinnedSites()) {
            return numFound;
        }

        for (int i = 0; i < position; i++) {
            if (mPinnedSites.get(i) != null) {
                numFound++;
            }
        }

        return numFound;
    }

    @Override
    public int getPosition() { return mIndex; }
    @Override
    public int getCount() { return mSize; }
    @Override
    public boolean isAfterLast() { return mIndex >= mSize; }
    @Override
    public boolean isBeforeFirst() { return mIndex < 0; }
    @Override
    public boolean isLast() { return mIndex == mSize - 1; }
    @Override
    public boolean moveToNext() { return moveToPosition(mIndex + 1); }
    @Override
    public boolean moveToPrevious() { return moveToPosition(mIndex - 1); }

    @Override
    public boolean moveToPosition(int position) {
        mIndex = position;

        // Move the real cursor as if we were stepping through it to this position.
        // Account for pinned sites, and be careful to update its position to the
        // minimum or maximum position, even if we're moving beyond its bounds.
        int before = getPinnedBefore(position);
        int p2 = position - before;
        if (p2 <= -1) {
            super.moveToPosition(-1);
        } else if (p2 >= mCursor.getCount()) {
            super.moveToPosition(mCursor.getCount());
        } else {
            super.moveToPosition(p2);
        }

        return !(isBeforeFirst() || isAfterLast());
    }

    @Override
    public long getLong(int columnIndex) {
        if (hasPinnedSites()) {
            PinnedSite site = getPinnedSite(mIndex);
            if (site != null) {
                return 0;
            }
        }

        if (!super.isBeforeFirst() && !super.isAfterLast())
            return super.getLong(columnIndex);
        return 0;
    }

    @Override
    public int getInt(int columnIndex) {
        if (hasPinnedSites()) {
            PinnedSite site = getPinnedSite(mIndex);
            if (site != null) {
                return 0;
            }
        }

        if (!super.isBeforeFirst() && !super.isAfterLast())
            return super.getInt(columnIndex);
        return 0;
    }

    @Override
    public String getString(int columnIndex) {
        if (hasPinnedSites()) {
            PinnedSite site = getPinnedSite(mIndex);
            if (site != null) {
                if (columnIndex == mCursor.getColumnIndex(URLColumns.URL)) {
                    return site.url;
                } else if (columnIndex == mCursor.getColumnIndex(URLColumns.TITLE)) {
                    return site.title;
                }
                return "";
            }
        }

        if (!super.isBeforeFirst() && !super.isAfterLast())
            return super.getString(columnIndex);
        return "";
    }

    @Override
    public boolean move(int offset) {
        return moveToPosition(mIndex + offset);
    }

    @Override
    public boolean moveToFirst() {
        return moveToPosition(0);
    }

    @Override
    public boolean moveToLast() {
        return moveToPosition(mSize-1);
    }
}
