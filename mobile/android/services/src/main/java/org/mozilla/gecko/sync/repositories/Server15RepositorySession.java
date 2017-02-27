/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import android.net.Uri;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionGuidsSinceDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;
import org.mozilla.gecko.sync.repositories.downloaders.BatchingDownloader;
import org.mozilla.gecko.sync.repositories.downloaders.BatchingDownloaderController;
import org.mozilla.gecko.sync.repositories.uploaders.BatchingUploader;

public class Server15RepositorySession extends RepositorySession {
  public static final String LOG_TAG = "Server15RepositorySession";

  protected final Server15Repository serverRepository;
  private BatchingUploader uploader;
  private final BatchingDownloader downloader;

  public Server15RepositorySession(Repository repository) {
    super(repository);
    this.serverRepository = (Server15Repository) repository;
    this.downloader = new BatchingDownloader(
            this.serverRepository.authHeaderProvider,
            Uri.parse(this.serverRepository.collectionURI().toString()),
            this.serverRepository.getSyncDeadline(),
            this.serverRepository.getAllowMultipleBatches(),
            this.serverRepository.getAllowHighWaterMark(),
            this.serverRepository.stateProvider,
            this);
  }

  @Override
  public void setStoreDelegate(RepositorySessionStoreDelegate storeDelegate) {
    super.setStoreDelegate(storeDelegate);

    // Now that we have the delegate, we can initialize our uploader.
    this.uploader = new BatchingUploader(
            this, storeWorkQueue, storeDelegate, Uri.parse(serverRepository.collectionURI.toString()),
            serverRepository.getCollectionLastModified(), serverRepository.getInfoConfiguration(),
            serverRepository.authHeaderProvider);
  }

  @Override
  public void guidsSince(long timestamp,
                         RepositorySessionGuidsSinceDelegate delegate) {
    // TODO Auto-generated method stub

  }

  @Override
  public void fetchSince(long sinceTimestamp,
                         RepositorySessionFetchRecordsDelegate delegate) {
    BatchingDownloaderController.resumeFetchSinceIfPossible(
            this.downloader,
            this.serverRepository.stateProvider,
            delegate,
            sinceTimestamp,
            serverRepository.getBatchLimit(),
            serverRepository.getSortOrder()
    );
  }

  @Override
  public void fetchAll(RepositorySessionFetchRecordsDelegate delegate) {
    this.fetchSince(-1, delegate);
  }

  @Override
  public void fetch(String[] guids,
                    RepositorySessionFetchRecordsDelegate delegate) {
    this.downloader.fetch(guids, delegate);
  }

  @Override
  public void wipe(RepositorySessionWipeDelegate delegate) {
    if (!isActive()) {
      delegate.onWipeFailed(new InactiveSessionException());
      return;
    }
    // TODO: implement wipe.
  }

  @Override
  public void store(Record record) throws NoStoreDelegateException {
    if (storeDelegate == null) {
      throw new NoStoreDelegateException();
    }

    // If delegate was set, this shouldn't happen.
    if (uploader == null) {
      throw new IllegalStateException("Uploader haven't been initialized");
    }

    uploader.process(record);
  }

  @Override
  public void storeDone() {
    Logger.debug(LOG_TAG, "storeDone().");

    // If delegate was set, this shouldn't happen.
    if (uploader == null) {
      throw new IllegalStateException("Uploader haven't been initialized");
    }

    uploader.noMoreRecordsToUpload();
  }

  /**
   * @return Repository's high-water-mark if it's available, its use is allowed by the repository,
   * repository is set to fetch oldest-first, and it's greater than collection's last-synced timestamp.
   * Otherwise, returns repository's last-synced timestamp.
   */
  @Override
  public long getLastSyncTimestamp() {
    if (!serverRepository.getAllowHighWaterMark() || !serverRepository.getSortOrder().equals("oldest")) {
      return super.getLastSyncTimestamp();
    }

    final Long highWaterMark = serverRepository.stateProvider.getLong(
            RepositoryStateProvider.KEY_HIGH_WATER_MARK);

    // After a successful sync we expect that last-synced timestamp for a collection will be greater
    // than the high-water-mark. High-water-mark is mostly useful in case of resuming a sync,
    // and if we're resuming we did not bump our last-sync timestamps during the previous sync.
    if (highWaterMark == null || super.getLastSyncTimestamp() > highWaterMark) {
      return super.getLastSyncTimestamp();
    }

    return highWaterMark;
  }

  @Override
  public boolean dataAvailable() {
    return serverRepository.updateNeeded(getLastSyncTimestamp());
  }
}
