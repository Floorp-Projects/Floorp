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
import org.mozilla.gecko.sync.repositories.uploaders.BatchingUploader;

public class Server11RepositorySession extends RepositorySession {
  public static final String LOG_TAG = "Server11Session";

  Server11Repository serverRepository;
  private BatchingUploader uploader;
  private final BatchingDownloader downloader;

  public Server11RepositorySession(Repository repository) {
    super(repository);
    serverRepository = (Server11Repository) repository;
    this.downloader = new BatchingDownloader(serverRepository, this);
  }

  public Server11Repository getServerRepository() {
    return serverRepository;
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
  public void fetchSince(long timestamp,
                         RepositorySessionFetchRecordsDelegate delegate) {
    this.downloader.fetchSince(timestamp, delegate);
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

  @Override
  public boolean dataAvailable() {
    return serverRepository.updateNeeded(getLastSyncTimestamp());
  }
}
