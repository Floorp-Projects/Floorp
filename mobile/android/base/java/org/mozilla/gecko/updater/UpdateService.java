/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.updater;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.CrashHandler;
import org.mozilla.gecko.R;

import org.mozilla.apache.commons.codec.binary.Hex;

import org.mozilla.gecko.permissions.Permissions;
import org.mozilla.gecko.util.IOUtils;
import org.mozilla.gecko.util.ProxySelector;
import org.w3c.dom.Document;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import android.Manifest;
import android.app.AlarmManager;
import android.app.IntentService;
import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiManager.WifiLock;
import android.os.Environment;
import android.provider.Settings;
import android.support.v4.app.NotificationManagerCompat;
import android.support.v4.content.ContextCompat;
import android.support.v4.net.ConnectivityManagerCompat;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationCompat.Builder;
import android.util.Log;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URI;
import java.net.URL;
import java.net.URLConnection;
import java.security.MessageDigest;
import java.util.Calendar;
import java.util.GregorianCalendar;
import java.util.TimeZone;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;

public class UpdateService extends IntentService {
    private static final int BUFSIZE = 8192;
    private static final int NOTIFICATION_ID = 0x3e40ddbd;

    private static final String LOGTAG = "UpdateService";

    private static final int INTERVAL_LONG = 86400000; // in milliseconds
    private static final int INTERVAL_SHORT = 14400000; // again, in milliseconds
    private static final int INTERVAL_RETRY = 3600000;

    private static final String PREFS_NAME = "UpdateService";
    private static final String KEY_LAST_BUILDID = "UpdateService.lastBuildID";
    private static final String KEY_LAST_HASH_FUNCTION = "UpdateService.lastHashFunction";
    private static final String KEY_LAST_HASH_VALUE = "UpdateService.lastHashValue";
    private static final String KEY_LAST_FILE_NAME = "UpdateService.lastFileName";
    private static final String KEY_LAST_ATTEMPT_DATE = "UpdateService.lastAttemptDate";
    private static final String KEY_AUTODOWNLOAD_POLICY = "UpdateService.autoDownloadPolicy";
    private static final String KEY_UPDATE_URL = "UpdateService.updateUrl";

    private SharedPreferences mPrefs;

    private NotificationManagerCompat mNotificationManager;
    private ConnectivityManager mConnectivityManager;
    private Builder mBuilder;

    private volatile WifiLock mWifiLock;

    private boolean mDownloading;
    private boolean mCancelDownload;
    private boolean mApplyImmediately;

    private CrashHandler mCrashHandler;

    public enum AutoDownloadPolicy {
        NONE(-1),
        WIFI(0),
        DISABLED(1),
        ENABLED(2);

        public final int value;

        private AutoDownloadPolicy(int value) {
            this.value = value;
        }

        private final static AutoDownloadPolicy[] sValues = AutoDownloadPolicy.values();

        public static AutoDownloadPolicy get(int value) {
            for (AutoDownloadPolicy id: sValues) {
                if (id.value == value) {
                    return id;
                }
            }
            return NONE;
        }

        public static AutoDownloadPolicy get(String name) {
            for (AutoDownloadPolicy id: sValues) {
                if (name.equalsIgnoreCase(id.toString())) {
                    return id;
                }
            }
            return NONE;
        }
    }

    private enum CheckUpdateResult {
        // Keep these in sync with mobile/android/chrome/content/about.xhtml
        NOT_AVAILABLE,
        AVAILABLE,
        DOWNLOADING,
        DOWNLOADED
    }


    public UpdateService() {
        super("updater");
    }

    @Override
    public void onCreate () {
        mCrashHandler = CrashHandler.createDefaultCrashHandler(getApplicationContext());

        super.onCreate();

        mPrefs = getSharedPreferences(PREFS_NAME, 0);
        mNotificationManager = NotificationManagerCompat.from(this);
        mConnectivityManager = (ConnectivityManager) getApplicationContext().getSystemService(Context.CONNECTIVITY_SERVICE);
        mWifiLock = ((WifiManager) getApplicationContext().getSystemService(Context.WIFI_SERVICE))
                    .createWifiLock(WifiManager.WIFI_MODE_FULL_HIGH_PERF, PREFS_NAME);
        mCancelDownload = false;
    }

