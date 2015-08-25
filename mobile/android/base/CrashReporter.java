/* -*- Mode: Java; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.HashMap;
import java.util.Map;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.channels.Channels;
import java.nio.channels.FileChannel;
import java.util.zip.GZIPOutputStream;

import org.mozilla.gecko.AppConstants.Versions;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.EditText;

public class CrashReporter extends Activity
{
    private static final String LOGTAG = "GeckoCrashReporter";

    private static final String PASSED_MINI_DUMP_KEY = "minidumpPath";
    private static final String MINI_DUMP_PATH_KEY = "upload_file_minidump";
    private static final String PAGE_URL_KEY = "URL";
    private static final String NOTES_KEY = "Notes";
    private static final String SERVER_URL_KEY = "ServerURL";

    private static final String CRASH_REPORT_SUFFIX = "/mozilla/Crash Reports/";
    private static final String PENDING_SUFFIX = CRASH_REPORT_SUFFIX + "pending";
    private static final String SUBMITTED_SUFFIX = CRASH_REPORT_SUFFIX + "submitted";

    private static final String PREFS_SEND_REPORT   = "sendReport";
    private static final String PREFS_INCLUDE_URL   = "includeUrl";
    private static final String PREFS_ALLOW_CONTACT = "allowContact";
    private static final String PREFS_CONTACT_EMAIL = "contactEmail";

    private Handler mHandler;
    private ProgressDialog mProgressDialog;
    private File mPendingMinidumpFile;
    private File mPendingExtrasFile;
    private HashMap<String, String> mExtrasStringMap;

    private boolean moveFile(File inFile, File outFile) {
        Log.i(LOGTAG, "moving " + inFile + " to " + outFile);
        if (inFile.renameTo(outFile))
            return true;
        try {
            outFile.createNewFile();
            Log.i(LOGTAG, "couldn't rename minidump file");
            // so copy it instead
            FileChannel inChannel = new FileInputStream(inFile).getChannel();
            FileChannel outChannel = new FileOutputStream(outFile).getChannel();
            long transferred = inChannel.transferTo(0, inChannel.size(), outChannel);
            inChannel.close();
            outChannel.close();

            if (transferred > 0)
                inFile.delete();
        } catch (Exception e) {
            Log.e(LOGTAG, "exception while copying minidump file: ", e);
            return false;
        }
        return true;
    }

    private void doFinish() {
        if (mHandler != null) {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    finish();
                }
            });
        }
    }

    @Override
    public void finish() {
        try {
            if (mProgressDialog.isShowing()) {
                mProgressDialog.dismiss();
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "exception while closing progress dialog: ", e);
        }
        super.finish();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // mHandler is created here so runnables can be run on the main thread
        mHandler = new Handler();
        setContentView(R.layout.crash_reporter);
        mProgressDialog = new ProgressDialog(this);
        mProgressDialog.setMessage(getString(R.string.sending_crash_report));

        String passedMinidumpPath = getIntent().getStringExtra(PASSED_MINI_DUMP_KEY);
        File passedMinidumpFile = new File(passedMinidumpPath);
        File pendingDir = new File(getFilesDir(), PENDING_SUFFIX);
        pendingDir.mkdirs();
        mPendingMinidumpFile = new File(pendingDir, passedMinidumpFile.getName());
        moveFile(passedMinidumpFile, mPendingMinidumpFile);

        File extrasFile = new File(passedMinidumpPath.replaceAll(".dmp", ".extra"));
        mPendingExtrasFile = new File(pendingDir, extrasFile.getName());
        moveFile(extrasFile, mPendingExtrasFile);

        mExtrasStringMap = new HashMap<String, String>();
        readStringsFromFile(mPendingExtrasFile.getPath(), mExtrasStringMap);

        // Set the flag that indicates we were stopped as expected, as
        // we will send a crash report, so it is not a silent OOM crash.
        SharedPreferences prefs = GeckoSharedPrefs.forApp(this);
        SharedPreferences.Editor editor = prefs.edit();
        editor.putBoolean(GeckoApp.PREFS_WAS_STOPPED, true);
        editor.putBoolean(GeckoApp.PREFS_CRASHED, true);
        editor.apply();

        final CheckBox allowContactCheckBox = (CheckBox) findViewById(R.id.allow_contact);
        final CheckBox includeUrlCheckBox = (CheckBox) findViewById(R.id.include_url);
        final CheckBox sendReportCheckBox = (CheckBox) findViewById(R.id.send_report);
        final EditText commentsEditText = (EditText) findViewById(R.id.comment);
        final EditText emailEditText = (EditText) findViewById(R.id.email);

        // Load CrashReporter preferences to avoid redundant user input.
        final boolean sendReport   = prefs.getBoolean(PREFS_SEND_REPORT, true);
        final boolean includeUrl   = prefs.getBoolean(PREFS_INCLUDE_URL, false);
        final boolean allowContact = prefs.getBoolean(PREFS_ALLOW_CONTACT, false);
        final String contactEmail  = prefs.getString(PREFS_CONTACT_EMAIL, "");

        allowContactCheckBox.setChecked(allowContact);
        includeUrlCheckBox.setChecked(includeUrl);
        sendReportCheckBox.setChecked(sendReport);
        emailEditText.setText(contactEmail);

        sendReportCheckBox.setOnCheckedChangeListener(new CheckBox.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton checkbox, boolean isChecked) {
                commentsEditText.setEnabled(isChecked);
                commentsEditText.requestFocus();

                includeUrlCheckBox.setEnabled(isChecked);
                allowContactCheckBox.setEnabled(isChecked);
                emailEditText.setEnabled(isChecked && allowContactCheckBox.isChecked());
            }
        });

        allowContactCheckBox.setOnCheckedChangeListener(new CheckBox.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton checkbox, boolean isChecked) {
                // We need to check isEnabled() here because this listener is
                // fired on rotation -- even when the checkbox is disabled.
                emailEditText.setEnabled(checkbox.isEnabled() && isChecked);
                emailEditText.requestFocus();
            }
        });

        emailEditText.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // Even if the email EditText is disabled, allow it to be
                // clicked and focused.
                if (sendReportCheckBox.isChecked() && !v.isEnabled()) {
                    allowContactCheckBox.setChecked(true);
                    v.setEnabled(true);
                    v.requestFocus();
                }
            }
        });
    }

    @Override
    public void onBackPressed() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setMessage(R.string.crash_closing_alert);
        builder.setNegativeButton(R.string.button_cancel, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                dialog.dismiss();
            }
        });
        builder.setPositiveButton(R.string.button_ok, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                CrashReporter.this.finish();
            }
        });
        builder.show();
    }

    private void backgroundSendReport() {
        final CheckBox sendReportCheckbox = (CheckBox) findViewById(R.id.send_report);
        if (!sendReportCheckbox.isChecked()) {
            doFinish();
            return;
        }

        // Persist settings to avoid redundant user input.
        savePrefs();

        mProgressDialog.show();
        new Thread(new Runnable() {
            @Override
            public void run() {
                sendReport(mPendingMinidumpFile, mExtrasStringMap, mPendingExtrasFile);
            }
        }, "CrashReporter Thread").start();
    }

    private void savePrefs() {
        SharedPreferences.Editor editor = GeckoSharedPrefs.forApp(this).edit();
                  
        final boolean allowContact = ((CheckBox) findViewById(R.id.allow_contact)).isChecked();
        final boolean includeUrl   = ((CheckBox) findViewById(R.id.include_url)).isChecked();
        final boolean sendReport   = ((CheckBox) findViewById(R.id.send_report)).isChecked();
        final String contactEmail  = ((EditText) findViewById(R.id.email)).getText().toString();
                   
        editor.putBoolean(PREFS_ALLOW_CONTACT, allowContact);
        editor.putBoolean(PREFS_INCLUDE_URL, includeUrl);
        editor.putBoolean(PREFS_SEND_REPORT, sendReport);
        editor.putString(PREFS_CONTACT_EMAIL, contactEmail);
                    
        // A slight performance improvement via async apply() vs. blocking on commit().
        editor.apply();
    }

    public void onCloseClick(View v) {  // bound via crash_reporter.xml
        backgroundSendReport();
    }

    public void onRestartClick(View v) {  // bound via crash_reporter.xml
        doRestart();
        backgroundSendReport();
    }

    private boolean readStringsFromFile(String filePath, Map<String, String> stringMap) {
        try {
            BufferedReader reader = new BufferedReader(new FileReader(filePath));
            return readStringsFromReader(reader, stringMap);
        } catch (Exception e) {
            Log.e(LOGTAG, "exception while reading strings: ", e);
            return false;
        }
    }

    private boolean readStringsFromReader(BufferedReader reader, Map<String, String> stringMap) throws IOException {
        String line;
        while ((line = reader.readLine()) != null) {
            int equalsPos = -1;
            if ((equalsPos = line.indexOf('=')) != -1) {
                String key = line.substring(0, equalsPos);
                String val = unescape(line.substring(equalsPos + 1));
                stringMap.put(key, val);
            }
        }
        reader.close();
        return true;
    }

    private String generateBoundary() {
        // Generate some random numbers to fill out the boundary
        int r0 = (int)(Integer.MAX_VALUE * Math.random());
        int r1 = (int)(Integer.MAX_VALUE * Math.random());
        return String.format("---------------------------%08X%08X", r0, r1);
    }

    private void sendPart(OutputStream os, String boundary, String name, String data) {
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

    private void sendFile(OutputStream os, String boundary, String name, File file) throws IOException {
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

    private String readLogcat() {
        BufferedReader br = null;
        try {
            // get the last 200 lines of logcat
            Process proc = Runtime.getRuntime().exec(new String[] {
                "logcat", "-v", "threadtime", "-t", "200", "-d", "*:D"
            });
            StringBuilder sb = new StringBuilder();
            br = new BufferedReader(new InputStreamReader(proc.getInputStream()));
            for (String s = br.readLine(); s != null; s = br.readLine()) {
                sb.append(s).append('\n');
            }
            return sb.toString();
        } catch (Exception e) {
            return "Unable to get logcat: " + e.toString();
        } finally {
            if (br != null) {
                try {
                    br.close();
                } catch (Exception e) {
                    // ignore
                }
            }
        }
    }

    private void sendReport(File minidumpFile, Map<String, String> extras, File extrasFile) {
        Log.i(LOGTAG, "sendReport: " + minidumpFile.getPath());
        final CheckBox includeURLCheckbox = (CheckBox) findViewById(R.id.include_url);

        String spec = extras.get(SERVER_URL_KEY);
        if (spec == null) {
            doFinish();
            return;
        }

        Log.i(LOGTAG, "server url: " + spec);
        try {
            URL url = new URL(spec);
            HttpURLConnection conn = (HttpURLConnection)url.openConnection();
            conn.setRequestMethod("POST");
            String boundary = generateBoundary();
            conn.setDoOutput(true);
            conn.setRequestProperty("Content-Type", "multipart/form-data; boundary=" + boundary);
            conn.setRequestProperty("Content-Encoding", "gzip");

            OutputStream os = new GZIPOutputStream(conn.getOutputStream());
            for (String key : extras.keySet()) {
                if (key.equals(PAGE_URL_KEY)) {
                    if (includeURLCheckbox.isChecked())
                        sendPart(os, boundary, key, extras.get(key));
                } else if (!key.equals(SERVER_URL_KEY) && !key.equals(NOTES_KEY)) {
                    sendPart(os, boundary, key, extras.get(key));
                }
            }

            // Add some extra information to notes so its displayed by
            // crash-stats.mozilla.org. Remove this when bug 607942 is fixed.
            StringBuilder sb = new StringBuilder();
            sb.append(extras.containsKey(NOTES_KEY) ? extras.get(NOTES_KEY) + "\n" : "");
            if (AppConstants.MOZ_MIN_CPU_VERSION < 7) {
                sb.append("nothumb Build\n");
            }
            sb.append(Build.MANUFACTURER).append(' ')
              .append(Build.MODEL).append('\n')
              .append(Build.FINGERPRINT);
            sendPart(os, boundary, NOTES_KEY, sb.toString());

            sendPart(os, boundary, "Min_ARM_Version", Integer.toString(AppConstants.MOZ_MIN_CPU_VERSION));
            sendPart(os, boundary, "Android_Manufacturer", Build.MANUFACTURER);
            sendPart(os, boundary, "Android_Model", Build.MODEL);
            sendPart(os, boundary, "Android_Board", Build.BOARD);
            sendPart(os, boundary, "Android_Brand", Build.BRAND);
            sendPart(os, boundary, "Android_Device", Build.DEVICE);
            sendPart(os, boundary, "Android_Display", Build.DISPLAY);
            sendPart(os, boundary, "Android_Fingerprint", Build.FINGERPRINT);
            sendPart(os, boundary, "Android_APP_ABI", AppConstants.MOZ_APP_ABI);
            sendPart(os, boundary, "Android_CPU_ABI", Build.CPU_ABI);
            sendPart(os, boundary, "Android_MIN_SDK", Integer.toString(AppConstants.Versions.MIN_SDK_VERSION));
            sendPart(os, boundary, "Android_MAX_SDK", Integer.toString(AppConstants.Versions.MAX_SDK_VERSION));
            try {
                sendPart(os, boundary, "Android_CPU_ABI2", Build.CPU_ABI2);
                sendPart(os, boundary, "Android_Hardware", Build.HARDWARE);
            } catch (Exception ex) {
                Log.e(LOGTAG, "Exception while sending SDK version 8 keys", ex);
            }
            sendPart(os, boundary, "Android_Version",  Build.VERSION.SDK_INT + " (" + Build.VERSION.CODENAME + ")");
            if (Versions.feature16Plus && includeURLCheckbox.isChecked()) {
                sendPart(os, boundary, "Android_Logcat", readLogcat());
            }

            String comment = ((EditText) findViewById(R.id.comment)).getText().toString();
            if (!TextUtils.isEmpty(comment)) {
                sendPart(os, boundary, "Comments", comment);
            }

            if (((CheckBox) findViewById(R.id.allow_contact)).isChecked()) {
                String email = ((EditText) findViewById(R.id.email)).getText().toString();
                sendPart(os, boundary, "Email", email);
            }

            sendFile(os, boundary, MINI_DUMP_PATH_KEY, minidumpFile);
            os.write(("\r\n--" + boundary + "--\r\n").getBytes());
            os.flush();
            os.close();
            BufferedReader br = new BufferedReader(
                new InputStreamReader(conn.getInputStream()));
            HashMap<String, String>  responseMap = new HashMap<String, String>();
            readStringsFromReader(br, responseMap);

            if (conn.getResponseCode() == HttpURLConnection.HTTP_OK) {
                File submittedDir = new File(getFilesDir(),
                                             SUBMITTED_SUFFIX);
                submittedDir.mkdirs();
                minidumpFile.delete();
                extrasFile.delete();
                String crashid = responseMap.get("CrashID");
                File file = new File(submittedDir, crashid + ".txt");
                FileOutputStream fos = new FileOutputStream(file);
                fos.write("Crash ID: ".getBytes());
                fos.write(crashid.getBytes());
                fos.close();
            } else {
                Log.i(LOGTAG, "Received failure HTTP response code from server: " + conn.getResponseCode());
            }
        } catch (IOException e) {
            Log.e(LOGTAG, "exception during send: ", e);
        }

        doFinish();
    }

    private void doRestart() {
        try {
            String action = "android.intent.action.MAIN";
            Intent intent = new Intent(action);
            intent.setClassName(AppConstants.ANDROID_PACKAGE_NAME,
                                AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);
            intent.putExtra("didRestart", true);
            Log.i(LOGTAG, intent.toString());
            startActivity(intent);
        } catch (Exception e) {
            Log.e(LOGTAG, "error while trying to restart", e);
        }
    }

    private String unescape(String string) {
        return string.replaceAll("\\\\\\\\", "\\").replaceAll("\\\\n", "\n").replaceAll("\\\\t", "\t");
    }
}
