/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc;

import android.content.Context;
import android.net.Uri;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.dlc.catalog.DownloadContent;
import org.mozilla.gecko.dlc.catalog.DownloadContentBuilder;
import org.mozilla.gecko.dlc.catalog.DownloadContentCatalog;
import org.mozilla.gecko.util.Experiments;
import org.mozilla.gecko.util.IOUtils;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;

/**
 * Sync: Synchronize catalog from a Kinto instance.
 */
public class SyncAction extends BaseAction {
    private static final String LOGTAG = "DLCSyncAction";

    private static final String KINTO_KEY_ID = "id";
    private static final String KINTO_KEY_DELETED = "deleted";
    private static final String KINTO_KEY_DATA = "data";

    private static final String KINTO_PARAMETER_SINCE = "_since";
    private static final String KINTO_PARAMETER_FIELDS = "_fields";
    private static final String KINTO_PARAMETER_SORT = "_sort";

    /**
     * Kinto endpoint with online version of downloadable content catalog
     *
     * Dev instance:
     * https://kinto-ota.dev.mozaws.net/v1/buckets/dlc/collections/catalog/records
     */
    private static final String CATALOG_ENDPOINT = "https://firefox.settings.services.mozilla.com/v1/buckets/fennec/collections/catalog/records";

    @Override
    public void perform(Context context, DownloadContentCatalog catalog) {
        Log.d(LOGTAG, "Synchronizing catalog.");

        if (!isSyncEnabledForClient(context)) {
            Log.d(LOGTAG, "Sync is not enabled for client. Skipping.");
            return;
        }

        boolean cleanupRequired = false;
        boolean studyRequired = false;

        try {
            long lastModified = catalog.getLastModified();

            // TODO: Consider using ETag here (Bug 1257459)
            JSONArray rawCatalog = fetchRawCatalog(lastModified);

            Log.d(LOGTAG, "Server returned " + rawCatalog.length() + " records (since " + lastModified + ")");

            for (int i = 0; i < rawCatalog.length(); i++) {
                JSONObject object = rawCatalog.getJSONObject(i);
                String id = object.getString(KINTO_KEY_ID);

                final boolean isDeleted = object.optBoolean(KINTO_KEY_DELETED, false);

                DownloadContent existingContent = catalog.getContentById(id);

                if (isDeleted) {
                    cleanupRequired |= deleteContent(catalog, id);
                } else if (existingContent != null) {
                    studyRequired |= updateContent(catalog, object, existingContent);
                } else {
                    studyRequired |= createContent(catalog, object);
                }
            }
        } catch (UnrecoverableDownloadContentException e) {
            Log.e(LOGTAG, "UnrecoverableDownloadContentException", e);
        } catch (RecoverableDownloadContentException e) {
            Log.e(LOGTAG, "RecoverableDownloadContentException");
        } catch (JSONException e) {
            Log.e(LOGTAG, "JSONException", e);
        }

        if (studyRequired) {
            startStudyAction(context);
        }

        if (cleanupRequired) {
            startCleanupAction(context);
        }

        Log.v(LOGTAG, "Done");
    }

    protected void startStudyAction(Context context) {
        DownloadContentService.startStudy(context);
    }

    protected void startCleanupAction(Context context) {
        DownloadContentService.startCleanup(context);
    }