    @Override
    public void onDestroy() {
        mCrashHandler.unregister();
        mCrashHandler = null;

        if (mWifiLock.isHeld()) {
            mWifiLock.release();
        }
    }

    @Override
    public synchronized int onStartCommand (Intent intent, int flags, int startId) {
        // If we are busy doing a download, the new Intent here would normally be queued for
        // execution once that is done. In this case, however, we want to flip the boolean
        // while that is running, so handle that now.
        if (mDownloading && UpdateServiceHelper.ACTION_APPLY_UPDATE.equals(intent.getAction())) {
            Log.i(LOGTAG, "will apply update when download finished");

            mApplyImmediately = true;
            showDownloadNotification();
        } else if (UpdateServiceHelper.ACTION_CANCEL_DOWNLOAD.equals(intent.getAction())) {
            mCancelDownload = true;
        } else {
            super.onStartCommand(intent, flags, startId);
        }

        return Service.START_REDELIVER_INTENT;
    }

    @Override
    protected void onHandleIntent (final Intent intent) {
        if (UpdateServiceHelper.ACTION_REGISTER_FOR_UPDATES.equals(intent.getAction())) {
            AutoDownloadPolicy policy = AutoDownloadPolicy.get(
                intent.getIntExtra(UpdateServiceHelper.EXTRA_AUTODOWNLOAD_NAME,
                                   AutoDownloadPolicy.NONE.value));

            if (policy != AutoDownloadPolicy.NONE) {
                setAutoDownloadPolicy(policy);
            }

            String url = intent.getStringExtra(UpdateServiceHelper.EXTRA_UPDATE_URL_NAME);
            if (url != null) {
                setUpdateUrl(url);
            }

            registerForUpdates(false);
        } else if (UpdateServiceHelper.ACTION_CHECK_FOR_UPDATE.equals(intent.getAction())) {
            startUpdate(intent.getIntExtra(UpdateServiceHelper.EXTRA_UPDATE_FLAGS_NAME, 0));
            // Use this instead for forcing a download from about:fennec
            // startUpdate(UpdateServiceHelper.FLAG_FORCE_DOWNLOAD | UpdateServiceHelper.FLAG_REINSTALL);
        } else if (UpdateServiceHelper.ACTION_DOWNLOAD_UPDATE.equals(intent.getAction())) {
            // We always want to do the download and apply it here
            mApplyImmediately = true;
            startUpdate(UpdateServiceHelper.FLAG_FORCE_DOWNLOAD);
        } else if (UpdateServiceHelper.ACTION_APPLY_UPDATE.equals(intent.getAction())) {
            applyUpdate(intent.getStringExtra(UpdateServiceHelper.EXTRA_PACKAGE_PATH_NAME));
        }
    }

    private static boolean hasFlag(int flags, int flag) {
        return (flags & flag) == flag;
    }

    private void sendCheckUpdateResult(CheckUpdateResult result) {
        Intent resultIntent = new Intent(UpdateServiceHelper.ACTION_CHECK_UPDATE_RESULT);
        resultIntent.putExtra("result", result.toString());
        sendBroadcast(resultIntent);
    }

    private int getUpdateInterval(boolean isRetry) {
        int interval;
        if (isRetry) {
            interval = INTERVAL_RETRY;
        } else if (!AppConstants.RELEASE_OR_BETA) {
            interval = INTERVAL_SHORT;
        } else {
            interval = INTERVAL_LONG;
        }

        return interval;
    }

