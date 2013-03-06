/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import java.util.ArrayList;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.db.Tab;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.NoContentProviderException;
import org.mozilla.gecko.sync.repositories.NoStoreDelegateException;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionGuidsSinceDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;
import org.mozilla.gecko.sync.repositories.domain.TabsRecord;

import android.content.ContentProviderClient;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.RemoteException;

public class FennecTabsRepository extends Repository {
  protected final String localClientName;
  protected final String localClientGuid;

  public FennecTabsRepository(final String localClientName, final String localClientGuid) {
    this.localClientName = localClientName;
    this.localClientGuid = localClientGuid;
  }

  /**
   * Note that — unlike most repositories — this will only fetch Fennec's tabs,
   * and only store tabs from other clients.
   *
   * It will never retrieve tabs from other clients, or store tabs for Fennec,
   * unless you use {@link #fetch(String[], RepositorySessionFetchRecordsDelegate)}
   * and specify an explicit GUID.
   */
  public class FennecTabsRepositorySession extends RepositorySession {
    protected static final String LOG_TAG = "FennecTabsSession";

    private final ContentProviderClient tabsProvider;
    private final ContentProviderClient clientsProvider;

    protected final RepoUtils.QueryHelper tabsHelper;

    protected ContentProviderClient getContentProvider(final Context context, final Uri uri) throws NoContentProviderException {
      ContentProviderClient client = context.getContentResolver().acquireContentProviderClient(uri);
      if (client == null) {
        throw new NoContentProviderException(uri);
      }
      return client;
    }

    protected void releaseProviders() {
      try {
        clientsProvider.release();
      } catch (Exception e) {}
      try {
        tabsProvider.release();
      } catch (Exception e) {}
    }

    public FennecTabsRepositorySession(Repository repository, Context context) throws NoContentProviderException {
      super(repository);
      clientsProvider = getContentProvider(context, BrowserContract.Clients.CONTENT_URI);
      try {
        tabsProvider = getContentProvider(context, BrowserContract.Tabs.CONTENT_URI);
      } catch (NoContentProviderException e) {
        clientsProvider.release();
        throw e;
      } catch (Exception e) {
        clientsProvider.release();
        // Oh, Java.
        throw new RuntimeException(e);
      }

      tabsHelper = new RepoUtils.QueryHelper(context, BrowserContract.Tabs.CONTENT_URI, LOG_TAG);
    }

    @Override
    public void abort() {
      releaseProviders();
      super.abort();
    }

    @Override
    public void finish(final RepositorySessionFinishDelegate delegate) throws InactiveSessionException {
      releaseProviders();
      super.finish(delegate);
    }

    // Default parameters for local data: local client has null GUID. Override
    // these to test against non-live data.
    protected String localClientSelection() {
      return BrowserContract.Tabs.CLIENT_GUID + " IS NULL";
    }

    protected String[] localClientSelectionArgs() {
      return null;
    }

    @Override
    public void guidsSince(final long timestamp,
                           final RepositorySessionGuidsSinceDelegate delegate) {
      // Bug 783692: Now that Bug 730039 has landed, we could implement this,
      // but it's not a priority since it's not used (yet).
      Logger.warn(LOG_TAG, "Not returning anything from guidsSince.");
      delegateQueue.execute(new Runnable() {
        @Override
        public void run() {
          delegate.onGuidsSinceSucceeded(new String[] {});
        }
      });
    }

    @Override
    public void fetchSince(final long timestamp,
                           final RepositorySessionFetchRecordsDelegate delegate) {
      if (tabsProvider == null) {
        throw new IllegalArgumentException("tabsProvider was null.");
      }
      if (tabsHelper == null) {
        throw new IllegalArgumentException("tabsHelper was null.");
      }

      final String positionAscending = BrowserContract.Tabs.POSITION + " ASC";

      final String localClientSelection = localClientSelection();
      final String[] localClientSelectionArgs = localClientSelectionArgs();

      final Runnable command = new Runnable() {
        @Override
        public void run() {
          // We fetch all local tabs (since the record must contain them all)
          // but only process the record if the timestamp is sufficiently
          // recent.
          try {
            final Cursor cursor = tabsHelper.safeQuery(tabsProvider, ".fetchSince()", null,
                localClientSelection, localClientSelectionArgs, positionAscending);
            try {
              final TabsRecord tabsRecord = FennecTabsRepository.tabsRecordFromCursor(cursor, localClientGuid, localClientName);

              if (tabsRecord.lastModified >= timestamp) {
                delegate.onFetchedRecord(tabsRecord);
              }
            } finally {
              cursor.close();
            }
          } catch (Exception e) {
            delegate.onFetchFailed(e, null);
            return;
          }
          delegate.onFetchCompleted(now());
        }
      };

      delegateQueue.execute(command);
    }

    @Override
    public void fetch(final String[] guids,
                      final RepositorySessionFetchRecordsDelegate delegate) {
      // Bug 783692: Now that Bug 730039 has landed, we could implement this,
      // but it's not a priority since it's not used (yet).
      Logger.warn(LOG_TAG, "Not returning anything from fetch");
      delegateQueue.execute(new Runnable() {
        @Override
        public void run() {
          delegate.onFetchCompleted(now());
        }
      });
    }

    @Override
    public void fetchAll(final RepositorySessionFetchRecordsDelegate delegate) {
      fetchSince(0, delegate);
    }