    protected JSONArray fetchRawCatalog(long lastModified)
            throws RecoverableDownloadContentException, UnrecoverableDownloadContentException {
        HttpURLConnection connection = null;

        try {
            Uri.Builder builder = Uri.parse(CATALOG_ENDPOINT).buildUpon();

            if (lastModified > 0) {
                builder.appendQueryParameter(KINTO_PARAMETER_SINCE, String.valueOf(lastModified));
            }
            // Only select the fields we are actually going to read.
            builder.appendQueryParameter(KINTO_PARAMETER_FIELDS,
                    "attachment.location,original.filename,original.hash,attachment.hash,type,kind,original.size,match");

            // We want to process items in the order they have been modified. This is to ensure that
            // our last_modified values are correct if we processing is interrupted and not all items
            // have been processed.
            builder.appendQueryParameter(KINTO_PARAMETER_SORT, "last_modified");

            connection = buildHttpURLConnection(builder.build().toString());

            // TODO: Read 'Alert' header and EOL message if existing (Bug 1249248)

            // TODO: Read and use 'Backoff' header if available (Bug 1249251)

            // TODO: Add support for Next-Page header (Bug 1257495)

            final int responseCode = connection.getResponseCode();

            if (responseCode != HttpURLConnection.HTTP_OK) {
                if (responseCode >= 500) {
                    // A Retry-After header will be added to error responses (>=500), telling the
                    // client how many seconds it should wait before trying again.

                    // TODO: Read and obey value in "Retry-After" header (Bug 1249249)
                    throw new RecoverableDownloadContentException(RecoverableDownloadContentException.SERVER, "Server error (" + responseCode + ")");
                } else if (responseCode == 410) {
                    // A 410 Gone error response can be returned if the client version is too old,
                    // or the service had been replaced with a new and better service using a new
                    // protocol version.

                    // TODO: The server is gone. Stop synchronizing the catalog from this server (Bug 1249248).
                    throw new UnrecoverableDownloadContentException("Server is gone (410)");
                } else if (responseCode >= 400) {
                    // If the HTTP status is >=400 the response contains a JSON response.
                    logErrorResponse(connection);

                    // Unrecoverable: Client errors 4xx - Unlikely that this version of the client will ever succeed.
                    throw new UnrecoverableDownloadContentException("(Unrecoverable) Catalog sync failed. Status code: " + responseCode);
                } else if (responseCode < 200) {
                    // If the HTTP status is <200 the response contains a JSON response.
                    logErrorResponse(connection);

                    throw new RecoverableDownloadContentException(RecoverableDownloadContentException.SERVER,  "Response code: " + responseCode);
                } else {
                    // HttpsUrlConnection: -1 (No valid response code)
                    // Successful 2xx: We don't know how to handle anything but 200.
                    // Redirection 3xx: We should have followed redirects if possible. We should not see those errors here.

                    throw new UnrecoverableDownloadContentException("(Unrecoverable) Download failed. Response code: " + responseCode);
                }
            }

            return fetchJSONResponse(connection).getJSONArray(KINTO_KEY_DATA);
        } catch (JSONException | IOException e) {
            throw new RecoverableDownloadContentException(RecoverableDownloadContentException.NETWORK, e);
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
    }

    private JSONObject fetchJSONResponse(HttpURLConnection connection) throws IOException, JSONException {
        InputStream inputStream = null;

        try {
            inputStream = new BufferedInputStream(connection.getInputStream());
            ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
            IOUtils.copy(inputStream, outputStream);
            return new JSONObject(outputStream.toString("UTF-8"));
        } finally {
            IOUtils.safeStreamClose(inputStream);
        }
    }

    protected boolean updateContent(DownloadContentCatalog catalog, JSONObject object, DownloadContent existingContent)
            throws JSONException {
        DownloadContent content = existingContent.buildUpon()
                .updateFromKinto(object)
                .build();

        if (existingContent.getLastModified() >= content.getLastModified()) {
            Log.d(LOGTAG, "Item has not changed: " + content);
            return false;
        }

        catalog.update(content);

        return true;
    }

    protected boolean createContent(DownloadContentCatalog catalog, JSONObject object) throws JSONException {
        DownloadContent content = new DownloadContentBuilder()
                .updateFromKinto(object)
                .build();

        catalog.add(content);

        return true;
    }

    protected boolean deleteContent(DownloadContentCatalog catalog, String id) {
        DownloadContent content = catalog.getContentById(id);
        if (content == null) {
            return false;
        }

        catalog.markAsDeleted(content);

        return true;
    }

    protected boolean isSyncEnabledForClient(Context context) {
        // Sync action is behind a switchboard flag for staged rollout.
        return Experiments.isInExperimentLocal(context, Experiments.DOWNLOAD_CONTENT_CATALOG_SYNC);
    }

    private void logErrorResponse(HttpURLConnection connection) {
        try {
            JSONObject error = fetchJSONResponse(connection);

            Log.w(LOGTAG, "Server returned error response:");
            Log.w(LOGTAG, "- Code:    " + error.getInt("code"));
            Log.w(LOGTAG, "- Errno:   " + error.getInt("errno"));
            Log.w(LOGTAG, "- Error:   " + error.optString("error", "-"));
            Log.w(LOGTAG, "- Message: " + error.optString("message", "-"));
            Log.w(LOGTAG, "- Info:    " + error.optString("info", "-"));
        } catch (JSONException | IOException e) {
            Log.w(LOGTAG, "Could not fetch error response", e);
        }
    }
}
