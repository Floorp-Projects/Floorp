/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.NoStoreDelegateException;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.ProfileDatabaseException;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.StoreTrackingRepositorySession;
import org.mozilla.gecko.sync.repositories.VersioningDelegateHelper;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.Context;

public class BookmarksRepositorySession extends StoreTrackingRepositorySession {

  private static final String LOG_TAG = "AndroidBrowserBookmarksRepositorySession";

  private int storeCount = 0;

  /**
   * Some notes on reparenting/reordering.
   *
   * Fennec stores new items with a high-negative position, because it doesn't care.
   * On the other hand, it also doesn't give us any help managing positions.
   *
   * We can process records and folders in any order, though we'll usually see folders
   * first because their sortindex is larger.
   *
   * We can also see folders that refer to children we haven't seen, and children we
   * won't see (perhaps due to a TTL, perhaps due to a limit on our fetch).
   *
   * And of course folders can refer to local children (including ones that might
   * be reconciled into oblivion!), or local children in other folders. And the local
   * version of a folder -- which might be a reconciling target, or might not -- can
   * have local additions or removals. (That causes complications with on-the-fly
   * reordering: we don't know in advance which records will even exist by the end
   * of the sync.)
   *
   * We opt to leave records in a reasonable state as we go, applying reordering/
   * reparenting operations whenever possible. A final sequence is applied after all
   * incoming records have been handled.
   *
   * As such, we need to track a bunch of stuff as we go:
   *
   * • For each downloaded folder, the array of children. These will be server GUIDs,
   *   but not necessarily identical to the remote list: if we download a record and
   *   it's been locally moved, it must be removed from this child array.
   *
   *   This mapping can be discarded when final reordering has occurred, either on
   *   store completion or when every child has been seen within this session.
   *
   * • A list of orphans: records whose parent folder does not yet exist. This can be
   *   trimmed as orphans are reparented.
   *
   * • Mappings from folder GUIDs to folder IDs, so that we can parent items without
   *   having to look in the DB. Of course, this must be kept up-to-date as we
   *   reconcile.
   *
   * Reordering also needs to occur during fetch. That is, a folder might have been
   * created locally, or modified locally without any remote changes. An order must
   * be generated for the folder's children array, and it must be persisted into the
   * database to act as a starting point for future changes. But of course we don't
   * want to incur a database write if the children already have a satisfactory order.
   *
   * Do we also need a list of "adopters", parents that are still waiting for children?
   * As items get picked out of the orphans list, we can do on-the-fly ordering, until
   * we're left with lonely records at the end.
   *
   * As we modify local folders, perhaps by moving children out of their purview, we
   * must bump their modification time so as to cause them to be uploaded on the next
   * stage of syncing. The same applies to simple reordering.
   */
  private final BookmarksDataAccessor dataAccessor;

  private final BookmarksSessionHelper sessionHelper;

  private final VersioningDelegateHelper versioningDelegateHelper;

  /**
   * An array of known-special GUIDs.
   */
  public static final String[] SPECIAL_GUIDS = new String[] {
    // Mobile and desktop places roots have to come first.
    "places",
    "mobile",
    "toolbar",
    "menu",
    "unfiled"
  };


  /**
   * A map of guids to their localized name strings.
   */
  // Oh, if only we could make this final and initialize it in the static initializer.
  public static Map<String, String> SPECIAL_GUIDS_MAP;

  protected BookmarksRepositorySession(Repository repository, Context context) {
    super(repository);

    if (SPECIAL_GUIDS_MAP == null) {
      HashMap<String, String> m = new HashMap<String, String>();

      // Note that we always use the literal name "mobile" for the Mobile Bookmarks
      // folder, regardless of its actual name in the database or the Fennec UI.
      // This is to match desktop (working around Bug 747699) and to avoid a similar
      // issue locally. See Bug 748898.
      m.put("mobile",  "mobile");

      // Other folders use their contextualized names, and we simply rely on
      // these not changing, matching desktop, and such to avoid issues.
      m.put("menu",    context.getString(R.string.bookmarks_folder_menu));
      m.put("places",  context.getString(R.string.bookmarks_folder_places));
      m.put("toolbar", context.getString(R.string.bookmarks_folder_toolbar));
      m.put("unfiled", context.getString(R.string.bookmarks_folder_unfiled));

      SPECIAL_GUIDS_MAP = Collections.unmodifiableMap(m);
    }

    dataAccessor = new BookmarksDataAccessor(context);
    sessionHelper = new BookmarksSessionHelper(this, dataAccessor);
    versioningDelegateHelper = new VersioningDelegateHelper(context, BrowserContractHelpers.BOOKMARKS_CONTENT_URI);
  }

