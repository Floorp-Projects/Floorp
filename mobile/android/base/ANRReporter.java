/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONObject;

import android.app.ActivityManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Base64;
import android.util.Log;

import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.Reader;
import java.nio.charset.Charset;
import java.security.GeneralSecurityException;
import java.security.MessageDigest;
import java.util.UUID;
import java.util.Locale;
import java.util.regex.Pattern;

public final class ANRReporter extends BroadcastReceiver
{
    private static final boolean DEBUG = false;
    private static final String LOGTAG = "GeckoANRReporter";

    private static final String ANR_ACTION = "android.intent.action.ANR";
    // Number of lines to search traces.txt to decide whether it's a Gecko ANR
    private static final int LINES_TO_IDENTIFY_TRACES = 10;
    // ANRs may happen because of memory pressure,
    //  so don't use up too much memory here
    // Size of buffer to hold one line of text
    private static final int TRACES_LINE_SIZE = 100;
    // Size of block to use when processing traces.txt
    private static final int TRACES_BLOCK_SIZE = 2000;
    // we use us-ascii here because when telemetry calculates checksum
    // for the ping file, it lossily converts utf-16 to ascii. therefore,
    // we have to treat characters in the traces file as ascii rather than
    // say utf-8. otherwise, we will get a wrong checksum
    private static final Charset TRACES_CHARSET = Charset.forName("us-ascii");
    private static final Charset PING_CHARSET = Charset.forName("us-ascii");

    private static final ANRReporter sInstance = new ANRReporter();
    private static int sRegisteredCount;
    private Handler mHandler;
    private volatile boolean mPendingANR;

    public static void register(Context context) {
        if (sRegisteredCount++ != 0) {
            // Already registered
            return;
        }
        sInstance.start(context);
    }

    public static void unregister() {
        if (sRegisteredCount == 0) {
            Log.w(LOGTAG, "register/unregister mismatch");
            return;
        }
        if (--sRegisteredCount != 0) {
            // Should still be registered
            return;
        }
        sInstance.stop();
    }

    private void start(final Context context) {

        Thread receiverThread = new Thread(new Runnable() {
            @Override
            public void run() {
                Looper.prepare();
                synchronized (ANRReporter.this) {
                    mHandler = new Handler();
                    ANRReporter.this.notify();
                }
                if (DEBUG) {
                    Log.d(LOGTAG, "registering receiver");
                }
                context.registerReceiver(ANRReporter.this,
                                         new IntentFilter(ANR_ACTION),
                                         null,
                                         mHandler);
                Looper.loop();

                if (DEBUG) {
                    Log.d(LOGTAG, "unregistering receiver");
                }
                context.unregisterReceiver(ANRReporter.this);
                mHandler = null;
            }
        }, LOGTAG);

        receiverThread.setDaemon(true);
        receiverThread.start();
    }

    private void stop() {
        synchronized (this) {
            while (mHandler == null) {
                try {
                    wait(1000);
                    if (mHandler == null) {
                        // We timed out; just give up. The process is probably
                        // quitting anyways, so we let the OS do the clean up
                        Log.w(LOGTAG, "timed out waiting for handler");
                        return;
                    }
                } catch (InterruptedException e) {
                }
            }
        }
        Looper looper = mHandler.getLooper();
        looper.quit();
        try {
            looper.getThread().join();
        } catch (InterruptedException e) {
        }
    }

    private ANRReporter() {
    }

