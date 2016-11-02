/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import java.net.URI;
import java.net.URISyntaxException;

import org.mozilla.gecko.sync.InfoCollections;
import org.mozilla.gecko.sync.InfoConfiguration;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;

import android.content.Context;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

/**
 * A Server11Repository implements fetching and storing against the Sync 1.5 API.
 * It doesn't do crypto: that's the job of the middleware.
 *
 * @author rnewman
 */
public class Server11Repository extends Repository {
  public final AuthHeaderProvider authHeaderProvider;

  private final long syncDeadlineMillis;
  /* package-local */ final URI collectionURI;

  protected final String collection;
  protected final InfoCollections infoCollections;

  private final InfoConfiguration infoConfiguration;
  private final static String DEFAULT_SORT_ORDER = "oldest";
  private final static long DEFAULT_BATCH_LIMIT = 100;

  /**
   * Construct a new repository that fetches and stores against the Sync 1.5 API.
   *
   * @param collection name.
   * @param storageURL full URL to storage endpoint.
   * @param authHeaderProvider to use in requests; may be null.
   * @param infoCollections instance; must not be null.
   * @throws URISyntaxException
   */
  public Server11Repository(
          @NonNull String collection,
          long syncDeadlineMillis,
          @NonNull String storageURL,
          AuthHeaderProvider authHeaderProvider,
          @NonNull InfoCollections infoCollections,
          @NonNull InfoConfiguration infoConfiguration) throws URISyntaxException {
    if (collection == null) {
      throw new IllegalArgumentException("collection must not be null");
    }
    if (storageURL == null) {
      throw new IllegalArgumentException("storageURL must not be null");
    }
    if (infoCollections == null) {
      throw new IllegalArgumentException("infoCollections must not be null");
    }
    this.collection = collection;
    this.syncDeadlineMillis = syncDeadlineMillis;
    this.collectionURI = new URI(storageURL + (storageURL.endsWith("/") ? collection : "/" + collection));
    this.authHeaderProvider = authHeaderProvider;
    this.infoCollections = infoCollections;
    this.infoConfiguration = infoConfiguration;
  }

  @Override
  public void createSession(RepositorySessionCreationDelegate delegate,
                            Context context) {
    delegate.onSessionCreated(new Server11RepositorySession(this));
  }

  public URI collectionURI() {
    return this.collectionURI;
  }

  /* package-local */ boolean updateNeeded(long lastSyncTimestamp) {
    return infoCollections.updateNeeded(collection, lastSyncTimestamp);
  }

  @Nullable
  /* package-local */ Long getCollectionLastModified() {
    return infoCollections.getTimestamp(collection);
  }

  public InfoConfiguration getInfoConfiguration() {
    return infoConfiguration;
  }

  public String getSortOrder() {
    return DEFAULT_SORT_ORDER;
  }

  public Long getBatchLimit() {
    return DEFAULT_BATCH_LIMIT;
  }

  public boolean getAllowMultipleBatches() {
    return true;
  }

  /**
   * A point in time by which this repository's session must complete fetch and store operations.
   * Particularly pertinent for batching downloads performed by the session (should we fetch
   * another batch?) and buffered repositories (do we have enough time to merge what we've downloaded?).
   */
  public long getSyncDeadline() {
    return syncDeadlineMillis;
  }
}
