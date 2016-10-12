/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.testhelpers;

import android.content.Context;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.NoStoreDelegateException;
import org.mozilla.gecko.sync.repositories.RecordFilter;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.StoreTrackingRepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionGuidsSinceDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

import java.util.Map.Entry;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class WBORepository extends Repository {

  public class WBORepositoryStats {
    public long created         = -1;
    public long begun           = -1;
    public long fetchBegan      = -1;
    public long fetchCompleted  = -1;
    public long storeBegan      = -1;
    public long storeCompleted  = -1;
    public long finished        = -1;
  }

  public static final String LOG_TAG = "WBORepository";

  // Access to stats is not guarded.
  public WBORepositoryStats stats;

  // Whether or not to increment the timestamp of stored records.
  public final boolean bumpTimestamps;

  public class WBORepositorySession extends StoreTrackingRepositorySession {

    protected WBORepository wboRepository;
    protected ExecutorService delegateExecutor = Executors.newSingleThreadExecutor();
    public ConcurrentHashMap<String, Record> wbos;

    public WBORepositorySession(WBORepository repository) {
      super(repository);

      wboRepository = repository;
      wbos          = new ConcurrentHashMap<String, Record>();
      stats         = new WBORepositoryStats();
      stats.created = now();
    }

    @Override
    protected synchronized void trackGUID(String guid) {
      if (wboRepository.shouldTrack()) {
        super.trackGUID(guid);
      }
    }

    @Override
    public void guidsSince(long timestamp,
                           RepositorySessionGuidsSinceDelegate delegate) {
      throw new RuntimeException("guidsSince not implemented.");
    }

    @Override
    public void fetchSince(long timestamp,
                           RepositorySessionFetchRecordsDelegate delegate) {
      long fetchBegan  = now();
      stats.fetchBegan = fetchBegan;
      RecordFilter filter = storeTracker.getFilter();

      for (Entry<String, Record> entry : wbos.entrySet()) {
        Record record = entry.getValue();
        if (record.lastModified >= timestamp) {
          if (filter != null &&
              filter.excludeRecord(record)) {
            Logger.debug(LOG_TAG, "Excluding record " + record.guid);
            continue;
          }
          delegate.deferredFetchDelegate(delegateExecutor).onFetchedRecord(record);
        }
      }
      long fetchCompleted  = now();
      stats.fetchCompleted = fetchCompleted;
      delegate.deferredFetchDelegate(delegateExecutor).onFetchCompleted(fetchCompleted);
    }

    @Override
    public void fetch(final String[] guids,
                      final RepositorySessionFetchRecordsDelegate delegate) {
      long fetchBegan  = now();
      stats.fetchBegan = fetchBegan;
      for (String guid : guids) {
        if (wbos.containsKey(guid)) {
          delegate.deferredFetchDelegate(delegateExecutor).onFetchedRecord(wbos.get(guid));
        }
      }
      long fetchCompleted  = now();
      stats.fetchCompleted = fetchCompleted;
      delegate.deferredFetchDelegate(delegateExecutor).onFetchCompleted(fetchCompleted);
    }

    @Override
    public void fetchAll(final RepositorySessionFetchRecordsDelegate delegate) {
      long fetchBegan  = now();
      stats.fetchBegan = fetchBegan;
      for (Entry<String, Record> entry : wbos.entrySet()) {
        Record record = entry.getValue();
        delegate.deferredFetchDelegate(delegateExecutor).onFetchedRecord(record);
      }
      long fetchCompleted  = now();
      stats.fetchCompleted = fetchCompleted;
      delegate.deferredFetchDelegate(delegateExecutor).onFetchCompleted(fetchCompleted);
    }

    @Override
    public void store(final Record record) throws NoStoreDelegateException {
      if (storeDelegate == null) {
        throw new NoStoreDelegateException();
      }
      final long now = now();
      if (stats.storeBegan < 0) {
        stats.storeBegan = now;
      }
      Record existing = wbos.get(record.guid);
      Logger.debug(LOG_TAG, "Existing record is " + (existing == null ? "<null>" : (existing.guid + ", " + existing)));
      if (existing != null &&
          existing.lastModified > record.lastModified) {
        Logger.debug(LOG_TAG, "Local record is newer. Not storing.");
        storeDelegate.deferredStoreDelegate(delegateExecutor).onRecordStoreSucceeded(record.guid);
        return;
      }
      if (existing != null) {
        Logger.debug(LOG_TAG, "Replacing local record.");
      }

      // Store a copy of the record with an updated modified time.
      Record toStore = record.copyWithIDs(record.guid, record.androidID);
      if (bumpTimestamps) {
        toStore.lastModified = now;
      }
      wbos.put(record.guid, toStore);

      trackRecord(toStore);
      storeDelegate.deferredStoreDelegate(delegateExecutor).onRecordStoreSucceeded(record.guid);
    }

    @Override
    public void wipe(final RepositorySessionWipeDelegate delegate) {
      if (!isActive()) {
        delegate.onWipeFailed(new InactiveSessionException());
        return;
      }

      Logger.info(LOG_TAG, "Wiping WBORepositorySession.");
      this.wbos = new ConcurrentHashMap<String, Record>();

      // Wipe immediately for the convenience of test code.
      wboRepository.wbos = new ConcurrentHashMap<String, Record>();
      delegate.deferredWipeDelegate(delegateExecutor).onWipeSucceeded();
    }

    @Override
    public void finish(RepositorySessionFinishDelegate delegate) throws InactiveSessionException {
      Logger.info(LOG_TAG, "Finishing WBORepositorySession: handing back " + this.wbos.size() + " WBOs.");
      wboRepository.wbos = this.wbos;
      stats.finished = now();
      super.finish(delegate);
    }

    @Override
    public void begin(RepositorySessionBeginDelegate delegate) throws InvalidSessionTransitionException {
      this.wbos = wboRepository.cloneWBOs();
      stats.begun = now();
      super.begin(delegate);
    }

    @Override
    public void storeDone(long end) {
      // TODO: this is not guaranteed to be called after all of the record
      // store callbacks have completed!
      if (stats.storeBegan < 0) {
        stats.storeBegan = end;
      }
      stats.storeCompleted = end;
      storeDelegate.deferredStoreDelegate(delegateExecutor).onStoreCompleted(end);
    }
  }

  public ConcurrentHashMap<String, Record> wbos;

  public WBORepository(boolean bumpTimestamps) {
    super();
    this.bumpTimestamps = bumpTimestamps;
    this.wbos = new ConcurrentHashMap<String, Record>();
  }

  public WBORepository() {
    this(false);
  }

  public synchronized boolean shouldTrack() {
    return false;
  }

  @Override
  public void createSession(RepositorySessionCreationDelegate delegate,
                            Context context) {
    delegate.deferredCreationDelegate().onSessionCreated(new WBORepositorySession(this));
  }

  public ConcurrentHashMap<String, Record> cloneWBOs() {
    ConcurrentHashMap<String, Record> out = new ConcurrentHashMap<String, Record>();
    for (Entry<String, Record> entry : wbos.entrySet()) {
      out.put(entry.getKey(), entry.getValue()); // Assume that records are
                                                 // immutable.
    }
    return out;
  }
}