    // Return the "traces.txt" file, or null if there is no such file
    private static File getTracesFile() {
        try {
            // getprop [prop-name [default-value]]
            Process propProc = (new ProcessBuilder())
                .command("/system/bin/getprop", "dalvik.vm.stack-trace-file")
                .redirectErrorStream(true)
                .start();
            try {
                BufferedReader buf = new BufferedReader(
                    new InputStreamReader(propProc.getInputStream()), TRACES_LINE_SIZE);
                String propVal = buf.readLine();
                if (DEBUG) {
                    Log.d(LOGTAG, "getprop returned " + String.valueOf(propVal));
                }
                // getprop can return empty string when the prop value is empty
                // or prop is undefined, treat both cases the same way
                if (propVal != null && propVal.length() != 0) {
                    File tracesFile = new File(propVal);
                    if (tracesFile.isFile() && tracesFile.canRead()) {
                        return tracesFile;
                    } else if (DEBUG) {
                        Log.d(LOGTAG, "cannot access traces file");
                    }
                } else if (DEBUG) {
                    Log.d(LOGTAG, "empty getprop result");
                }
            } finally {
                propProc.destroy();
            }
        } catch (IOException e) {
            Log.w(LOGTAG, e);
        }
        // Check most common location one last time just in case
        File tracesFile = new File("/data/anr/traces.txt");
        if (tracesFile.isFile() && tracesFile.canRead()) {
            return tracesFile;
        }
        return null;
    }

    private static File getPingFile() {
        if (GeckoApp.mAppContext == null) {
            return null;
        }
        GeckoProfile profile = GeckoApp.mAppContext.getProfile();
        if (profile == null) {
            return null;
        }
        File profDir = profile.getDir();
        if (profDir == null) {
            return null;
        }
        File pingDir = new File(profDir, "saved-telemetry-pings");
        pingDir.mkdirs();
        if (!(pingDir.exists() && pingDir.isDirectory())) {
            return null;
        }
        return new File(pingDir, UUID.randomUUID().toString());
    }

    // Return true if the traces file corresponds to a Gecko ANR
    private static boolean isGeckoTraces(String pkgName, File tracesFile) {
        try {
            final String END_OF_PACKAGE_NAME = "([^a-zA-Z0-9_]|$)";
            // Regex for finding our package name in the traces file
            Pattern pkgPattern = Pattern.compile(Pattern.quote(pkgName) + END_OF_PACKAGE_NAME);
            Pattern mangledPattern = null;
            if (!AppConstants.MANGLED_ANDROID_PACKAGE_NAME.equals(pkgName)) {
                mangledPattern = Pattern.compile(Pattern.quote(
                    AppConstants.MANGLED_ANDROID_PACKAGE_NAME) + END_OF_PACKAGE_NAME);
            }
            if (DEBUG) {
                Log.d(LOGTAG, "trying to match package: " + pkgName);
            }
            BufferedReader traces = new BufferedReader(
                new FileReader(tracesFile), TRACES_BLOCK_SIZE);
            try {
                for (int count = 0; count < LINES_TO_IDENTIFY_TRACES; count++) {
                    String line = traces.readLine();
                    if (DEBUG) {
                        Log.d(LOGTAG, "identifying line: " + String.valueOf(line));
                    }
                    if (pkgPattern.matcher(line).find()) {
                        // traces.txt file contains our package
                        return true;
                    }
                    if (mangledPattern != null && mangledPattern.matcher(line).find()) {
                        // traces.txt file contains our alternate package
                        return true;
                    }
                }
            } finally {
                traces.close();
            }
        } catch (IOException e) {
            // meh, can't even read from it right. just return false
        }
        return false;
    }

    private static long getUptimeMins() {

        long uptimeMins = (new File("/proc/self/stat")).lastModified();
        if (uptimeMins != 0L) {
            uptimeMins = (System.currentTimeMillis() - uptimeMins) / 1000L / 60L;
            if (DEBUG) {
                Log.d(LOGTAG, "uptime " + String.valueOf(uptimeMins));
            }
            return uptimeMins;
        } else if (DEBUG) {
            Log.d(LOGTAG, "could not get uptime");
        }
        return 0L;
    }

    private static long getTotalMem() {

        if (Build.VERSION.SDK_INT >= 16 && GeckoApp.mAppContext != null) {
            ActivityManager am = (ActivityManager)
                GeckoApp.mAppContext.getSystemService(Context.ACTIVITY_SERVICE);
            ActivityManager.MemoryInfo mi = new ActivityManager.MemoryInfo();
            am.getMemoryInfo(mi);
            mi.totalMem /= 1024L * 1024L;
            if (DEBUG) {
                Log.d(LOGTAG, "totalMem " + String.valueOf(mi.totalMem));
            }
            return mi.totalMem;
        } else if (DEBUG) {
            Log.d(LOGTAG, "totalMem unavailable");
        }
        return 0L;
    }

