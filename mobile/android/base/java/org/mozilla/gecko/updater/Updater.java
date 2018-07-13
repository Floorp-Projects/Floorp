/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.updater;

import android.Manifest;
import android.app.AlarmManager;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiManager.WifiLock;
import android.os.Environment;
import android.provider.Settings;
import android.support.annotation.NonNull;
import android.support.annotation.StringRes;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationManagerCompat;
import android.support.v4.content.ContextCompat;
import android.support.v4.net.ConnectivityManagerCompat;
import android.util.Log;

import org.mozilla.gecko.GeckoUpdateReceiver;
import org.mozilla.gecko.R;
import org.mozilla.gecko.permissions.Permissions;
import org.mozilla.gecko.updater.UpdateServiceHelper.AutoDownloadPolicy;
import org.mozilla.gecko.updater.UpdateServiceHelper.CheckUpdateResult;
import org.mozilla.gecko.updater.UpdateServiceHelper.UpdateInfo;
import org.mozilla.gecko.util.IOUtils;
import org.mozilla.gecko.util.ProxySelector;
import org.w3c.dom.Document;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URI;
import java.net.URL;
import java.net.URLConnection;
import java.util.Calendar;
import java.util.GregorianCalendar;
import java.util.TimeZone;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;

/**
 * Helper class that handles everything needed to download and apply an updated package.<br>
 * Only to be used on background threads because of the long time running operations involved.
 */
public class Updater {
    // Because this class can be called by multiple parties the log tag
    // will actually be the caller's name to easily identify whom requested the operations.
    private String logtag;
    private static final boolean DEBUG = false;

    // Flags for ACTION_CHECK_FOR_UPDATE
    static final int FLAG_FORCE_DOWNLOAD = 1;
    static final int FLAG_OVERWRITE_EXISTING = 1 << 1;
    static final int FLAG_REINSTALL = 1 << 2;

    private final Context context;
    private final ConnectivityManager connectivityManager;
    private final NotificationManagerCompat notificationManager;
    private final int bufferSize;
    private final int notificationId;
    private volatile WifiLock mWifiLock;

    private NotificationCompat.Builder notifBuilder;
    private BroadcastReceiver innerBroadcastReceiver;
    private UpdatesPrefs prefs;

    private boolean shouldCancelDownload;
    private boolean shouldApplyImmediately;

    Updater(final Context context, final UpdatesPrefs prefs, final int bufSize, final int notificationId) {
        this.logtag = getLogtagFromParentName(context);
        this.context = context;
        this.connectivityManager = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        this.notificationManager = NotificationManagerCompat.from(context);
        this.prefs = prefs;
        this.mWifiLock = ((WifiManager) context.getApplicationContext().getSystemService(Context.WIFI_SERVICE))
                .createWifiLock(WifiManager.WIFI_MODE_FULL_HIGH_PERF, prefs.getPreferenceName());
        this.bufferSize = bufSize;
        this.notificationId = notificationId;

        registerForLocalBroadcasts(context);

        if (DEBUG) {
            Log.d(logtag, "Updater created");
        }
    }

    /**
     * Must be called everytime the caller finished it's work to cleanup used resources.
     */
    void finish() {
        unregisterFromLocalBroadcasts();

        if (mWifiLock.isHeld()) {
            mWifiLock.release();
        }
    }

    void setIfShouldApplyUpdateImmediately(boolean shouldApplyImmediately) {
        this.shouldApplyImmediately = shouldApplyImmediately;
    }

