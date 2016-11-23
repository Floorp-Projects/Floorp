/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;

import org.mozilla.gecko.Tab;

import java.util.List;

public interface TabsAccessor {
    public interface OnQueryTabsCompleteListener {
        public void onQueryTabsComplete(List<RemoteClient> clients);
    }

    public Cursor getRemoteClientsNoStaleSorted(Context context);
    public Cursor getRemoteTabsCursor(Context context);
    public Cursor getRemoteTabsCursor(Context context, int limit);
    public List<RemoteClient> getClientsWithoutTabsNoStaleSortedFromCursor(final Cursor cursor);
    public List<RemoteClient> getClientsFromCursor(final Cursor cursor);
    public void getTabs(final Context context, final OnQueryTabsCompleteListener listener);
    public void getTabs(final Context context, final int limit, final OnQueryTabsCompleteListener listener);
    public void persistLocalTabs(final ContentResolver cr, final Iterable<Tab> tabs);
}