    private void registerForUpdates(boolean isRetry) {
        Calendar lastAttempt = getLastAttemptDate();
        Calendar now = new GregorianCalendar(TimeZone.getTimeZone("GMT"));

        int interval = getUpdateInterval(isRetry);

        if (lastAttempt == null || (now.getTimeInMillis() - lastAttempt.getTimeInMillis()) > interval) {
            // We've either never attempted an update, or we are passed the desired
            // time. Start an update now.
            Log.i(LOGTAG, "no update has ever been attempted, checking now");
            startUpdate(0);
            return;
        }

        AlarmManager manager = (AlarmManager) getSystemService(Context.ALARM_SERVICE);
        if (manager == null)
            return;

        PendingIntent pending = PendingIntent.getService(this, 0, new Intent(UpdateServiceHelper.ACTION_CHECK_FOR_UPDATE, null, this, UpdateService.class), PendingIntent.FLAG_UPDATE_CURRENT);
        manager.cancel(pending);

        lastAttempt.setTimeInMillis(lastAttempt.getTimeInMillis() + interval);
        Log.i(LOGTAG, "next update will be at: " + lastAttempt.getTime());

        manager.set(AlarmManager.RTC_WAKEUP, lastAttempt.getTimeInMillis(), pending);
    }

    private void startUpdate(final int flags) {
        setLastAttemptDate();

        NetworkInfo netInfo = mConnectivityManager.getActiveNetworkInfo();
        if (netInfo == null || !netInfo.isConnected()) {
            Log.i(LOGTAG, "not connected to the network");
            registerForUpdates(true);
            sendCheckUpdateResult(CheckUpdateResult.NOT_AVAILABLE);
            return;
        }

        registerForUpdates(false);

        final UpdateInfo info = findUpdate(hasFlag(flags, UpdateServiceHelper.FLAG_REINSTALL));
        boolean haveUpdate = (info != null);

        if (!haveUpdate) {
            Log.i(LOGTAG, "no update available");
            sendCheckUpdateResult(CheckUpdateResult.NOT_AVAILABLE);
            return;
        }

        Log.i(LOGTAG, "update available, buildID = " + info.buildID);

        Permissions.from(this)
                .withPermissions(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                .doNotPrompt()
                .andFallback(new Runnable() {
                    @Override
                    public void run() {
                        showPermissionNotification();
                        sendCheckUpdateResult(CheckUpdateResult.NOT_AVAILABLE);
                    }})
                .run(new Runnable() {
                    @Override
                    public void run() {
                        startDownload(info, flags);
                    }});
    }

    private void startDownload(UpdateInfo info, int flags) {
        AutoDownloadPolicy policy = getAutoDownloadPolicy();

        // We only start a download automatically if one of following criteria are met:
        //
        // - We have a FORCE_DOWNLOAD flag passed in
        // - The preference is set to 'always'
        // - The preference is set to 'wifi' and we are using a non-metered network (i.e. the user
        //   is OK with large data transfers occurring)
        //
        boolean shouldStartDownload = hasFlag(flags, UpdateServiceHelper.FLAG_FORCE_DOWNLOAD) ||
                policy == AutoDownloadPolicy.ENABLED ||
                (policy == AutoDownloadPolicy.WIFI && !ConnectivityManagerCompat.isActiveNetworkMetered(mConnectivityManager));

        if (!shouldStartDownload) {
            Log.i(LOGTAG, "not initiating automatic update download due to policy " + policy.toString());
            sendCheckUpdateResult(CheckUpdateResult.AVAILABLE);

            // We aren't autodownloading here, so prompt to start the update
            Intent notificationIntent = new Intent(UpdateServiceHelper.ACTION_DOWNLOAD_UPDATE);
            notificationIntent.setClass(this, UpdateService.class);
            PendingIntent contentIntent = PendingIntent.getService(this, 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);

            NotificationCompat.Builder builder = new NotificationCompat.Builder(this);
            builder.setSmallIcon(R.drawable.ic_status_logo);
            builder.setWhen(System.currentTimeMillis());
            builder.setAutoCancel(true);
            builder.setContentTitle(getString(R.string.updater_start_title));
            builder.setContentText(getString(R.string.updater_start_select));
            builder.setContentIntent(contentIntent);

            mNotificationManager.notify(NOTIFICATION_ID, builder.build());

            return;
        }

        File pkg = downloadUpdatePackage(info, hasFlag(flags, UpdateServiceHelper.FLAG_OVERWRITE_EXISTING));
        if (pkg == null) {
            sendCheckUpdateResult(CheckUpdateResult.NOT_AVAILABLE);
            return;
        }

        Log.i(LOGTAG, "have update package at " + pkg);

        saveUpdateInfo(info, pkg);
        sendCheckUpdateResult(CheckUpdateResult.DOWNLOADED);

        if (mApplyImmediately) {
            applyUpdate(pkg);
        } else {
            // Prompt to apply the update

            Intent notificationIntent = new Intent(UpdateServiceHelper.ACTION_APPLY_UPDATE);
            notificationIntent.setClass(this, UpdateService.class);
            notificationIntent.putExtra(UpdateServiceHelper.EXTRA_PACKAGE_PATH_NAME, pkg.getAbsolutePath());
            PendingIntent contentIntent = PendingIntent.getService(this, 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);


            NotificationCompat.Builder builder = new NotificationCompat.Builder(this);
            builder.setSmallIcon(R.drawable.ic_status_logo);
            builder.setWhen(System.currentTimeMillis());
            builder.setAutoCancel(true);
            builder.setContentTitle(getString(R.string.updater_apply_title));
            builder.setContentText(getString(R.string.updater_apply_select));
            builder.setContentIntent(contentIntent);

            mNotificationManager.notify(NOTIFICATION_ID, builder.build());
        }
    }

    private UpdateInfo findUpdate(boolean force) {
        URLConnection conn = null;
        try {
            URI uri = getUpdateURI(force);

            if (uri == null) {
              Log.e(LOGTAG, "failed to get update URI");
              return null;
            }

            DocumentBuilder builder = DocumentBuilderFactory.newInstance().newDocumentBuilder();
            conn = ProxySelector.openConnectionWithProxy(uri);
            Document dom = builder.parse(conn.getInputStream());

            NodeList nodes = dom.getElementsByTagName("update");
            if (nodes == null || nodes.getLength() == 0)
                return null;

            Node updateNode = nodes.item(0);
            Node buildIdNode = updateNode.getAttributes().getNamedItem("buildID");
            if (buildIdNode == null)
                return null;

            nodes = dom.getElementsByTagName("patch");
            if (nodes == null || nodes.getLength() == 0)
                return null;

            Node patchNode = nodes.item(0);
            Node urlNode = patchNode.getAttributes().getNamedItem("URL");
            Node hashFunctionNode = patchNode.getAttributes().getNamedItem("hashFunction");
            Node hashValueNode = patchNode.getAttributes().getNamedItem("hashValue");
            Node sizeNode = patchNode.getAttributes().getNamedItem("size");

            if (urlNode == null || hashFunctionNode == null ||
                hashValueNode == null || sizeNode == null) {
                return null;
            }

            // Fill in UpdateInfo from the XML data
            UpdateInfo info = new UpdateInfo();
            info.uri = new URI(urlNode.getTextContent());
            info.buildID = buildIdNode.getTextContent();
            info.hashFunction = hashFunctionNode.getTextContent();
            info.hashValue = hashValueNode.getTextContent();

            try {
                info.size = Integer.parseInt(sizeNode.getTextContent());
            } catch (NumberFormatException e) {
                Log.e(LOGTAG, "Failed to find APK size: ", e);
                return null;
            }

            // Make sure we have all the stuff we need to apply the update
            if (!info.isValid()) {
                Log.e(LOGTAG, "missing some required update information, have: " + info);
                return null;
            }

            return info;
        } catch (Exception e) {
            Log.e(LOGTAG, "failed to check for update: ", e);
            return null;
        } finally {
            // conn isn't guaranteed to be an HttpURLConnection, hence we don't want to cast earlier
            // in this method. However in our current implementation it usually is, so we need to
            // make sure we close it in that case:
            final HttpURLConnection httpConn = (HttpURLConnection) conn;
            if (httpConn != null) {
                httpConn.disconnect();
            }
        }
    }

    private MessageDigest createMessageDigest(String hashFunction) {
        String javaHashFunction = null;

        if ("sha512".equalsIgnoreCase(hashFunction)) {
            javaHashFunction = "SHA-512";
        } else {
            Log.e(LOGTAG, "Unhandled hash function: " + hashFunction);
            return null;
        }

        try {
            return MessageDigest.getInstance(javaHashFunction);
        } catch (java.security.NoSuchAlgorithmException e) {
            Log.e(LOGTAG, "Couldn't find algorithm " + javaHashFunction, e);
            return null;
        }
    }

    private void showDownloadNotification() {
        showDownloadNotification(null);
    }

    private void showDownloadNotification(File downloadFile) {

        Intent notificationIntent = new Intent(UpdateServiceHelper.ACTION_APPLY_UPDATE);
        notificationIntent.setClass(this, UpdateService.class);

        Intent cancelIntent = new Intent(UpdateServiceHelper.ACTION_CANCEL_DOWNLOAD);
        cancelIntent.setClass(this, UpdateService.class);

        if (downloadFile != null)
            notificationIntent.putExtra(UpdateServiceHelper.EXTRA_PACKAGE_PATH_NAME, downloadFile.getAbsolutePath());

        PendingIntent contentIntent = PendingIntent.getService(this, 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);
        PendingIntent deleteIntent = PendingIntent.getService(this, 0, cancelIntent, PendingIntent.FLAG_CANCEL_CURRENT);

        mBuilder = new NotificationCompat.Builder(this);
        mBuilder.setContentTitle(getResources().getString(R.string.updater_downloading_title))
                .setContentText(mApplyImmediately ? "" : getResources().getString(R.string.updater_downloading_select))
                .setSmallIcon(android.R.drawable.stat_sys_download)
                .setContentIntent(contentIntent)
                .setDeleteIntent(deleteIntent);

        mBuilder.setProgress(100, 0, true);
        mNotificationManager.notify(NOTIFICATION_ID, mBuilder.build());
    }

    private void showDownloadFailure() {
        Intent notificationIntent = new Intent(UpdateServiceHelper.ACTION_CHECK_FOR_UPDATE);
        notificationIntent.setClass(this, UpdateService.class);
        PendingIntent contentIntent = PendingIntent.getService(this, 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);

        NotificationCompat.Builder builder = new NotificationCompat.Builder(this);
        builder.setSmallIcon(R.drawable.ic_status_logo);
        builder.setWhen(System.currentTimeMillis());
        builder.setContentTitle(getString(R.string.updater_downloading_title_failed));
        builder.setContentText(getString(R.string.updater_downloading_retry));
        builder.setContentIntent(contentIntent);

        mNotificationManager.notify(NOTIFICATION_ID, builder.build());
    }

    private boolean deleteUpdatePackage(String path) {
        if (path == null) {
            return false;
        }

        File pkg = new File(path);
        if (!pkg.exists()) {
            return false;
        }

        pkg.delete();
        Log.i(LOGTAG, "deleted update package: " + path);

        return true;
    }

    private File downloadUpdatePackage(UpdateInfo info, boolean overwriteExisting) {
        URL url = null;
        try {
            url = info.uri.toURL();
        } catch (java.net.MalformedURLException e) {
            Log.e(LOGTAG, "failed to read URL: ", e);
            return null;
        }

        File path = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
        path.mkdirs();
        String fileName = new File(url.getFile()).getName();
        File downloadFile = new File(path, fileName);

        if (!overwriteExisting && info.buildID.equals(getLastBuildID()) && downloadFile.exists()) {
            // The last saved buildID is the same as the one for the current update. We also have a file
            // already downloaded, so it's probably the package we want. Verify it to be sure and just
            // return that if it matches.

            if (verifyDownloadedPackage(downloadFile)) {
                Log.i(LOGTAG, "using existing update package");
                return downloadFile;
            } else {
                // Didn't match, so we're going to download a new one.
                downloadFile.delete();
            }
        }

        if (!info.buildID.equals(getLastBuildID())) {
            // Delete the previous package when a new version becomes available.
            deleteUpdatePackage(getLastFileName());
        }

        Log.i(LOGTAG, "downloading update package");
        sendCheckUpdateResult(CheckUpdateResult.DOWNLOADING);

        OutputStream output = null;
        InputStream input = null;
        URLConnection conn = null;

        mDownloading = true;
        mCancelDownload = false;
        showDownloadNotification(downloadFile);

        try {
            NetworkInfo netInfo = mConnectivityManager.getActiveNetworkInfo();
            if (netInfo != null && netInfo.isConnected() &&
                netInfo.getType() == ConnectivityManager.TYPE_WIFI) {
                mWifiLock.acquire();
            }

            conn = ProxySelector.openConnectionWithProxy(info.uri);
            int length = conn.getContentLength();

            output = new BufferedOutputStream(new FileOutputStream(downloadFile));
            input = new BufferedInputStream(conn.getInputStream());

            byte[] buf = new byte[BUFSIZE];
            int len = 0;

            int bytesRead = 0;
            int lastNotify = 0;

            while ((len = input.read(buf, 0, BUFSIZE)) > 0 && !mCancelDownload) {
                output.write(buf, 0, len);
                bytesRead += len;
                // Updating the notification takes time so only do it every 1MB
                if (bytesRead - lastNotify > 1048576) {
                    mBuilder.setProgress(length, bytesRead, false);
                    mNotificationManager.notify(NOTIFICATION_ID, mBuilder.build());
                    lastNotify = bytesRead;
                }
            }

            mNotificationManager.cancel(NOTIFICATION_ID);

            // if the download was canceled by the user
            // delete the update package
            if (mCancelDownload) {
                Log.i(LOGTAG, "download canceled by user!");
                downloadFile.delete();

                return null;
            } else {
                Log.i(LOGTAG, "completed update download!");
                return downloadFile;
            }
        } catch (Exception e) {
            downloadFile.delete();
            showDownloadFailure();

            Log.e(LOGTAG, "failed to download update: ", e);
            return null;
        } finally {
            IOUtils.safeStreamClose(input);
            IOUtils.safeStreamClose(output);

            mDownloading = false;

            if (mWifiLock.isHeld()) {
                mWifiLock.release();
            }

            // conn isn't guaranteed to be an HttpURLConnection, hence we don't want to cast earlier
            // in this method. However in our current implementation it usually is, so we need to
            // make sure we close it in that case:
            final HttpURLConnection httpConn = (HttpURLConnection) conn;
            if (httpConn != null) {
                httpConn.disconnect();
            }
        }
    }

    private boolean verifyDownloadedPackage(File updateFile) {
        MessageDigest digest = createMessageDigest(getLastHashFunction());
        if (digest == null)
            return false;

        InputStream input = null;

        try {
            input = new BufferedInputStream(new FileInputStream(updateFile));

            byte[] buf = new byte[BUFSIZE];
            int len;
            while ((len = input.read(buf, 0, BUFSIZE)) > 0) {
                digest.update(buf, 0, len);
            }
        } catch (java.io.IOException e) {
            Log.e(LOGTAG, "Failed to verify update package: ", e);
            return false;
        } finally {
            try {
                if (input != null)
                    input.close();
            } catch (java.io.IOException e) { }
        }

        String hex = Hex.encodeHexString(digest.digest());
        if (!hex.equals(getLastHashValue())) {
            Log.e(LOGTAG, "Package hash does not match");
            return false;
        }

        return true;
    }

    private void applyUpdate(String updatePath) {
        if (updatePath == null) {
            updatePath = getLastFileName();
        }

        if (updatePath != null) {
            applyUpdate(new File(updatePath));
        }
    }

    private void applyUpdate(File updateFile) {
        mApplyImmediately = false;

        if (!updateFile.exists())
            return;

        Log.i(LOGTAG, "Verifying package: " + updateFile);

        if (!verifyDownloadedPackage(updateFile)) {
            Log.e(LOGTAG, "Not installing update, failed verification");
            return;
        }

        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setDataAndType(Uri.fromFile(updateFile), "application/vnd.android.package-archive");
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(intent);
    }

    private void showPermissionNotification() {
        Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                Uri.fromParts("package", getPackageName(), null));

        PendingIntent pendingIntent = PendingIntent.getActivity(this, 0, intent, 0);

        NotificationCompat.BigTextStyle bigTextStyle = new NotificationCompat.BigTextStyle()
                .bigText(getString(R.string.updater_permission_text));

        Notification notification = new NotificationCompat.Builder(this)
                .setContentTitle(getString(R.string.updater_permission_title))
                .setContentText(getString(R.string.updater_permission_text))
                .setStyle(bigTextStyle)
                .setAutoCancel(true)
                .setSmallIcon(R.drawable.ic_status_logo)
                .setColor(ContextCompat.getColor(this, R.color.rejection_red))
                .setContentIntent(pendingIntent)
                .build();

        NotificationManagerCompat.from(this)
                .notify(R.id.updateServicePermissionNotification, notification);
    }

