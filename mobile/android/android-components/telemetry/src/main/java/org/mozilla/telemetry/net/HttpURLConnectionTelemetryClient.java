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

            return responseCode == 200;
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
        System.out.println("Connecting to: " + endpoint + path);

        final URL url = new URL(endpoint + path);
        return (HttpURLConnection) url.openConnection();
    }
}
