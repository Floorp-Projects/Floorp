package org.mozilla.geckoview;

import org.mozilla.gecko.util.ProxySelector;

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.AnyThread;
import android.support.annotation.NonNull;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.net.URLDecoder;
import java.nio.channels.Channels;
import java.nio.channels.FileChannel;
import java.security.MessageDigest;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.zip.GZIPOutputStream;

/**
 * Sends a crash report to the Mozilla  <a href="https://wiki.mozilla.org/Socorro">Socorro</a>
 * crash report server.
 */
public class CrashReporter {
    private static final String LOGTAG = "GeckoCrashReporter";
    private static final String MINI_DUMP_PATH_KEY = "upload_file_minidump";
    private static final String MINI_DUMP_SUCCESS_KEY = "minidumpSuccess";
    private static final String PAGE_URL_KEY = "URL";
    private static final String NOTES_KEY = "Notes";
    private static final String SERVER_URL_KEY = "ServerURL";
    private static final String STACK_TRACES_KEY = "StackTraces";
    private static final String PRODUCT_NAME_KEY = "ProductName";
    private static final String PRODUCT_ID_KEY = "ProductID";
    private static final String PRODUCT_ID = "{eeb82917-e434-4870-8148-5c03d4caa81b}";
    private static final List<String> IGNORE_KEYS = Arrays.asList(
            NOTES_KEY,
            PAGE_URL_KEY,
            SERVER_URL_KEY,
            STACK_TRACES_KEY
    );

    /**
     * Sends a crash report to the Mozilla  <a href="https://wiki.mozilla.org/Socorro">Socorro</a>
     * crash report server.
     * <br>
     * The {@code appName} needs to be whitelisted for the server to accept the crash.
     * <a href="https://bugzilla.mozilla.org/enter_bug.cgi?product=Socorro">File a bug</a>
     * if you would like to get your app added to the whitelist.
     *
     * @param context The current Context
     * @param intent The Intent sent to the {@link GeckoRuntime} crash handler
     * @param appName A human-readable app name.
     * @throws IOException This can be thrown if there was a networking error while sending the report.
     * @throws URISyntaxException This can be thrown if the crash server URI from the extra data was invalid.
     * @return A GeckoResult containing the crash ID as a String.
     * @see GeckoRuntimeSettings.Builder#crashHandler(Class)
     * @see GeckoRuntime#ACTION_CRASHED
     */
    @AnyThread
    public static @NonNull GeckoResult<String> sendCrashReport(@NonNull final Context context,
                                                               @NonNull final Intent intent,
                                                               @NonNull final String appName)
            throws IOException, URISyntaxException {
        return sendCrashReport(context, intent.getExtras(), appName);
    }

    /**
     * Sends a crash report to the Mozilla  <a href="https://wiki.mozilla.org/Socorro">Socorro</a>
     * crash report server.
     * <br>
     * The {@code appName} needs to be whitelisted for the server to accept the crash.
     * <a href="https://bugzilla.mozilla.org/enter_bug.cgi?product=Socorro">File a bug</a>
     * if you would like to get your app added to the whitelist.
     *
     * @param context The current Context
     * @param intentExtras The Bundle of extras attached to the Intent received by a crash handler.
     * @param appName A human-readable app name.
     * @throws IOException This can be thrown if there was a networking error while sending the report.
     * @throws URISyntaxException This can be thrown if the crash server URI from the extra data was invalid.
     * @return A GeckoResult containing the crash ID as a String.
     * @see GeckoRuntimeSettings.Builder#crashHandler(Class)
     * @see GeckoRuntime#ACTION_CRASHED
     */
    @AnyThread
    public static @NonNull GeckoResult<String> sendCrashReport(@NonNull final Context context,
                                                               @NonNull final Bundle intentExtras,
                                                               @NonNull final String appName)
            throws IOException, URISyntaxException {
        final File dumpFile = new File(intentExtras.getString(GeckoRuntime.EXTRA_MINIDUMP_PATH));
        final File extrasFile = new File(intentExtras.getString(GeckoRuntime.EXTRA_EXTRAS_PATH));
        final boolean success = intentExtras.getBoolean(GeckoRuntime.EXTRA_MINIDUMP_SUCCESS, false);

        return sendCrashReport(context, dumpFile, extrasFile, success, appName);
    }

