/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.net;

import android.support.annotation.VisibleForTesting;
import android.util.Log;

import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.util.IOUtils;

import java.io.BufferedWriter;
import java.io.IOException;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;

public class HttpURLConnectionTelemetryClient implements TelemetryClient {
    private static final String LOG_TAG = "HttpURLTelemetryClient";

    @Override
    public boolean uploadPing(TelemetryConfiguration configuration, String path, String serializedPing) {
        HttpURLConnection connection = null;

        try {
            connection = openConnectionConnection(configuration.getServerEndpoint(), path);
            connection.setConnectTimeout(configuration.getConnectTimeout());
            connection.setReadTimeout(configuration.getReadTimeout());

            connection.setRequestProperty("Content-Type", "application/json; charset=utf-8");
            connection.setRequestProperty("User-Agent", configuration.getUserAgent());

            connection.setRequestMethod("POST");
            connection.setDoOutput(true);

            int responseCode = upload(connection, serializedPing);

            Log.d(LOG_TAG, "Ping upload: " + responseCode);

            if (responseCode >= 200 && responseCode <= 299) {
                // Known success errors (2xx):
                // 200 - OK. Request accepted into the pipeline.

                // We treat all success codes as successful upload even though we only expect 200.
                return true;
            } else if (responseCode >= 400 && responseCode <= 499) {
                // Known client (4xx) errors:
                // 404 - not found - POST/PUT to an unknown namespace
                // 405 - wrong request type (anything other than POST/PUT)
                // 411 - missing content-length header
                // 413 - request body too large (Note that if we have badly-behaved clients that
                //       retry on 4XX, we should send back 202 on body/path too long).
                // 414 - request path too long (See above)

                // Something our client did is not correct. It's unlikely that the client is going
                // to recover from this by re-trying again, so we just log and error and report a
                // successful upload to the service.
                Log.e(LOG_TAG, "Server returned client error code: " + responseCode);
                return true;
            } else {
                // Known other errors:
                // 500 - internal error

                // For all other errors we log a warning an try again at a later time.
                Log.w(LOG_TAG, "Server returned response code: " + responseCode);
                return false;
            }
        } catch (MalformedURLException e) {
            // There's nothing we can do to recover from this here. So let's just log an error and
            // notify the service that this job has been completed - even though we didn't upload
            // anything to the server.
            Log.e(LOG_TAG, "Could not upload telemetry due to malformed URL", e);
            return true;
        } catch (IOException e) {
            Log.w(LOG_TAG, "IOException while uploading ping", e);
            return false;
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
    }

    @VisibleForTesting int upload(HttpURLConnection connection, String serializedPing) throws IOException {
        OutputStream stream = null;

        try {
            final BufferedWriter writer = new BufferedWriter(
                    new OutputStreamWriter(stream = connection.getOutputStream()));

            writer.write(serializedPing);
            writer.flush();
            writer.close();

            return connection.getResponseCode();
        } finally {
            IOUtils.safeClose(stream);
        }
    }

    @VisibleForTesting HttpURLConnection openConnectionConnection(String endpoint, String path) throws IOException {
        final URL url = new URL(endpoint + path);
        return (HttpURLConnection) url.openConnection();
    }
}
