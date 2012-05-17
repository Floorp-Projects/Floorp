/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.concurrent.atomic.AtomicInteger;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.mozilla.gecko.sync.CommandProcessor;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.HTTPFailureException;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.NoCollectionKeysSetException;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.delegates.ClientsDataDelegate;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.SyncStorageCollectionRequest;
import org.mozilla.gecko.sync.net.SyncStorageRecordRequest;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.net.WBOCollectionRequestDelegate;
import org.mozilla.gecko.sync.net.WBORequestDelegate;
import org.mozilla.gecko.sync.repositories.android.ClientsDatabaseAccessor;
import org.mozilla.gecko.sync.repositories.android.RepoUtils;
import org.mozilla.gecko.sync.repositories.domain.ClientRecord;
import org.mozilla.gecko.sync.repositories.domain.ClientRecordFactory;
import org.mozilla.gecko.sync.repositories.domain.VersionConstants;

import ch.boye.httpclientandroidlib.HttpStatus;

public class SyncClientsEngineStage implements GlobalSyncStage {
  public static final String LOG_TAG = "SyncClientsEngineStage";
  public static final String COLLECTION_NAME = "clients";
  public static final int CLIENTS_TTL_REFRESH = 604800000; // 7 days
  public static final int MAX_UPLOAD_FAILURE_COUNT = 5;

  protected final GlobalSession session;
  protected final ClientRecordFactory factory = new ClientRecordFactory();
  protected ClientUploadDelegate clientUploadDelegate;
  protected ClientDownloadDelegate clientDownloadDelegate;

  // Be sure to use this safely via getClientsDatabaseAccessor/closeDataAccessor.
  protected ClientsDatabaseAccessor db;

  protected volatile boolean shouldWipe;
  protected volatile boolean commandsProcessedShouldUpload;
  protected final AtomicInteger uploadAttemptsCount = new AtomicInteger();

  public SyncClientsEngineStage(GlobalSession session) {
    if (session == null) {
      throw new IllegalArgumentException("session must not be null.");
    }
    this.session = session;
  }

  protected int getClientsCount() {
    return getClientsDatabaseAccessor().clientsCount();
  }

  protected synchronized ClientsDatabaseAccessor getClientsDatabaseAccessor() {
    if (db == null) {
      db = new ClientsDatabaseAccessor(session.getContext());
    }
    return db;
  }

  protected synchronized void closeDataAccessor() {
    if (db == null) {
      return;
    }
    db.close();
    db = null;
  }

  /**
   * The following two delegates, ClientDownloadDelegate and ClientUploadDelegate
   * are both triggered in a chain, starting when execute() calls
   * downloadClientRecords().
   *
   * Client records are downloaded using a get() request. Upon success of the
   * get() request, the local client record is uploaded.
   *
   * @author Marina Samuel
   *
   */
  public class ClientDownloadDelegate extends WBOCollectionRequestDelegate {

    // We use this on each WBO, so lift it out.
    final ClientsDataDelegate clientsDelegate = session.getClientsDelegate();
    boolean localAccountGUIDDownloaded = false;

    @Override
    public String credentials() {
      return session.credentials();
    }

    @Override
    public String ifUnmodifiedSince() {
      // TODO last client download time?
      return null;
    }

    @Override
    public void handleRequestSuccess(SyncStorageResponse response) {

      // Hang onto the server's last modified timestamp to use
      // in X-If-Unmodified-Since for upload.
      session.config.persistServerClientsTimestamp(response.normalizedWeaveTimestamp());
      BaseResource.consumeEntity(response);

      // If we successfully downloaded all records but ours was not one of them
      // then reset the timestamp.
      if (!localAccountGUIDDownloaded) {
        Logger.info(LOG_TAG, "Local client GUID does not exist on the server. Upload timestamp will be reset.");
        session.config.persistServerClientRecordTimestamp(0);
      }
      localAccountGUIDDownloaded = false;

      final int clientsCount;
      try {
        clientsCount = getClientsCount();
      } finally {
        // Close the database to clear cached readableDatabase/writableDatabase
        // after we've completed our last transaction (db.store()).
        closeDataAccessor();
      }

      Logger.debug(LOG_TAG, "Database contains " + clientsCount + " clients.");
      Logger.debug(LOG_TAG, "Server response asserts " + response.weaveRecords() + " records.");

      // TODO: persist the response timestamp to know whether to download next time (Bug 726055).
      clientUploadDelegate = new ClientUploadDelegate();
      clientsDelegate.setClientsCount(clientsCount);
      checkAndUpload();
    }

