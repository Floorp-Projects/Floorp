/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.updater;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.R;

import org.mozilla.apache.commons.codec.binary.Hex;

import org.w3c.dom.Document;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import android.app.AlarmManager;
import android.app.IntentService;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.os.Environment;
import android.util.Log;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Proxy;
import java.net.ProxySelector;
import java.net.URL;
import java.net.URLConnection;
import java.security.MessageDigest;
import java.util.Calendar;
import java.util.GregorianCalendar;
import java.util.List;
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

    private SharedPreferences mPrefs;

    private NotificationManager mNotificationManager;
    private ConnectivityManager mConnectivityManager;

    private boolean mDownloading;
    private boolean mApplyImmediately;

    public UpdateService() {
        super("updater");
    }

    @Override
    public void onCreate () {
        super.onCreate();
        
        mPrefs = getSharedPreferences(PREFS_NAME, 0);
        mNotificationManager = (NotificationManager)getSystemService(Context.NOTIFICATION_SERVICE);
        mConnectivityManager = (ConnectivityManager)getSystemService(Context.CONNECTIVITY_SERVICE);
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
        } else {
            super.onStartCommand(intent, flags, startId);
        }

        return Service.START_REDELIVER_INTENT;
    }

    @Override
    protected void onHandleIntent (Intent intent) {
        if (UpdateServiceHelper.ACTION_REGISTER_FOR_UPDATES.equals(intent.getAction())) {
            int policy = intent.getIntExtra(UpdateServiceHelper.EXTRA_AUTODOWNLOAD_NAME, -1);
            if (policy >= 0) {
                setAutoDownloadPolicy(policy);
            }

            registerForUpdates(false);
        } else if (UpdateServiceHelper.ACTION_CHECK_FOR_UPDATE.equals(intent.getAction())) {
            Log.i(LOGTAG, "XYZZY Coming from Point 1");
            startUpdate(intent.getIntExtra(UpdateServiceHelper.EXTRA_UPDATE_FLAGS_NAME, 0));
        } else if (UpdateServiceHelper.ACTION_DOWNLOAD_UPDATE.equals(intent.getAction())) {
            // We always want to do the download here
            Log.i(LOGTAG, "XYZZY Coming from Point 2");
            startUpdate(UpdateServiceHelper.FLAG_FORCE_DOWNLOAD);
        } else if (UpdateServiceHelper.ACTION_APPLY_UPDATE.equals(intent.getAction())) {
            applyUpdate(intent.getStringExtra(UpdateServiceHelper.EXTRA_PACKAGE_PATH_NAME));
        }
    }

    private static boolean hasFlag(int flags, int flag) {
        return (flags & flag) == flag;
    }

    private void sendCheckUpdateResult(UpdateServiceHelper.CheckUpdateResult result) {
        Intent resultIntent = new Intent(UpdateServiceHelper.ACTION_CHECK_UPDATE_RESULT);
        resultIntent.putExtra("result", result.toString());
        sendBroadcast(resultIntent);
    }

    private int getUpdateInterval(boolean isRetry) {
        int interval;
        if (isRetry) {
            interval = INTERVAL_RETRY;
        } else if (AppConstants.MOZ_UPDATE_CHANNEL.equals("nightly") ||
                   AppConstants.MOZ_UPDATE_CHANNEL.equals("aurora")) {
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
            Log.i(LOGTAG, "XYZZY Coming from Point 3");
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

    private void startUpdate(int flags) {
        setLastAttemptDate();

        NetworkInfo netInfo = mConnectivityManager.getActiveNetworkInfo();
        if (netInfo == null || !netInfo.isConnected()) {
            Log.i(LOGTAG, "not connected to the network");
            registerForUpdates(true);
            sendCheckUpdateResult(UpdateServiceHelper.CheckUpdateResult.NOT_AVAILABLE);
            return;
        }

        registerForUpdates(false);

        UpdateInfo info = findUpdate(hasFlag(flags, UpdateServiceHelper.FLAG_REINSTALL));
        boolean haveUpdate = (info != null);

        if (!haveUpdate) {
            Log.i(LOGTAG, "no update available");
            sendCheckUpdateResult(UpdateServiceHelper.CheckUpdateResult.NOT_AVAILABLE);
            return;
        }

        Log.i(LOGTAG, "update available, buildID = " + info.buildID);
        
        int connectionType = netInfo.getType();
        int autoDownloadPolicy = getAutoDownloadPolicy();
        Log.i(LOGTAG, "XYZZY Checking AutoDownloadPolicy " + Integer.toString(autoDownloadPolicy));


        /**
         * We only start a download automatically if one of following criteria are met:
         *
         * - We have a FORCE_DOWNLOAD flag passed in
         * - The preference is set to 'always'
         * - The preference is set to 'wifi' and we are actually using wifi (or regular ethernet)
         */

        Log.i(LOGTAG, "XYZZY " + Integer.toString(flags));

        boolean shouldStartDownload = hasFlag(flags, UpdateServiceHelper.FLAG_FORCE_DOWNLOAD) ||
            autoDownloadPolicy == UpdateServiceHelper.AUTODOWNLOAD_ENABLED ||
            (autoDownloadPolicy == UpdateServiceHelper.AUTODOWNLOAD_WIFI &&
             (connectionType == ConnectivityManager.TYPE_WIFI || connectionType == ConnectivityManager.TYPE_ETHERNET));

        if (!shouldStartDownload) {
            Log.i(LOGTAG, "not initiating automatic update download due to policy " + autoDownloadPolicy);
            sendCheckUpdateResult(UpdateServiceHelper.CheckUpdateResult.AVAILABLE);

            // We aren't autodownloading here, so prompt to start the update
            Notification notification = new Notification(R.drawable.ic_status_logo, null, System.currentTimeMillis());

            Intent notificationIntent = new Intent(UpdateServiceHelper.ACTION_DOWNLOAD_UPDATE);
            notificationIntent.setClass(this, UpdateService.class);

            PendingIntent contentIntent = PendingIntent.getService(this, 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);
            notification.flags = Notification.FLAG_AUTO_CANCEL;

            notification.setLatestEventInfo(this, getResources().getString(R.string.updater_start_title),
                                            getResources().getString(R.string.updater_start_select),
                                            contentIntent);

            mNotificationManager.notify(NOTIFICATION_ID, notification);

            return;
        }

        File pkg = downloadUpdatePackage(info, hasFlag(flags, UpdateServiceHelper.FLAG_OVERWRITE_EXISTING));
        if (pkg == null) {
            sendCheckUpdateResult(UpdateServiceHelper.CheckUpdateResult.NOT_AVAILABLE);
            return;
        }

        Log.i(LOGTAG, "have update package at " + pkg);

        saveUpdateInfo(info, pkg);
        sendCheckUpdateResult(UpdateServiceHelper.CheckUpdateResult.DOWNLOADED);

        if (mApplyImmediately) {
            applyUpdate(pkg);
        } else {
            // Prompt to apply the update
            Notification notification = new Notification(R.drawable.ic_status_logo, null, System.currentTimeMillis());

            Intent notificationIntent = new Intent(UpdateServiceHelper.ACTION_APPLY_UPDATE);
            notificationIntent.setClass(this, UpdateService.class);
            notificationIntent.putExtra(UpdateServiceHelper.EXTRA_PACKAGE_PATH_NAME, pkg.getAbsolutePath());

            PendingIntent contentIntent = PendingIntent.getService(this, 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);
            notification.flags = Notification.FLAG_AUTO_CANCEL;

            notification.setLatestEventInfo(this, getResources().getString(R.string.updater_apply_title),
                                            getResources().getString(R.string.updater_apply_select),
                                            contentIntent);

            mNotificationManager.notify(NOTIFICATION_ID, notification);
        }
    }

    private URLConnection openConnectionWithProxy(URL url) throws java.net.URISyntaxException, java.io.IOException {
        Log.i(LOGTAG, "openning connection with url: " + url);

        ProxySelector ps = ProxySelector.getDefault();
        Proxy proxy = Proxy.NO_PROXY;
        if (ps != null) {
            List<Proxy> proxies = ps.select(url.toURI());
            if (proxies != null && !proxies.isEmpty()) {
                proxy = proxies.get(0);
            }
        }

        return url.openConnection(proxy);
    }

    private UpdateInfo findUpdate(boolean force) {
        try {
            URL url = UpdateServiceHelper.getUpdateUrl(this, force);

            DocumentBuilder builder = DocumentBuilderFactory.newInstance().newDocumentBuilder();
            Document dom = builder.parse(openConnectionWithProxy(url).getInputStream());

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
            info.url = new URL(urlNode.getTextContent());
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
        Notification notification = new Notification(android.R.drawable.stat_sys_download, null, System.currentTimeMillis());

        Intent notificationIntent = new Intent(UpdateServiceHelper.ACTION_APPLY_UPDATE);
        notificationIntent.setClass(this, UpdateService.class);

        if (downloadFile != null)
            notificationIntent.putExtra(UpdateServiceHelper.EXTRA_PACKAGE_PATH_NAME, downloadFile.getAbsolutePath());

        PendingIntent contentIntent = PendingIntent.getService(this, 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);

        notification.setLatestEventInfo(this, getResources().getString(R.string.updater_downloading_title),
                                        mApplyImmediately ? "" : getResources().getString(R.string.updater_downloading_select),
                                        contentIntent);
        
        mNotificationManager.notify(NOTIFICATION_ID, notification);
    }

    private void showDownloadFailure() {
        Notification notification = new Notification(R.drawable.ic_status_logo, null, System.currentTimeMillis());

        Intent notificationIntent = new Intent(UpdateServiceHelper.ACTION_CHECK_FOR_UPDATE);
        notificationIntent.setClass(this, UpdateService.class);

        PendingIntent contentIntent = PendingIntent.getService(this, 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);

        notification.setLatestEventInfo(this, getResources().getString(R.string.updater_downloading_title_failed),
                                        getResources().getString(R.string.updater_downloading_retry),
                                        contentIntent);
        
        mNotificationManager.notify(NOTIFICATION_ID, notification);
    }

    private File downloadUpdatePackage(UpdateInfo info, boolean overwriteExisting) {
        File downloadFile = new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS), new File(info.url.getFile()).getName());

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

        Log.i(LOGTAG, "downloading update package");
        sendCheckUpdateResult(UpdateServiceHelper.CheckUpdateResult.DOWNLOADING);

        OutputStream output = null;
        InputStream input = null;

        mDownloading = true;
        showDownloadNotification(downloadFile);

        try {
            URLConnection conn = openConnectionWithProxy(info.url);
            int length = conn.getContentLength();

            output = new BufferedOutputStream(new FileOutputStream(downloadFile));
            input = new BufferedInputStream(conn.getInputStream());

            byte[] buf = new byte[BUFSIZE];
            int len = 0;

            int bytesRead = 0;
            float lastPercent = 0.0f;

            while ((len = input.read(buf, 0, BUFSIZE)) > 0) {
                output.write(buf, 0, len);
                bytesRead += len;
            }

            Log.i(LOGTAG, "completed update download!");

            mNotificationManager.cancel(NOTIFICATION_ID);

            return downloadFile;
        } catch (Exception e) {
            downloadFile.delete();
            showDownloadFailure();

            Log.e(LOGTAG, "failed to download update: ", e);
            return null;
        } finally {
            try {
                if (input != null)
                    input.close();
            } catch (java.io.IOException e) {}

            try {
                if (output != null)
                    output.close();
            } catch (java.io.IOException e) {}

            mDownloading = false;
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
            } catch(java.io.IOException e) {}
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
            updatePath = mPrefs.getString(KEY_LAST_FILE_NAME, null);
        }
        applyUpdate(new File(updatePath));
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

    private String getLastBuildID() {
        return mPrefs.getString(KEY_LAST_BUILDID, null);
    }

    private String getLastHashFunction() {
        return mPrefs.getString(KEY_LAST_HASH_FUNCTION, null);
    }

    private String getLastHashValue() {
        return mPrefs.getString(KEY_LAST_HASH_VALUE, null);
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

    private int getAutoDownloadPolicy() {
        return mPrefs.getInt(KEY_AUTODOWNLOAD_POLICY, UpdateServiceHelper.AUTODOWNLOAD_WIFI);
    }

    private void setAutoDownloadPolicy(int policy) {
        Log.i(LOGTAG, "XYZZY Setting AutoDownloadPolicy " + Integer.toString(policy));
        SharedPreferences.Editor editor = mPrefs.edit();
        editor.putInt(KEY_AUTODOWNLOAD_POLICY, policy);
        editor.commit();
        Log.i(LOGTAG, "XYZZY Verifying AutoDownloadPolicy " + Integer.toString(getAutoDownloadPolicy()));
    }

    private void saveUpdateInfo(UpdateInfo info, File downloaded) {
        SharedPreferences.Editor editor = mPrefs.edit();
        editor.putString(KEY_LAST_BUILDID, info.buildID);
        editor.putString(KEY_LAST_HASH_FUNCTION, info.hashFunction);
        editor.putString(KEY_LAST_HASH_VALUE, info.hashValue);
        editor.putString(KEY_LAST_FILE_NAME, downloaded.toString());
        editor.commit();
    }

    private class UpdateInfo {
        public URL url;
        public String buildID;
        public String hashFunction;
        public String hashValue;
        public int size;

        private boolean isNonEmpty(String s) {
            return s != null && s.length() > 0;
        }

        public boolean isValid() {
            return url != null && isNonEmpty(buildID) &&
                isNonEmpty(hashFunction) && isNonEmpty(hashValue) && size > 0;
        }

        @Override
        public String toString() {
            return "url = " + url + ", buildID = " + buildID + ", hashFunction = " + hashFunction + ", hashValue = " + hashValue + ", size = " + size;
        }
    }
}