    private static final String TABS_CLIENT_GUID_IS = BrowserContract.Tabs.CLIENT_GUID + " = ?";
    private static final String CLIENT_GUID_IS = BrowserContract.Clients.GUID + " = ?";

    @Override
    public void store(final Record record) throws NoStoreDelegateException {
      if (delegate == null) {
        Logger.warn(LOG_TAG, "No store delegate.");
        throw new NoStoreDelegateException();
      }
      if (record == null) {
        Logger.error(LOG_TAG, "Record sent to store was null");
        throw new IllegalArgumentException("Null record passed to FennecTabsRepositorySession.store().");
      }
      if (!(record instanceof TabsRecord)) {
        Logger.error(LOG_TAG, "Can't store anything but a TabsRecord");
        throw new IllegalArgumentException("Non-TabsRecord passed to FennecTabsRepositorySession.store().");
      }
      final TabsRecord tabsRecord = (TabsRecord) record;

      Runnable command = new Runnable() {
        @Override
        public void run() {
          Logger.debug(LOG_TAG, "Storing tabs for client " + tabsRecord.guid);
          if (!isActive()) {
            delegate.onRecordStoreFailed(new InactiveSessionException(null), record.guid);
            return;
          }
          if (tabsRecord.guid == null) {
            delegate.onRecordStoreFailed(new RuntimeException("Can't store record with null GUID."), record.guid);
            return;
          }

          try {
            // This is nice and easy: we *always* store.
            final String[] selectionArgs = new String[] { tabsRecord.guid };
            if (tabsRecord.deleted) {
              try {
                Logger.debug(LOG_TAG, "Clearing entry for client " + tabsRecord.guid);
                clientsProvider.delete(BrowserContract.Clients.CONTENT_URI,
                                       CLIENT_GUID_IS,
                                       selectionArgs);
                delegate.onRecordStoreSucceeded(record.guid);
              } catch (Exception e) {
                delegate.onRecordStoreFailed(e, record.guid);
              }
              return;
            }

            // If it exists, update the client record; otherwise insert.
            final ContentValues clientsCV = tabsRecord.getClientsContentValues();

            Logger.debug(LOG_TAG, "Updating clients provider.");
            final int updated = clientsProvider.update(BrowserContract.Clients.CONTENT_URI,
                clientsCV,
                CLIENT_GUID_IS,
                selectionArgs);
            if (0 == updated) {
              clientsProvider.insert(BrowserContract.Clients.CONTENT_URI, clientsCV);
            }

            // Now insert tabs.
            final ContentValues[] tabsArray = tabsRecord.getTabsContentValues();
            Logger.debug(LOG_TAG, "Inserting " + tabsArray.length + " tabs for client " + tabsRecord.guid);

            tabsProvider.delete(BrowserContract.Tabs.CONTENT_URI, TABS_CLIENT_GUID_IS, selectionArgs);
            final int inserted = tabsProvider.bulkInsert(BrowserContract.Tabs.CONTENT_URI, tabsArray);
            Logger.trace(LOG_TAG, "Inserted: " + inserted);

            delegate.onRecordStoreSucceeded(record.guid);
          } catch (Exception e) {
            Logger.warn(LOG_TAG, "Error storing tabs.", e);
            delegate.onRecordStoreFailed(e, record.guid);
          }
        }
      };

      storeWorkQueue.execute(command);
    }

    @Override
    public void wipe(RepositorySessionWipeDelegate delegate) {
      try {
        tabsProvider.delete(BrowserContract.Tabs.CONTENT_URI, null, null);
        clientsProvider.delete(BrowserContract.Clients.CONTENT_URI, null, null);
      } catch (RemoteException e) {
        Logger.warn(LOG_TAG, "Got RemoteException in wipe.", e);
        delegate.onWipeFailed(e);
        return;
      }
      delegate.onWipeSucceeded();
    }
  }

  @Override
  public void createSession(RepositorySessionCreationDelegate delegate,
                            Context context) {
    try {
      final FennecTabsRepositorySession session = new FennecTabsRepositorySession(this, context);
      delegate.onSessionCreated(session);
    } catch (Exception e) {
      delegate.onSessionCreateFailed(e);
    }
  }

  /**
   * Extract a <code>TabsRecord</code> from a cursor.
   * <p>
   * Caller is responsible for creating and closing cursor. Each row of the
   * cursor should be an individual tab record.
   * <p>
   * The extracted tabs record has the given client GUID and client name.
   *
   * @param cursor
   *          to inspect.
   * @param clientGuid
   *          returned tabs record will have this client GUID.
   * @param clientName
   *          returned tabs record will have this client name.
   * @return <code>TabsRecord</code> instance.
   */
  public static TabsRecord tabsRecordFromCursor(final Cursor cursor, final String clientGuid, final String clientName) {
    final String collection = "tabs";
    final TabsRecord record = new TabsRecord(clientGuid, collection, 0, false);
    record.tabs = new ArrayList<Tab>();
    record.clientName = clientName;

    record.androidID = -1;
    record.deleted = false;

    record.lastModified = 0;

    int position = cursor.getPosition();
    try {
      cursor.moveToFirst();
      while (!cursor.isAfterLast()) {
        final Tab tab = Tab.fromCursor(cursor);
        record.tabs.add(tab);

        if (tab.lastUsed > record.lastModified) {
          record.lastModified = tab.lastUsed;
        }

        cursor.moveToNext();
      }
    } finally {
      cursor.moveToPosition(position);
    }

    return record;
  }
}
