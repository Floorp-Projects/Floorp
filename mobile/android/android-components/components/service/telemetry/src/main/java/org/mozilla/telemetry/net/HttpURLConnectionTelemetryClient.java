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
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Locale;
import java.util.TimeZone;

import mozilla.components.support.base.log.logger.Logger;

public class HttpURLConnectionTelemetryClient implements TelemetryClient {
    private Logger logger = new Logger("telemetry/client");

    @Override
    public boolean uploadPing(TelemetryConfiguration configuration, String path, String serializedPing) {
        HttpURLConnection connection = null;

        try {
            connection = openConnectionConnection(configuration.getServerEndpoint(), path);
            connection.setConnectTimeout(configuration.getConnectTimeout());
            connection.setReadTimeout(configuration.getReadTimeout());

            connection.setRequestProperty("Content-Type", "application/json; charset=utf-8");
            connection.setRequestProperty("User-Agent", configuration.getUserAgent());
            connection.setRequestProperty("Date", createDateHeaderValue());

            connection.setRequestMethod("POST");
            connection.setDoOutput(true);

            int responseCode = upload(connection, serializedPing);

            logger.debug("Ping upload: " + responseCode, null);

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
                logger.error("Server returned client error code: " + responseCode, null);
                return true;
            } else {
                // Known other errors:
                // 500 - internal error

                // For all other errors we log a warning an try again at a later time.
                logger.warn("Server returned response code: " + responseCode, null);
                return false;
            }
        } catch (MalformedURLException e) {
            // There's nothing we can do to recover from this here. So let's just log an error and
            // notify the service that this job has been completed - even though we didn't upload
            // anything to the server.
            logger.error("Could not upload telemetry due to malformed URL", e);
            return true;
        } catch (IOException e) {
            logger.warn("IOException while uploading ping", e);
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
        } catch (ArrayIndexOutOfBoundsException e) {
            // This exception is sometimes thrown from the inside of HttpUrlConnection/OkHttp:
            // https://github.com/mozilla-mobile/telemetry-android/issues/54
            // We wrap it and handle it like other IO exceptions.
            throw new IOException(e);
        } finally {
            IOUtils.safeClose(stream);
        }
    }

    @VisibleForTesting HttpURLConnection openConnectionConnection(String endpoint, String path) throws IOException {
        final URL url = new URL(endpoint + path);
        return (HttpURLConnection) url.openConnection();
    }

    @VisibleForTesting String createDateHeaderValue() {
        final Calendar calendar = Calendar.getInstance();
        final SimpleDateFormat dateFormat = new SimpleDateFormat("EEE, dd MMM yyyy HH:mm:ss z", Locale.US);
        dateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));
        return dateFormat.format(calendar.getTime());
    }
}