    private String getLastBuildID() {
        return mPrefs.getString(KEY_LAST_BUILDID, null);
    }

    private String getLastHashFunction() {
        return mPrefs.getString(KEY_LAST_HASH_FUNCTION, null);
    }

    private String getLastHashValue() {
        return mPrefs.getString(KEY_LAST_HASH_VALUE, null);
    }

    private String getLastFileName() {
        return mPrefs.getString(KEY_LAST_FILE_NAME, null);
    }

    private Calendar getLastAttemptDate() {
        long lastAttempt = mPrefs.getLong(KEY_LAST_ATTEMPT_DATE, -1);
        if (lastAttempt < 0)
            return null;

        GregorianCalendar cal = new GregorianCalendar(TimeZone.getTimeZone("GMT"));
        cal.setTimeInMillis(lastAttempt);
        return cal;
    }

    private void setLastAttemptDate() {
        SharedPreferences.Editor editor = mPrefs.edit();
        editor.putLong(KEY_LAST_ATTEMPT_DATE, System.currentTimeMillis());
        editor.commit();
    }

    private AutoDownloadPolicy getAutoDownloadPolicy() {
        return AutoDownloadPolicy.get(mPrefs.getInt(KEY_AUTODOWNLOAD_POLICY, AutoDownloadPolicy.WIFI.value));
    }