    private static String getLocale() {
        // Having a different locale than system locale is not
        // supported right now; assume we are using the system locale
        return Locale.getDefault().toString().replace('_', '-');
    }

    /*
        a saved telemetry ping file consists of JSON in the following format,
            {
                "slug": "<uuid-string>",
                "payload": "<escaped-json-data-string>",
                "checksum": "<base64-sha-256-string>"
            }
        for Android ANR, our unescaped JSON payload should look like,
            {
                "ver": 1,
                "simpleMeasurements": {
                    "uptime": <uptime>
                },
                "info": {
                    "reason": "android-anr-report",
                    "OS": "Android",
                    ...
                },
                "androidANR": "...",
                "androidLogcat": "..."
            }
    */

    private static int writePingPayload(OutputStream ping,
                                        MessageDigest checksum,
                                        String payload) throws IOException {

        byte [] data = payload.getBytes(PING_CHARSET);
        checksum.update(data);

        data = JSONObject.quote(payload).getBytes(PING_CHARSET);
        // first and last bytes are quotes inserted by JSONObject.quote; discard them
        ping.write(data, 1, data.length - 2);
        return data.length - 2;
    }

    private static void fillPingHeader(OutputStream ping, MessageDigest checksum, String slug)
            throws IOException {

        // ping file header
        byte [] data = ("{" +
            "\"slug\":" + JSONObject.quote(slug) + "," +
            "\"payload\":\"").getBytes(PING_CHARSET);
        ping.write(data);
        if (DEBUG) {
            Log.d(LOGTAG, "wrote ping header, size = " + String.valueOf(data.length));
        }

        // payload start
        int size = writePingPayload(ping, checksum, ("{" +
            "\"ver\":1," +
            "\"simpleMeasurements\":{" +
                "\"uptime\":" + String.valueOf(getUptimeMins()) +
            "}," +
            "\"info\":{" +
                "\"reason\":\"android-anr-report\"," +
                "\"OS\":" + JSONObject.quote(AppConstants.OS_TARGET) + "," +
                "\"version\":\"" + String.valueOf(Build.VERSION.SDK_INT) + "\"," +
                "\"appID\":" + JSONObject.quote(AppConstants.MOZ_APP_ID) + "," +
                "\"appVersion\":" + JSONObject.quote(AppConstants.MOZ_APP_VERSION)+ "," +
                "\"appName\":" + JSONObject.quote(AppConstants.MOZ_APP_BASENAME) + "," +
                "\"appBuildID\":" + JSONObject.quote(AppConstants.MOZ_APP_BUILDID) + "," +
                "\"appUpdateChannel\":" + JSONObject.quote(AppConstants.MOZ_UPDATE_CHANNEL) + "," +
                // Technically the platform build ID may be different, but we'll never know
                "\"platformBuildID\":" + JSONObject.quote(AppConstants.MOZ_APP_BUILDID) + "," +
                "\"locale\":" + JSONObject.quote(getLocale()) + "," +
                "\"cpucount\":" + String.valueOf(Runtime.getRuntime().availableProcessors()) + "," +
                "\"memsize\":" + String.valueOf(getTotalMem()) + "," +
                "\"arch\":" + JSONObject.quote(System.getProperty("os.arch")) + "," +
                "\"kernel_version\":" + JSONObject.quote(System.getProperty("os.version")) + "," +
                "\"device\":" + JSONObject.quote(Build.MODEL) + "," +
                "\"manufacturer\":" + JSONObject.quote(Build.MANUFACTURER) + "," +
                "\"hardware\":" + JSONObject.quote(Build.VERSION.SDK_INT < 8 ? "" :
                                                   Build.HARDWARE) +
            "}," +
            "\"androidANR\":\""));
        if (DEBUG) {
            Log.d(LOGTAG, "wrote metadata, size = " + String.valueOf(size));
        }

        // We are at the start of ANR data
    }

