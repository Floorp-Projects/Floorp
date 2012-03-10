/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.Logger;
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
import android.net.Uri;
import android.os.RemoteException;

public class FennecTabsRepository extends Repository {

  /**
   * Note that — unlike most repositories — this will only fetch Fennec's tabs,
   * and only store tabs from other clients.
   *
   * It will never retrieve tabs from other clients, or store tabs for Fennec,
   * unless you use {@link fetch(String[], RepositorySessionFetchRecordsDelegate)}
   * and specify an explicit GUID.
   */
  public static class FennecTabsRepositorySession extends RepositorySession {

    private static final String LOG_TAG = "FennecTabsSession";

    private final ContentProviderClient tabsProvider;
    private final ContentProviderClient clientsProvider;

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

    @Override
    public void guidsSince(long timestamp,
                           RepositorySessionGuidsSinceDelegate delegate) {
      // Empty until Bug 730039 lands.
      delegate.onGuidsSinceSucceeded(new String[] {});
    }

    @Override
    public void fetchSince(long timestamp,
                           RepositorySessionFetchRecordsDelegate delegate) {
      // Empty until Bug 730039 lands.
      delegate.onFetchCompleted(now());
    }

    @Override
    public void fetch(String[] guids,
                      RepositorySessionFetchRecordsDelegate delegate) {
      // Incomplete until Bug 730039 lands.
      // TODO
      delegate.onFetchCompleted(now());
    }

    @Override
    public void fetchAll(RepositorySessionFetchRecordsDelegate delegate) {
      // Incomplete until Bug 730039 lands.
      // TODO
      delegate.onFetchCompleted(now());
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
            delegate.onRecordStoreFailed(new InactiveSessionException(null));
            return;
          }
          if (tabsRecord.guid == null) {
            delegate.onRecordStoreFailed(new RuntimeException("Can't store record with null GUID."));
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
                delegate.onRecordStoreSucceeded(record);
              } catch (Exception e) {
                delegate.onRecordStoreFailed(e);
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

            delegate.onRecordStoreSucceeded(tabsRecord);
          } catch (Exception e) {
            Logger.warn(LOG_TAG, "Error storing tabs.", e);
            delegate.onRecordStoreFailed(e);
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
}