    /**
     * Sends a crash report to the Mozilla  <a href="https://wiki.mozilla.org/Socorro">Socorro</a>
     * crash report server.
     * <br>
     * The {@code appName} needs to be whitelisted for the server to accept the crash.
     * <a href="https://bugzilla.mozilla.org/enter_bug.cgi?product=Socorro">File a bug</a>
     * if you would like to get your app added to the whitelist.
     *
     * @param context The current {@link Context}
     * @param minidumpFile A {@link File} referring to the minidump.
     * @param extrasFile A {@link File} referring to the extras file.
     * @param success A boolean indicating whether the dump was successfully generated.
     * @param appName A human-readable app name.
     * @throws IOException This can be thrown if there was a networking error while sending the report.
     * @throws URISyntaxException This can be thrown if the crash server URI from the extra data was invalid.
     * @return A GeckoResult containing the crash ID as a String.
     * @see GeckoRuntimeSettings.Builder#crashHandler(Class)
     * @see GeckoRuntime#ACTION_CRASHED
     */
    @AnyThread
    public static @NonNull GeckoResult<String> sendCrashReport(@NonNull final Context context,
                                                               @NonNull final File minidumpFile,
                                                               @NonNull final File extrasFile,
                                                               final boolean success,
                                                               @NonNull final String appName)
            throws IOException, URISyntaxException {
        // Compute the minidump hash and generate the stack traces
        computeMinidumpHash(extrasFile, minidumpFile);

        // Extract the annotations from the .extra file
        HashMap<String, String> extrasMap = readStringsFromFile(extrasFile.getPath());

        return sendCrashReport(context, minidumpFile, extrasMap, success, appName);
    }

    /**
     * Sends a crash report to the Mozilla  <a href="https://wiki.mozilla.org/Socorro">Socorro</a>
     * crash report server.
     *
     * @param context The current {@link Context}
     * @param minidumpFile A {@link File} referring to the minidump.
     * @param extras A {@link HashMap} with the parsed key-value pairs from the extras file.
     * @param success A boolean indicating whether the dump was successfully generated.
     * @param appName A human-readable app name.
     * @throws IOException This can be thrown if there was a networking error while sending the report.
     * @throws URISyntaxException This can be thrown if the crash server URI from the extra data was invalid.
     * @return A GeckoResult containing the crash ID as a String.
     * @see GeckoRuntimeSettings.Builder#crashHandler(Class)
     * @see GeckoRuntime#ACTION_CRASHED
     */
    @AnyThread
    public static @NonNull GeckoResult<String> sendCrashReport(
        @NonNull final Context context, @NonNull final File minidumpFile,
        @NonNull final Map<String, String> extras, final boolean success,
        @NonNull final String appName) throws IOException, URISyntaxException {
        Log.d(LOGTAG, "Sending crash report: " + minidumpFile.getPath());

        String spec = extras.get(SERVER_URL_KEY);
        if (spec == null) {
            return GeckoResult.fromException(new Exception("No server url present"));
        }

        extras.put(PRODUCT_NAME_KEY, appName);
        extras.put(PRODUCT_ID_KEY, PRODUCT_ID);

        HttpURLConnection conn = null;
        try {
            final URL url = new URL(URLDecoder.decode(spec, "UTF-8"));
            final URI uri = new URI(url.getProtocol(), url.getUserInfo(),
                    url.getHost(), url.getPort(),
                    url.getPath(), url.getQuery(), url.getRef());
            conn = (HttpURLConnection) ProxySelector.openConnectionWithProxy(uri);
            conn.setRequestMethod("POST");
            String boundary = generateBoundary();
            conn.setDoOutput(true);
            conn.setRequestProperty("Content-Type", "multipart/form-data; boundary=" + boundary);
            conn.setRequestProperty("Content-Encoding", "gzip");

            OutputStream os = new GZIPOutputStream(conn.getOutputStream());
            for (String key : extras.keySet()) {
                if (IGNORE_KEYS.contains(key)) {
                    Log.d(LOGTAG, "Ignoring: " + key);
                    continue;
                }

                sendPart(os, boundary, key, extras.get(key));
            }

            StringBuilder sb = new StringBuilder();
            sb.append(extras.containsKey(NOTES_KEY) ? extras.get(NOTES_KEY) + "\n" : "");
            sb.append(Build.MANUFACTURER).append(' ')
                    .append(Build.MODEL).append('\n')
                    .append(Build.FINGERPRINT);
            sendPart(os, boundary, NOTES_KEY, sb.toString());

            sendPart(os, boundary, "Android_Manufacturer", Build.MANUFACTURER);
            sendPart(os, boundary, "Android_Model", Build.MODEL);
            sendPart(os, boundary, "Android_Board", Build.BOARD);
            sendPart(os, boundary, "Android_Brand", Build.BRAND);
            sendPart(os, boundary, "Android_Device", Build.DEVICE);
            sendPart(os, boundary, "Android_Display", Build.DISPLAY);
            sendPart(os, boundary, "Android_Fingerprint", Build.FINGERPRINT);
            sendPart(os, boundary, "Android_CPU_ABI", Build.CPU_ABI);
            sendPart(os, boundary, "Android_PackageName", context.getPackageName());
            try {
                sendPart(os, boundary, "Android_CPU_ABI2", Build.CPU_ABI2);
                sendPart(os, boundary, "Android_Hardware", Build.HARDWARE);
            } catch (Exception ex) {
                Log.e(LOGTAG, "Exception while sending SDK version 8 keys", ex);
            }
            sendPart(os, boundary, "Android_Version",  Build.VERSION.SDK_INT + " (" + Build.VERSION.CODENAME + ")");
            sendPart(os, boundary, MINI_DUMP_SUCCESS_KEY, success ? "True" : "False");
            sendFile(os, boundary, MINI_DUMP_PATH_KEY, minidumpFile);
            os.write(("\r\n--" + boundary + "--\r\n").getBytes());
            os.flush();
            os.close();

            BufferedReader br = null;
            try {
                br = new BufferedReader(
                        new InputStreamReader(conn.getInputStream()));
                HashMap<String, String> responseMap = readStringsFromReader(br);

                if (conn.getResponseCode() == HttpURLConnection.HTTP_OK) {
                    String crashid = responseMap.get("CrashID");
                    if (crashid != null) {
                        Log.i(LOGTAG, "Successfully sent crash report: " + crashid);
                        return GeckoResult.fromValue(crashid);
                    } else {
                        Log.i(LOGTAG, "Server rejected crash report");
                    }
                } else {
                    Log.w(LOGTAG, "Received failure HTTP response code from server: " + conn.getResponseCode());
                }
            } catch (Exception e) {
                return GeckoResult.fromException(new Exception("Failed to submit crash report", e));
            } finally {
                try {
                    if (br != null) {
                        br.close();
                    }
                } catch (IOException e) {
                    return GeckoResult.fromException(new Exception("Failed to submit crash report", e));
                }
            }
        } catch (Exception e) {
            return GeckoResult.fromException(new Exception("Failed to submit crash report", e));
        } finally {
            if (conn != null) {
                conn.disconnect();
            }
        }
        return GeckoResult.fromException(new Exception("Failed to submit crash report"));
    }