  @Override
  public void fetchModified(RepositorySessionFetchRecordsDelegate delegate) {
    if (this.storeTracker == null) {
      throw new IllegalStateException("Store tracker not yet initialized!");
    }

    Logger.debug(LOG_TAG, "Running fetchModified.");
    delegateQueue.execute(
            sessionHelper.getFetchModifiedRunnable(
                    now(),
                    this.storeTracker.getFilter(),
                    versioningDelegateHelper.getFetchDelegate(delegate)
            )
    );
  }

  @Override
  public void fetch(String[] guids, RepositorySessionFetchRecordsDelegate delegate) throws InactiveSessionException {
    executeDelegateCommand(sessionHelper.getFetchRunnable(
            guids, now(), null, versioningDelegateHelper.getFetchDelegate(delegate))
    );
  }

  @Override
  public void fetchAll(RepositorySessionFetchRecordsDelegate delegate) {
    if (this.storeTracker == null) {
      throw new IllegalStateException("Store tracker not yet initialized!");
    }

    final long since = -1L;

    Logger.debug(LOG_TAG, "Running fetchSince(" + since + ").");
    delegateQueue.execute(
            sessionHelper.getFetchSinceRunnable(
                    since,
                    now(),
                    this.storeTracker.getFilter(),
                    versioningDelegateHelper.getFetchDelegate(delegate)
            )
    );
  }

  @Override
  public void setStoreDelegate(RepositorySessionStoreDelegate delegate) {
    super.setStoreDelegate(versioningDelegateHelper.getStoreDelegate(delegate));
  }

  @Override
  public void store(Record record) throws NoStoreDelegateException {
    if (storeDelegate == null) {
      throw new NoStoreDelegateException();
    }
    if (record == null) {
      Logger.error(LOG_TAG, "Record sent to store was null");
      throw new IllegalArgumentException("Null record passed to AndroidBrowserRepositorySession.store().");
    }

    storeCount += 1;
    Logger.debug(LOG_TAG, "Storing record with GUID " + record.guid + " (stored " + storeCount + " records this session).");

    // Store Runnables *must* complete synchronously. It's OK, they
    // run on a background thread.
    storeWorkQueue.execute(sessionHelper.getStoreRunnable(record, storeDelegate));
  }

  @Override
  public void wipe(RepositorySessionWipeDelegate delegate) {
    Runnable command = sessionHelper.getWipeRunnable(delegate);
    storeWorkQueue.execute(command);
  }

  @Override
  public void performCleanup() {
    versioningDelegateHelper.persistSyncVersions();
    super.performCleanup();
  }

  @Override
  public void begin(RepositorySessionBeginDelegate delegate) throws InvalidSessionTransitionException {
    // Check for the existence of special folders
    // and insert them if they don't exist.
    try {
      Logger.debug(LOG_TAG, "Check and build special GUIDs.");
      dataAccessor.checkAndBuildSpecialGuids();
      Logger.debug(LOG_TAG, "Got GUIDs for folders.");
    } catch (android.database.sqlite.SQLiteConstraintException e) {
      Logger.error(LOG_TAG, "Got sqlite constraint exception working with Fennec bookmark DB.", e);
      delegate.onBeginFailed(e);
      return;
    } catch (Exception e) {
      delegate.onBeginFailed(e);
      return;
    }

    try {
      sessionHelper.doBegin();
    } catch (NullCursorException e) {
      delegate.onBeginFailed(e);
      return;
    }

    RepositorySessionBeginDelegate deferredDelegate = delegate.deferredBeginDelegate(delegateQueue);
    super.sharedBegin();

    try {
      // We do this check here even though it results in one extra call to the DB
      // because if we didn't, we have to do a check on every other call since there
      // is no way of knowing which call would be hit first.
      sessionHelper.checkDatabase();
    } catch (ProfileDatabaseException e) {
      Logger.error(LOG_TAG, "ProfileDatabaseException from begin. Fennec must be launched once until this error is fixed");
      deferredDelegate.onBeginFailed(e);
      return;
    } catch (Exception e) {
      deferredDelegate.onBeginFailed(e);
      return;
    }
    storeTracker = createStoreTracker();
    deferredDelegate.onBeginSucceeded(this);
  }

  @Override
  public void finish(RepositorySessionFinishDelegate delegate) throws InactiveSessionException {
    sessionHelper.finish();
    super.finish(delegate);
  };

  @Override
  public void storeDone() {
    storeWorkQueue.execute(sessionHelper.getStoreDoneRunnable(storeDelegate));
    // Work queue is single-threaded, and so this should be well-ordered - onStoreComplete will run
    // after the above runnable is finished.
    storeWorkQueue.execute(new Runnable() {
      @Override
      public void run() {
        setLastStoreTimestamp(now());
        storeDelegate.onStoreCompleted();
      }
    });
  }
}