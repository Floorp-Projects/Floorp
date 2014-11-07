/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.mozstumbler.service.utils;

import android.os.Build;
import android.util.Log;

import org.mozilla.mozstumbler.service.AppGlobals;
import org.mozilla.mozstumbler.service.Prefs;

import java.io.BufferedOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;

public abstract class AbstractCommunicator {

    private static final String LOG_TAG = AppGlobals.makeLogTag(AbstractCommunicator.class.getSimpleName());
    private static final String NICKNAME_HEADER = "X-Nickname";
    private static final String USER_AGENT_HEADER = "User-Agent";
    private HttpURLConnection mHttpURLConnection;
    private final String mUserAgent;
    private static int sBytesSentTotal = 0;
    private static String sMozApiKey;

    public abstract String getUrlString();

    public static class HttpErrorException extends IOException {
        private static final long serialVersionUID = -5404095858043243126L;
        public final int responseCode;

        public HttpErrorException(int responseCode) {
            super();
            this.responseCode = responseCode;
        }

        public boolean isTemporary() {
            return responseCode >= 500 && responseCode <= 599;
        }
    }

    public static class SyncSummary {
        public int numIoExceptions;
        public int totalBytesSent;
    }

    public static class NetworkSendResult {
        public int bytesSent;
        // Zero is no error, for HTTP error cases, set this code to the error
        public int errorCode = -1;
    }

    public abstract NetworkSendResult cleanSend(byte[] data);

    public String getNickname() {
        return null;
    }

    public AbstractCommunicator(String userAgent) {
        mUserAgent = userAgent;
    }

    private void openConnectionAndSetHeaders() {
        try {
            if (sMozApiKey == null) {
                sMozApiKey = Prefs.getInstance().getMozApiKey();
            }
            URL url = new URL(getUrlString() + "?key=" + sMozApiKey);
            mHttpURLConnection = (HttpURLConnection) url.openConnection();
            mHttpURLConnection.setRequestMethod("POST");
        } catch (MalformedURLException e) {
            throw new IllegalArgumentException(e);
        } catch (IOException e) {
            Log.e(LOG_TAG, "Couldn't open a connection: " + e);
        }
        mHttpURLConnection.setDoOutput(true);
        mHttpURLConnection.setRequestProperty(USER_AGENT_HEADER, mUserAgent);
        mHttpURLConnection.setRequestProperty("Content-Type", "application/json");

        // Workaround for a bug in Android mHttpURLConnection. When the library
        // reuses a stale connection, the connection may fail with an EOFException
        if (Build.VERSION.SDK_INT > 13 && Build.VERSION.SDK_INT < 19) {
            mHttpURLConnection.setRequestProperty("Connection", "Close");
        }
        String nickname = getNickname();
        if (nickname != null) {
            mHttpURLConnection.setRequestProperty(NICKNAME_HEADER, nickname);
        }
    }

    private byte[] zipData(byte[] data) throws IOException {
        byte[] output = Zipper.zipData(data);
        return output;
    }

    private void sendData(byte[] data) throws IOException{
        mHttpURLConnection.setFixedLengthStreamingMode(data.length);
        OutputStream out = new BufferedOutputStream(mHttpURLConnection.getOutputStream());
        out.write(data);
        out.flush();
        int code = mHttpURLConnection.getResponseCode();
        final boolean isSuccessCode2XX = (code/100 == 2);
        if (!isSuccessCode2XX) {
            throw new HttpErrorException(code);
        }
    }

    public enum ZippedState { eNotZipped, eAlreadyZipped };
    /* Return the number of bytes sent. */
    public int send(byte[] data, ZippedState isAlreadyZipped) throws IOException {
        openConnectionAndSetHeaders();
        String logMsg;
        try {
            if (isAlreadyZipped != ZippedState.eAlreadyZipped) {
                data = zipData(data);
            }
            mHttpURLConnection.setRequestProperty("Content-Encoding","gzip");
        } catch (IOException e) {
            Log.e(LOG_TAG, "Couldn't compress and send data, falling back to plain-text: ", e);
            close();
        }

        try {
            sendData(data);
        } finally {
            close();
        }
        sBytesSentTotal += data.length;
        logMsg = "Send data: " + String.format("%.2f", data.length / 1024.0) + " kB";
        logMsg += " Session Total:" + String.format("%.2f", sBytesSentTotal / 1024.0) + " kB";
        AppGlobals.guiLogInfo(logMsg, "#FFFFCC", true);
        Log.d(LOG_TAG, logMsg);
        return data.length;
    }

    public InputStream getInputStream() {
        try {
            return mHttpURLConnection.getInputStream();
        } catch (IOException e) {
            return mHttpURLConnection.getErrorStream();
        }
    }

    public void close() {
        if (mHttpURLConnection == null) {
            return;
        }
        mHttpURLConnection.disconnect();
        mHttpURLConnection = null;
    }
}
