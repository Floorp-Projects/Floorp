/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport.upload;

import java.io.IOException;
import java.util.UUID;

import org.json.JSONObject;
import org.mozilla.gecko.background.bagheera.BagheeraClient;
import org.mozilla.gecko.background.bagheera.BagheeraRequestDelegate;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.healthreport.EnvironmentBuilder;
import org.mozilla.gecko.background.healthreport.HealthReportConstants;
import org.mozilla.gecko.background.healthreport.HealthReportDatabaseStorage;
import org.mozilla.gecko.background.healthreport.HealthReportGenerator;

import android.content.ContentProviderClient;
import android.content.Context;
import android.content.SharedPreferences;
import ch.boye.httpclientandroidlib.HttpResponse;

public class AndroidSubmissionClient implements SubmissionClient {
  protected static final String LOG_TAG = AndroidSubmissionClient.class.getSimpleName();

  protected final Context context;
  protected final SharedPreferences sharedPreferences;
  protected final String profilePath;

  public AndroidSubmissionClient(Context context, SharedPreferences sharedPreferences, String profilePath) {
    this.context = context;
    this.sharedPreferences = sharedPreferences;
    this.profilePath = profilePath;
  }

  public SharedPreferences getSharedPreferences() {
    return sharedPreferences;
  }

  public String getDocumentServerURI() {
    return getSharedPreferences().getString(HealthReportConstants.PREF_DOCUMENT_SERVER_URI, HealthReportConstants.DEFAULT_DOCUMENT_SERVER_URI);
  }

  public String getDocumentServerNamespace() {
    return getSharedPreferences().getString(HealthReportConstants.PREF_DOCUMENT_SERVER_NAMESPACE, HealthReportConstants.DEFAULT_DOCUMENT_SERVER_NAMESPACE);
  }