    private void setAutoDownloadPolicy(AutoDownloadPolicy policy) {
        SharedPreferences.Editor editor = mPrefs.edit();
        editor.putInt(KEY_AUTODOWNLOAD_POLICY, policy.value);
        editor.commit();
    }

    private URI getUpdateURI(boolean force) {
        return UpdateServiceHelper.expandUpdateURI(this, mPrefs.getString(KEY_UPDATE_URL, null), force);
    }

    private void setUpdateUrl(String url) {
        SharedPreferences.Editor editor = mPrefs.edit();
        editor.putString(KEY_UPDATE_URL, url);
        editor.commit();
    }

    private void saveUpdateInfo(UpdateInfo info, File downloaded) {
        SharedPreferences.Editor editor = mPrefs.edit();
        editor.putString(KEY_LAST_BUILDID, info.buildID);
        editor.putString(KEY_LAST_HASH_FUNCTION, info.hashFunction);
        editor.putString(KEY_LAST_HASH_VALUE, info.hashValue);
        editor.putString(KEY_LAST_FILE_NAME, downloaded.toString());
        editor.commit();
    }

    private static final class UpdateInfo {
        public URI uri;
        public String buildID;
        public String hashFunction;
        public String hashValue;
        public int size;

        private boolean isNonEmpty(String s) {
            return s != null && s.length() > 0;
        }

        public boolean isValid() {
            return uri != null && isNonEmpty(buildID) &&
                isNonEmpty(hashFunction) && isNonEmpty(hashValue) && size > 0;
        }

        @Override
        public String toString() {
            return "uri = " + uri + ", buildID = " + buildID + ", hashFunction = " + hashFunction + ", hashValue = " + hashValue + ", size = " + size;
        }
    }
}