    void startUpdate(final int flags) {
        prefs.setLastAttemptDate();

        NetworkInfo netInfo = connectivityManager.getActiveNetworkInfo();
        if (netInfo == null || !netInfo.isConnected()) {
            if (DEBUG) {
                Log.i(logtag, "not connected to the network");
            }
            registerForUpdates(true);
            sendCheckUpdateResult(CheckUpdateResult.NOT_AVAILABLE);
            return;
        }

        registerForUpdates(false);

        final UpdateInfo info = findUpdate(hasFlag(flags, FLAG_REINSTALL));
        boolean haveUpdate = (info != null);

        if (!haveUpdate) {
            if (DEBUG) {
                Log.i(logtag, "no update available");
            }
            sendCheckUpdateResult(CheckUpdateResult.NOT_AVAILABLE);
            return;
        }

        if (DEBUG) {
            Log.i(logtag, "update available, buildID = " + info.buildID);
        }

        Permissions.from(context)
                .withPermissions(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                .doNotPrompt()
                .andFallback(new Runnable() {
                    @Override
                    public void run() {
                        showPermissionNotification();
                        sendCheckUpdateResult(CheckUpdateResult.NOT_AVAILABLE);
                    }
                })
                .run(new Runnable() {
                    @Override
                    public void run() {
                        startDownload(info, flags);
                    }
                });

    }

    void registerForUpdates(boolean isRetry) {
        Calendar lastAttempt = prefs.getLastAttemptDate();
        Calendar now = new GregorianCalendar(TimeZone.getTimeZone("GMT"));

        int interval = UpdateServiceHelper.getUpdateInterval(isRetry);

        if (lastAttempt == null || (now.getTimeInMillis() - lastAttempt.getTimeInMillis()) > interval) {
            // We've either never attempted an update, or we are passed the desired
            // time. Start an update now.
            if (DEBUG) {
                Log.i(logtag, "no update has ever been attempted, checking now");
            }
            startUpdate(0);
            return;
        }

        AlarmManager manager = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
        if (manager == null)
            return;

        PendingIntent pending = PendingIntent.getBroadcast(context, 0,
                new Intent(UpdateServiceHelper.ACTION_CHECK_FOR_UPDATE, null, context, UpdateServiceReceiver.class),
                PendingIntent.FLAG_UPDATE_CURRENT);
        manager.cancel(pending);

        lastAttempt.setTimeInMillis(lastAttempt.getTimeInMillis() + interval);
        if (DEBUG) {
            Log.i(logtag, "next update will be at: " + lastAttempt.getTime());
        }

        manager.set(AlarmManager.RTC_WAKEUP, lastAttempt.getTimeInMillis(), pending);
    }

     void applyUpdate(String updatePath) {
        if (updatePath == null) {
            updatePath = prefs.getLastFileName();
        }

        if (updatePath != null) {
            applyUpdate(new File(updatePath));
        }
    }

    private void showPermissionNotification() {
        Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                Uri.fromParts("package", context.getPackageName(), null));

        PendingIntent pendingIntent = PendingIntent.getActivity(context, 0, intent, 0);

        NotificationCompat.BigTextStyle bigTextStyle = new NotificationCompat.BigTextStyle()
                .bigText(getString(R.string.updater_permission_text));

        Notification notification = new NotificationCompat.Builder(context)
                .setContentTitle(getString(R.string.updater_permission_title))
                .setContentText(getString(R.string.updater_permission_text))
                .setStyle(bigTextStyle)
                .setAutoCancel(true)
                .setSmallIcon(R.drawable.ic_status_logo)
                .setColor(ContextCompat.getColor(context, R.color.rejection_red))
                .setContentIntent(pendingIntent)
                .build();

        NotificationManagerCompat.from(context)
                .notify(R.id.updateServicePermissionNotification, notification);
    }