    private static void computeMinidumpHash(final File extraFile, final File minidump) {
        try {
            FileInputStream stream = new FileInputStream(minidump);
            MessageDigest md = MessageDigest.getInstance("SHA-256");

            try {
                byte[] buffer = new byte[4096];
                int readBytes;

                while ((readBytes = stream.read(buffer)) != -1) {
                    md.update(buffer, 0, readBytes);
                }
            } finally {
                stream.close();
            }

            byte[] digest = md.digest();
            StringBuilder hash = new StringBuilder(84);

            hash.append("MinidumpSha256Hash=");

            for (int i = 0; i < digest.length; i++) {
                hash.append(Integer.toHexString((digest[i] & 0xf0) >> 4));
                hash.append(Integer.toHexString(digest[i] & 0x0f));
            }

            hash.append('\n');

            FileWriter writer = new FileWriter(extraFile, /* append */ true);

            try {
                writer.write(hash.toString());
            } finally {
                writer.close();
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "exception while computing the minidump hash: ", e);
        }
    }

    private static HashMap<String, String> readStringsFromFile(final String filePath)
            throws IOException {
        FileReader fileReader = null;
        BufferedReader bufReader = null;
        try {
            fileReader = new FileReader(filePath);
            bufReader = new BufferedReader(fileReader);
            return readStringsFromReader(bufReader);
        } finally {
            try {
                if (fileReader != null) {
                    fileReader.close();
                }

                if (bufReader != null) {
                    bufReader.close();
                }
            } catch (IOException e) {
            }
        }
    }

    private static HashMap<String, String> readStringsFromReader(final BufferedReader reader)
            throws IOException {
        String line;
        HashMap<String, String> map = new HashMap<>();
        while ((line = reader.readLine()) != null) {
            int equalsPos = -1;
            if ((equalsPos = line.indexOf('=')) != -1) {
                String key = line.substring(0, equalsPos);
                String val = unescape(line.substring(equalsPos + 1));
                map.put(key, val);
            }
        }
        return map;
    }

    private static String generateBoundary() {
        // Generate some random numbers to fill out the boundary
        int r0 = (int)(Integer.MAX_VALUE * Math.random());
        int r1 = (int)(Integer.MAX_VALUE * Math.random());
        return String.format("---------------------------%08X%08X", r0, r1);
    }

    private static void sendPart(final OutputStream os, final String boundary, final String name,
                                 final String data) {
        try {
            os.write(("--" + boundary + "\r\n" +
                    "Content-Disposition: form-data; name=\"" + name + "\"\r\n" +
                    "\r\n" +
                    data + "\r\n"
            ).getBytes());
        } catch (Exception ex) {
            Log.e(LOGTAG, "Exception when sending \"" + name + "\"", ex);
        }
    }

    private static void sendFile(final OutputStream os, final String boundary, final String name,
                                 final File file) throws IOException {
        os.write(("--" + boundary + "\r\n" +
                "Content-Disposition: form-data; name=\"" + name + "\"; " +
                "filename=\"" + file.getName() + "\"\r\n" +
                "Content-Type: application/octet-stream\r\n" +
                "\r\n"
        ).getBytes());
        FileChannel fc = new FileInputStream(file).getChannel();
        fc.transferTo(0, fc.size(), Channels.newChannel(os));
        fc.close();
    }

    private static String unescape(final String string) {
        return string.replaceAll("\\\\\\\\", "\\").replaceAll("\\\\n", "\n").replaceAll("\\\\t", "\t");
    }
}