    // Block is a section of the larger input stream, and we want to find pattern within
    // the stream. This is straightforward if the entire pattern is within one block;
    // however, if the pattern spans across two blocks, we have to match both the start of
    // the pattern in the first block and the end of the pattern in the second block.
    // * If pattern is found in block, this method returns the index at the end of the
    //   found pattern, which must always be > 0.
    // * If pattern is not found, it returns 0.
    // * If the start of the pattern matches the end of the block, it returns a number
    //   < 0, which equals the negated value of how many characters in pattern are already
    //   matched; when processing the next block, this number is passed in through
    //   prevIndex, and the rest of the characters in pattern are matched against the
    //   start of this second block. The method returns value > 0 if the rest of the
    //   characters match, or 0 if they do not.
    private static int getEndPatternIndex(String block, String pattern, int prevIndex) {
        if (pattern == null || block.length() < pattern.length()) {
            // Nothing to do
            return 0;
        }
        if (prevIndex == 0) {
            // Did not find pattern in last block; see if entire pattern is inside this block
            int index = block.indexOf(pattern);
            if (index >= 0) {
                // Found pattern; return index at end of the pattern
                return index + pattern.length();
            }
            // Block does not contain the entire pattern, but see if the end of the block
            // contains the start of pattern. To do that, we see if block ends with the
            // first 1 character of pattern, the first 2 characters of pattern, first 3,
            // and so on.
            for (index = block.length() - 1; index > block.length() - pattern.length(); index--) {
                // Using index as a start, see if the rest of block contains the start of pattern
                if (block.charAt(index) == pattern.charAt(0) &&
                    block.endsWith(pattern.substring(0, block.length() - index))) {
                    // Found partial match; return -(number of characters matched),
                    // i.e. -1 for 1 character matched, -2 for 2 characters matched, etc.
                    return index - block.length();
                }
            }
        } else if (prevIndex < 0) {
            // Last block ended with a partial start; now match start of block to rest of pattern
            if (block.startsWith(pattern.substring(-prevIndex, pattern.length()))) {
                // Rest of pattern matches; return index at end of pattern
                return pattern.length() + prevIndex;
            }
        }
        return 0;
    }

    // Copy the content of reader to ping and update checksum;
    // copying stops when endPattern is found in the input stream
    private static int fillPingBlock(OutputStream ping, MessageDigest checksum,
                                     Reader reader, String endPattern)
            throws IOException {

        int total = 0;
        int endIndex = 0;
        char [] block = new char[TRACES_BLOCK_SIZE];
        for (int size = reader.read(block); size >= 0; size = reader.read(block)) {
            String stringBlock = new String(block, 0, size);
            endIndex = getEndPatternIndex(stringBlock, endPattern, endIndex);
            if (endIndex > 0) {
                // Found end pattern; clip the string
                stringBlock = stringBlock.substring(0, endIndex);
            }
            String quoted = JSONObject.quote(stringBlock);
            total += writePingPayload(ping, checksum, quoted.substring(1, quoted.length() - 1));
            if (endIndex > 0) {
                // End pattern already found; return now
                break;
            }
        }
        return total;
    }

    private static void fillPingFooter(OutputStream ping, MessageDigest checksum)
            throws IOException {

        // We are at the end of ANR data

        int total = writePingPayload(ping, checksum, ("\"," +
                "\"androidLogcat\":\""));

        try {
            // get the last 200 lines of logcat
            Process proc = (new ProcessBuilder())
                .command("/system/bin/logcat", "-v", "threadtime", "-t", "200", "-d", "*:D")
                .redirectErrorStream(true)
                .start();
            try {
                Reader procOut = new InputStreamReader(proc.getInputStream(), TRACES_CHARSET);
                int size = fillPingBlock(ping, checksum, procOut, null);
                if (DEBUG) {
                    Log.d(LOGTAG, "wrote logcat, size = " + String.valueOf(size));
                }
            } finally {
                proc.destroy();
            }
        } catch (IOException e) {
            // ignore because logcat is not essential
            Log.w(LOGTAG, e);
        }

        total += writePingPayload(ping, checksum, "\"}");

        String base64Checksum = Base64.encodeToString(checksum.digest(), Base64.NO_WRAP);
        if (DEBUG) {
            Log.d(LOGTAG, "checksum: " + base64Checksum);
        }
        byte [] data = (
                "\"," +
                "\"checksum\":" + JSONObject.quote(base64Checksum) +
            "}").getBytes(PING_CHARSET);
        ping.write(data);
        if (DEBUG) {
            Log.d(LOGTAG, "wrote ping footer, size = " + String.valueOf(data.length + total));
        }
    }