    private void startDownload(UpdateInfo info, int flags) {
        AutoDownloadPolicy policy = prefs.getAutoDownloadPolicy();

        // We only start a download automatically if one of following criteria are met:
        //
        // - We have a FORCE_DOWNLOAD flag passed in
        // - The preference is set to 'always'
        // - The preference is set to 'wifi' and we are using a non-metered network (i.e. the user
        //   is OK with large data transfers occurring)
        //
        boolean shouldStartDownload = hasFlag(flags, FLAG_FORCE_DOWNLOAD) ||
                policy == AutoDownloadPolicy.ENABLED ||
                (policy == AutoDownloadPolicy.WIFI && !ConnectivityManagerCompat.isActiveNetworkMetered(connectivityManager));

        if (!shouldStartDownload) {
            if (DEBUG) {
                Log.i(logtag, "not initiating automatic update download due to policy " + policy.toString());
            }
            sendCheckUpdateResult(CheckUpdateResult.AVAILABLE);

            // We aren't autodownloading here, so prompt to start the update with a new JobIntentService
            Intent notificationIntent = new Intent(UpdateServiceHelper.ACTION_DOWNLOAD_UPDATE);
            notificationIntent.setClass(context, UpdateServiceReceiver.class);
            PendingIntent contentIntent = PendingIntent.getBroadcast(context, 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);

            NotificationCompat.Builder builder = new NotificationCompat.Builder(context);
            builder.setSmallIcon(R.drawable.ic_status_logo);
            builder.setWhen(System.currentTimeMillis());
            builder.setAutoCancel(true);
            builder.setContentTitle(getString(R.string.updater_start_title));
            builder.setContentText(getString(R.string.updater_start_select));
            builder.setContentIntent(contentIntent);

            notificationManager.notify(notificationId, builder.build());

            return;
        }

        File pkg = downloadUpdatePackage(info, hasFlag(flags, FLAG_OVERWRITE_EXISTING));
        if (pkg == null) {
            sendCheckUpdateResult(CheckUpdateResult.NOT_AVAILABLE);
            return;
        }

        if (DEBUG) {
            Log.i(logtag, "have update package at " + pkg);
        }

        prefs.saveUpdateInfo(info, pkg);
        sendCheckUpdateResult(CheckUpdateResult.DOWNLOADED);

        if (shouldApplyImmediately) {
            applyUpdate(pkg);
        } else {
            // Prompt to apply the update with a new JobIntentService

            Intent notificationIntent = new Intent(UpdateServiceHelper.ACTION_APPLY_UPDATE);
            notificationIntent.setClass(context, UpdateServiceReceiver.class);
            notificationIntent.putExtra(UpdateServiceHelper.EXTRA_PACKAGE_PATH_NAME, pkg.getAbsolutePath());
            PendingIntent contentIntent = PendingIntent.getBroadcast(context, 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);


            NotificationCompat.Builder builder = new NotificationCompat.Builder(context);
            builder.setSmallIcon(R.drawable.ic_status_logo);
            builder.setWhen(System.currentTimeMillis());
            builder.setAutoCancel(true);
            builder.setContentTitle(getString(R.string.updater_apply_title));
            builder.setContentText(getString(R.string.updater_apply_select));
            builder.setContentIntent(contentIntent);

            notificationManager.notify(notificationId, builder.build());
        }
    }