    @Override
    public void handleRequestFailure(SyncStorageResponse response) {
      BaseResource.consumeEntity(response); // We don't need the response at all, and any exception handling shouldn't need the response body.
      localAccountGUIDDownloaded = false;

      try {
        Logger.info(LOG_TAG, "Client upload failed. Aborting sync.");
        session.abort(new HTTPFailureException(response), "Client download failed.");
      } finally {
        // Close the database upon failure.
        closeDataAccessor();
      }
    }

    @Override
    public void handleRequestError(Exception ex) {
      localAccountGUIDDownloaded = false;
      try {
        Logger.info(LOG_TAG, "Client upload error. Aborting sync.");
        session.abort(ex, "Failure fetching client record.");
      } finally {
        // Close the database upon error.
        closeDataAccessor();
      }
    }

    @Override
    public void handleWBO(CryptoRecord record) {
      ClientRecord r;
      try {
        r = (ClientRecord) factory.createRecord(record.decrypt());
        if (clientsDelegate.isLocalGUID(r.guid)) {
          Logger.info(LOG_TAG, "Local client GUID exists on server and was downloaded");

          localAccountGUIDDownloaded = true;
          session.config.persistServerClientRecordTimestamp(r.lastModified);
          processCommands(r.commands);
        }
        RepoUtils.logClient(r);
      } catch (Exception e) {
        session.abort(e, "Exception handling client WBO.");
        return;
      }
      wipeAndStore(r);
    }

    @Override
    public KeyBundle keyBundle() {
      try {
        return session.keyBundleForCollection(COLLECTION_NAME);
      } catch (NoCollectionKeysSetException e) {
        session.abort(e, "No collection keys set.");
        return null;
      }
    }
  }

  public class ClientUploadDelegate extends WBORequestDelegate {
    protected static final String LOG_TAG = "ClientUploadDelegate";

    @Override
    public String credentials() {
      return session.credentials();
    }

    @Override
    public String ifUnmodifiedSince() {
      // Use the timestamp for the whole collection per Sync storage 1.1 spec.
      Long timestampInMilliseconds = session.config.getPersistedServerClientsTimestamp();

      // It's the first upload so we don't care about X-If-Unmodified-Since.
      if (timestampInMilliseconds == 0) {
        return null;
      }

      return Utils.millisecondsToDecimalSecondsString(timestampInMilliseconds);
    }

    @Override
    public void handleRequestSuccess(SyncStorageResponse response) {
      Logger.debug(LOG_TAG, "Upload succeeded.");
      try {
        commandsProcessedShouldUpload = false;
        uploadAttemptsCount.set(0);

        // Persist the timestamp for the record we just uploaded,
        // and bump the collection timestamp, too.
        long timestamp = response.normalizedWeaveTimestamp();
        session.config.persistServerClientRecordTimestamp(timestamp);
        session.config.persistServerClientsTimestamp(timestamp);
        BaseResource.consumeEntity(response);

        Logger.debug(LOG_TAG, "Timestamp is " + timestamp);
      } catch (Exception e) {
        session.abort(e, "Unable to fetch timestamp.");
        return;
      }
      session.advance();
    }

    @Override
    public void handleRequestFailure(SyncStorageResponse response) {
      int statusCode = response.getStatusCode();

      // If upload failed because of `ifUnmodifiedSince` then there are new
      // commands uploaded to our record. We must download and process them first.
      if (!commandsProcessedShouldUpload ||
          statusCode == HttpStatus.SC_PRECONDITION_FAILED ||
          uploadAttemptsCount.incrementAndGet() > MAX_UPLOAD_FAILURE_COUNT) {
        Logger.debug(LOG_TAG, "Client upload failed. Aborting sync.");
        BaseResource.consumeEntity(response); // The exception thrown should need the response body.
        session.abort(new HTTPFailureException(response), "Client upload failed.");
        return;
      }
      Logger.trace(LOG_TAG, "Retrying upload…");
      // Preconditions:
      // commandsProcessedShouldUpload == true &&
      // statusCode != 412 &&
      // uploadAttemptCount < MAX_UPLOAD_FAILURE_COUNT
      checkAndUpload();
    }

    @Override
    public void handleRequestError(Exception ex) {
      Logger.info(LOG_TAG, "Client upload error. Aborting sync.");
      session.abort(ex, "Client upload failed.");
    }

    @Override
    public KeyBundle keyBundle() {
      try {
        return session.keyBundleForCollection(COLLECTION_NAME);
      } catch (NoCollectionKeysSetException e) {
        session.abort(e, "No collection keys set.");
        return null;
      }
    }
  }