    private static void processTraces(Reader traces, File pingFile) {
        try {
            OutputStream ping = new BufferedOutputStream(
                new FileOutputStream(pingFile), TRACES_BLOCK_SIZE);
            try {
                MessageDigest checksum = MessageDigest.getInstance("SHA-256");
                fillPingHeader(ping, checksum, pingFile.getName());
                // Traces file has the format
                //    ----- pid xxx at xxx -----
                //    Cmd line: org.mozilla.xxx
                //    * stack trace *
                //    ----- end xxx -----
                //    ----- pid xxx at xxx -----
                //    Cmd line: com.android.xxx
                //    * stack trace *
                //    ...
                // If we end the stack dump at the first end marker,
                // only Fennec stacks will be dumped
                int size = fillPingBlock(ping, checksum, traces, "\n----- end");
                if (DEBUG) {
                    Log.d(LOGTAG, "wrote traces, size = " + String.valueOf(size));
                }
                fillPingFooter(ping, checksum);
                if (DEBUG) {
                    Log.d(LOGTAG, "finished creating ping file");
                }
                return;
            } finally {
                ping.close();
            }
        } catch (GeneralSecurityException e) {
            Log.w(LOGTAG, e);
        } catch (IOException e) {
            Log.w(LOGTAG, e);
        }
        // exception; delete ping file
        if (pingFile.exists()) {
            pingFile.delete();
        }
    }

    private static void processTraces(File tracesFile, File pingFile) {
        try {
            Reader traces = new InputStreamReader(
                new FileInputStream(tracesFile), TRACES_CHARSET);
            try {
                processTraces(traces, pingFile);
            } finally {
                traces.close();
            }
        } catch (IOException e) {
            Log.w(LOGTAG, e);
        }
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (mPendingANR) {
            // we already processed an ANR without getting unstuck; skip this one
            if (DEBUG) {
                Log.d(LOGTAG, "skipping duplicate ANR");
            }
            return;
        }
        if (ThreadUtils.getUiHandler() != null) {
            mPendingANR = true;
            // detect when the main thread gets unstuck
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    // okay to reset mPendingANR on main thread
                    mPendingANR = false;
                    if (DEBUG) {
                        Log.d(LOGTAG, "yay we got unstuck!");
                    }
                }
            });
        }
        if (DEBUG) {
            Log.d(LOGTAG, "receiving " + String.valueOf(intent));
        }
        if (!ANR_ACTION.equals(intent.getAction())) {
            return;
        }

        // make sure we have a good save location first
        File pingFile = getPingFile();
        if (DEBUG) {
            Log.d(LOGTAG, "using ping file: " + String.valueOf(pingFile));
        }
        if (pingFile == null) {
            return;
        }

        File tracesFile = getTracesFile();
        if (DEBUG) {
            Log.d(LOGTAG, "using traces file: " + String.valueOf(tracesFile));
        }
        if (tracesFile == null) {
            return;
        }

        // We get ANR intents from all ANRs in the system, but we only want Gecko ANRs
        if (!isGeckoTraces(context.getPackageName(), tracesFile)) {
            if (DEBUG) {
                Log.d(LOGTAG, "traces is not Gecko ANR");
            }
            return;
        }
        Log.i(LOGTAG, "processing Gecko ANR");
        processTraces(tracesFile, pingFile);
    }
}