    private File downloadUpdatePackage(UpdateInfo info, boolean overwriteExisting) {
        URL url = null;
        try {
            url = info.uri.toURL();
        } catch (java.net.MalformedURLException e) {
            Log.e(logtag, "failed to read URL: ", e);
            return null;
        }

        File path = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
        path.mkdirs();
        String fileName = new File(url.getFile()).getName();
        File downloadFile = new File(path, fileName);


        final String lastBuildId = prefs.getLastBuildID();
        if (!overwriteExisting && info.buildID.equals(lastBuildId) && downloadFile.exists()) {
            // The last saved buildID is the same as the one for the current update. We also have a file
            // already downloaded, so it's probably the package we want. Verify it to be sure and just
            // return that if it matches.

            if (PackageVerifier.verifyDownloadedPackage(prefs, downloadFile, bufferSize)) {
                if (DEBUG) {
                    Log.i(logtag, "using existing update package");
                }
                return downloadFile;
            } else {
                // Didn't match, so we're going to download a new one.
                downloadFile.delete();
            }
        }

        if (!info.buildID.equals(prefs.getLastBuildID())) {
            // Delete the previous package when a new version becomes available.
            deleteUpdatePackage(prefs.getLastFileName());
        }

        if (DEBUG) {
            Log.i(logtag, "downloading update package");
        }
        sendCheckUpdateResult(CheckUpdateResult.DOWNLOADING);

        OutputStream output = null;
        InputStream input = null;
        URLConnection conn = null;

        shouldCancelDownload = false;
        showDownloadNotification(downloadFile);

        try {
            NetworkInfo netInfo = connectivityManager.getActiveNetworkInfo();
            if (netInfo != null && netInfo.isConnected() &&
                    netInfo.getType() == ConnectivityManager.TYPE_WIFI) {
                mWifiLock.acquire();
            }

            conn = ProxySelector.openConnectionWithProxy(info.uri);
            int length = conn.getContentLength();

            output = new BufferedOutputStream(new FileOutputStream(downloadFile));
            input = new BufferedInputStream(conn.getInputStream());

            byte[] buf = new byte[bufferSize];
            int len = 0;

            int bytesRead = 0;
            int lastNotify = 0;

            while ((len = input.read(buf, 0, bufferSize)) > 0 && !shouldCancelDownload) {
                output.write(buf, 0, len);
                bytesRead += len;
                // Updating the notification takes time so only do it every 1MB
                if (bytesRead - lastNotify > 1048576) {
                    notifBuilder.setProgress(length, bytesRead, false);
                    notificationManager.notify(notificationId, notifBuilder.build());
                    lastNotify = bytesRead;
                }
            }

            notificationManager.cancel(notificationId);

            // if the download was canceled by the user
            // delete the update package
            if (shouldCancelDownload) {
                if (DEBUG) {
                    Log.i(logtag, "download canceled by user!");
                }
                downloadFile.delete();

                return null;
            } else {
                if (DEBUG) {
                    Log.i(logtag, "completed update download!");
                }
                return downloadFile;
            }
        } catch (Exception e) {
            downloadFile.delete();
            showDownloadFailure();

            Log.e(logtag, "failed to download update: ", e);
            return null;
        } finally {
            IOUtils.safeStreamClose(input);
            IOUtils.safeStreamClose(output);

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

    private void showDownloadNotification() {
        showDownloadNotification(null);
    }

    private void showDownloadNotification(File downloadFile) {

        Intent notificationIntent = new Intent(UpdateServiceHelper.ACTION_APPLY_UPDATE);
        notificationIntent.setClass(context, UserUpdatesReceiver.class);

        Intent cancelIntent = new Intent(UpdateServiceHelper.ACTION_CANCEL_DOWNLOAD);
        cancelIntent.setClass(context, UserUpdatesReceiver.class);

        if (downloadFile != null)
            notificationIntent.putExtra(UpdateServiceHelper.EXTRA_PACKAGE_PATH_NAME, downloadFile.getAbsolutePath());

        PendingIntent contentIntent = PendingIntent.getBroadcast(context, 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);
        PendingIntent deleteIntent = PendingIntent.getBroadcast(context, 0, cancelIntent, PendingIntent.FLAG_CANCEL_CURRENT);

        notifBuilder = new NotificationCompat.Builder(context);
        notifBuilder.setContentTitle(getString(R.string.updater_downloading_title))
                .setContentText(shouldApplyImmediately ? "" : getString(R.string.updater_downloading_select))
                .setSmallIcon(android.R.drawable.stat_sys_download)
                .setContentIntent(contentIntent)
                .setDeleteIntent(deleteIntent);

        notifBuilder.setProgress(100, 0, true);
        notificationManager.notify(notificationId, notifBuilder.build());
    }

    private void showDownloadFailure() {
        // Let the user restart the update process with a new JobIntentService
        Intent notificationIntent = new Intent(UpdateServiceHelper.ACTION_CHECK_FOR_UPDATE);
        notificationIntent.setClass(context, UpdateServiceReceiver.class);
        PendingIntent contentIntent = PendingIntent.getBroadcast(context, 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);

        NotificationCompat.Builder builder = new NotificationCompat.Builder(context);
        builder.setSmallIcon(R.drawable.ic_status_logo);
        builder.setWhen(System.currentTimeMillis());
        builder.setContentTitle(getString(R.string.updater_downloading_title_failed));
        builder.setContentText(getString(R.string.updater_downloading_retry));
        builder.setContentIntent(contentIntent);

        notificationManager.notify(notificationId, builder.build());
    }

    private void applyUpdate(File updateFile) {
        shouldApplyImmediately = false;

        if (!updateFile.exists())
            return;

        if (DEBUG) {
            Log.i(logtag, "Verifying package: " + updateFile);
        }

        if (!PackageVerifier.verifyDownloadedPackage(prefs, updateFile, bufferSize)) {
            Log.e(logtag, "Not installing update, failed verification");
            return;
        }

        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setDataAndType(Uri.fromFile(updateFile), "application/vnd.android.package-archive");
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
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

        if (DEBUG) {
            Log.i(logtag, "deleted update package: " + path);
        }

        return true;
    }

    private void sendCheckUpdateResult(CheckUpdateResult result) {
        Intent resultIntent = new Intent(context, GeckoUpdateReceiver.class);
        resultIntent.setAction(UpdateServiceHelper.ACTION_CHECK_UPDATE_RESULT);
        resultIntent.putExtra("result", result.toString());
        context.sendBroadcast(resultIntent);
    }

    private UpdateInfo findUpdate(boolean force) {
        URLConnection conn = null;
        try {
            URI uri = prefs.getUpdateURI(force);

            if (uri == null) {
                Log.e(logtag, "failed to get update URI");
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
                Log.e(logtag, "Failed to find APK size: ", e);
                return null;
            }

            // Make sure we have all the stuff we need to apply the update
            if (!info.isValid()) {
                Log.e(logtag, "missing some required update information, have: " + info);
                return null;
            }

            return info;
        } catch (Exception e) {
            Log.e(logtag, "failed to check for update: ", e);
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

    private String getString(@StringRes final int stringResId) {
        return context.getString(stringResId);
    }

    private static boolean hasFlag(int flags, int flag) {
        return (flags & flag) == flag;
    }

    // The log tag can have at most 23 chars or Log will throw an IllegalArgumentException
    private String getLogtagFromParentName(@NonNull final Context context) {
        final String parentName = "Gecko" + context.getClass().getSimpleName();
        if (parentName.length() > 23) {
            return parentName.substring(0, 24);
        } else {
            return parentName;
        }
    }

    /**
     * Receiver only used in local notifications that allow for actions to modify current running state.
     * As such it is an inner class of current JobIntentService.<br>
     * For any notifications that could allow for actions after the service completed it's work
     * use {@link UpdateServiceReceiver}
     */
    private class UserUpdatesReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            final String action = intent.getAction();

            switch (action) {
                case UpdateServiceHelper.ACTION_APPLY_UPDATE:
                    // User can only press a notification button for this while a download is in progress.
                    // If download finished the notification will be update to allow starting
                    // a new JobIntentService to apply the update
                    if (DEBUG) {
                        Log.i(logtag, "will apply update when download finished");
                    }
                    shouldApplyImmediately = true;
                    showDownloadNotification();
                    break;
                case UpdateServiceHelper.ACTION_CANCEL_DOWNLOAD:
                    shouldCancelDownload = true;
                    break;
                default:
                    // no-op, we only listen for the above
            }
        }
    }

    private void registerForLocalBroadcasts(@NonNull final Context context) {
        innerBroadcastReceiver = new UserUpdatesReceiver();
        final IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(UpdateServiceHelper.ACTION_APPLY_UPDATE);
        intentFilter.addAction(UpdateServiceHelper.ACTION_CANCEL_DOWNLOAD);

        context.registerReceiver(innerBroadcastReceiver, intentFilter);
    }

    private void unregisterFromLocalBroadcasts() {
        context.unregisterReceiver(innerBroadcastReceiver);
    }
}