  @Override
  public void execute() throws NoSuchStageException {
    if (shouldDownload()) {
      downloadClientRecords();   // Will kick off upload, too…
    } else {
      // Upload if necessary.
    }
  }

  @Override
  public void resetLocal() {
    // Clear timestamps and local data.
    session.config.persistServerClientRecordTimestamp(0L);   // TODO: roll these into one.
    session.config.persistServerClientsTimestamp(0L);

    session.getClientsDelegate().setClientsCount(0);
    try {
      getClientsDatabaseAccessor().wipe();
    } finally {
      closeDataAccessor();
    }
  }

  @Override
  public void wipeLocal() throws Exception {
    // Nothing more to do.
    this.resetLocal();
  }

  public Integer getStorageVersion() {
    return VersionConstants.CLIENTS_ENGINE_VERSION;
  }

  protected ClientRecord newLocalClientRecord(ClientsDataDelegate delegate) {
    final String ourGUID = delegate.getAccountGUID();
    final String ourName = delegate.getClientName();

    ClientRecord r = new ClientRecord(ourGUID);
    r.name = ourName;
    return r;
  }

  // TODO: Bug 726055 - More considered handling of when to sync.
  protected boolean shouldDownload() {
    // Ask info/collections whether a download is needed.
    return true;
  }

  protected boolean shouldUpload() {
    if (commandsProcessedShouldUpload) {
      return true;
    }

    long lastUpload = session.config.getPersistedServerClientRecordTimestamp();   // Defaults to 0.
    if (lastUpload == 0) {
      return true;
    }

    // Note the opportunity for clock drift problems here.
    // TODO: if we track download times, we can use the timestamp of most
    // recent download response instead of the current time.
    long now = System.currentTimeMillis();
    long age = now - lastUpload;
    return age >= CLIENTS_TTL_REFRESH;
  }

  protected void processCommands(JSONArray commands) {
    if (commands == null ||
        commands.size() == 0) {
      return;
    }

    commandsProcessedShouldUpload = true;
    CommandProcessor processor = CommandProcessor.getProcessor();

    // TODO: Bug 715792 - Process commands here.
    for (int i = 0; i < commands.size(); i++) {
      processor.processCommand(new ExtendedJSONObject((JSONObject) commands.get(i)));
    }
  }

  protected void checkAndUpload() {
    if (!shouldUpload()) {
      Logger.debug(LOG_TAG, "Not uploading client record.");
      session.advance();
      return;
    }

    // Generate CryptoRecord from ClientRecord to upload.
    final String encryptionFailure = "Couldn't encrypt new client record.";
    final ClientRecord localClient = newLocalClientRecord(session.getClientsDelegate());
    try {
      CryptoRecord cryptoRecord = localClient.getEnvelope();
      cryptoRecord.keyBundle = clientUploadDelegate.keyBundle();
      cryptoRecord.encrypt();
      this.uploadClientRecord(cryptoRecord);
    } catch (UnsupportedEncodingException e) {
      session.abort(e, encryptionFailure + " Unsupported encoding.");
    } catch (CryptoException e) {
      session.abort(e, encryptionFailure);
    }
  }

  protected void downloadClientRecords() {
    shouldWipe = true;
    clientDownloadDelegate = makeClientDownloadDelegate();

    try {
      URI getURI = session.config.collectionURI(COLLECTION_NAME, true);

      SyncStorageCollectionRequest request = new SyncStorageCollectionRequest(getURI);
      request.delegate = clientDownloadDelegate;

      Logger.trace(LOG_TAG, "Downloading client records.");
      request.get();
    } catch (URISyntaxException e) {
      session.abort(e, "Invalid URI.");
    }
  }

  /**
   * Upload a client record via HTTP POST to the parent collection.
   */
  protected void uploadClientRecord(CryptoRecord record) {
    Logger.debug(LOG_TAG, "Uploading client record " + record.guid);
    try {
      URI postURI = session.config.collectionURI(COLLECTION_NAME);
      SyncStorageRecordRequest request = new SyncStorageRecordRequest(postURI);
      request.delegate = clientUploadDelegate;
      request.post(record);
    } catch (URISyntaxException e) {
      session.abort(e, "Invalid URI.");
    }
  }

  protected ClientDownloadDelegate makeClientDownloadDelegate() {
    return new ClientDownloadDelegate();
  }

  protected void wipeAndStore(ClientRecord record) {
    ClientsDatabaseAccessor db = getClientsDatabaseAccessor();
    if (shouldWipe) {
      db.wipe();
      shouldWipe = false;
    }
    db.store(record);
  }
}