  public long getLastUploadLocalTime() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_LAST_UPLOAD_LOCAL_TIME, 0L);
  }

  public String getLastUploadDocumentId() {
    return getSharedPreferences().getString(HealthReportConstants.PREF_LAST_UPLOAD_DOCUMENT_ID, null);
  }

  public void setLastUploadLocalTimeAndDocumentId(long localTime, String id) {
    getSharedPreferences().edit()
      .putLong(HealthReportConstants.PREF_LAST_UPLOAD_LOCAL_TIME, localTime)
      .putString(HealthReportConstants.PREF_LAST_UPLOAD_DOCUMENT_ID, id)
      .commit();
  }

  protected void uploadPayload(String payload, BagheeraRequestDelegate uploadDelegate) {
    final BagheeraClient client = new BagheeraClient(getDocumentServerURI());

    final String id = UUID.randomUUID().toString();
    final String lastId = getLastUploadDocumentId();

    Logger.pii(LOG_TAG, "New health report has id " + id +
        (lastId == null ? "." : " and obsoletes id " + lastId + "."));

    try {
      client.uploadJSONDocument(getDocumentServerNamespace(), id, payload, lastId, uploadDelegate);
    } catch (Exception e) {
      uploadDelegate.handleError(e);
    }
  }

  @Override
  public void upload(long localTime, Delegate delegate) {
    // We abuse the life-cycle of an Android ContentProvider slightly by holding
    // onto a ContentProviderClient while we generate a payload. This keeps our
    // database storage alive, and may also allow us to share a database
    // connection with a BrowserHealthRecorder from Fennec.  The ContentProvider
    // owns all underlying Storage instances, so we don't need to explicitly
    // close them.
    ContentProviderClient client = EnvironmentBuilder.getContentProviderClient(context);
    if (client == null) {
      delegate.onHardFailure(localTime, null, "Could not fetch content provider client.", null);
      return;
    }

    try {
      // Storage instance is owned by HealthReportProvider, so we don't need to
      // close it. It's worth noting that this call will fail if called
      // out-of-process.
      HealthReportDatabaseStorage storage = EnvironmentBuilder.getStorage(client, profilePath);
      if (storage == null) {
        delegate.onHardFailure(localTime, null, "No storage when generating report.", null);
        return;
      }

      long since = localTime - HealthReportConstants.MILLISECONDS_PER_SIX_MONTHS;
      long last = Math.max(getLastUploadLocalTime(), HealthReportConstants.EARLIEST_LAST_PING);

      if (!storage.hasEventSince(last)) {
        delegate.onHardFailure(localTime, null, "No new events in storage.", null);
        return;
      }

      HealthReportGenerator generator = new HealthReportGenerator(storage);
      JSONObject document = generator.generateDocument(since, last, profilePath);
      if (document == null) {
        delegate.onHardFailure(localTime, null, "Generator returned null document.", null);
        return;
      }

      BagheeraRequestDelegate uploadDelegate = new RequestDelegate(delegate, localTime, true, null);
      this.uploadPayload(document.toString(), uploadDelegate);
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Got exception generating document.", e);
      delegate.onHardFailure(localTime, null, "Got exception uploading.", e);
      return;
    } finally {
      client.release();
    }
  }

  @Override
  public void delete(final long localTime, final String id, Delegate delegate) {
    final BagheeraClient client = new BagheeraClient(getDocumentServerURI());

    Logger.pii(LOG_TAG, "Deleting health report with id " + id + ".");

    BagheeraRequestDelegate deleteDelegate = new RequestDelegate(delegate, localTime, false, id);
    try {
      client.deleteDocument(getDocumentServerNamespace(), id, deleteDelegate);
    } catch (Exception e) {
      deleteDelegate.handleError(e);
    }
  }

  protected class RequestDelegate implements BagheeraRequestDelegate {
    protected final Delegate delegate;
    protected final boolean isUpload;
    protected final String methodString;
    protected final long localTime;
    protected final String id;

    public RequestDelegate(Delegate delegate, long localTime, boolean isUpload, String id) {
      this.delegate = delegate;
      this.localTime = localTime;
      this.isUpload = isUpload;
      this.methodString = this.isUpload ? "upload" : "delete";
      this.id = this.isUpload ? null : id; // id is known for deletions only.
    }

    @Override
    public void handleSuccess(int status, String namespace, String id, HttpResponse response) {
      if (isUpload) {
        setLastUploadLocalTimeAndDocumentId(localTime, id);
      }
      Logger.debug(LOG_TAG, "Successful " + methodString + " at " + localTime + ".");
      delegate.onSuccess(localTime, id);
    }

    /**
     * Bagheera status codes:
     *
     * 403 Forbidden - Violated access restrictions. Most likely because of the method used.
     * 413 Request Too Large - Request payload was larger than the configured maximum.
     * 400 Bad Request - Returned if the POST/PUT failed validation in some manner.
     * 404 Not Found - Returned if the URI path doesn't exist or if the URI was not in the proper format.
     * 500 Server Error - General server error. Someone with access should look at the logs for more details.
     */
    @Override
    public void handleFailure(int status, String namespace, HttpResponse response) {
      Logger.debug(LOG_TAG, "Failed " + methodString + " at " + localTime + ".");
      if (status >= 500) {
        delegate.onSoftFailure(localTime, id, "Got status " + status + " from server.", null);
        return;
      }
      // Things are either bad locally (bad payload format, too much data) or
      // bad remotely (badly configured server, temporarily unavailable). Try
      // again tomorrow.
      delegate.onHardFailure(localTime, id, "Got status " + status + " from server.", null);
    }

    @Override
    public void handleError(Exception e) {
      Logger.debug(LOG_TAG, "Exception during " + methodString + " at " + localTime + ".", e);
      if (e instanceof IOException) {
        // Let's assume IO exceptions are Android dropping the network.
        delegate.onSoftFailure(localTime, id, "Got exception during " + methodString + ".", e);
        return;
      }
      delegate.onHardFailure(localTime, id, "Got exception during " + methodString + ".", e);
    }
  };
}
