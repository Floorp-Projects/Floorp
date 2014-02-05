/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.webapp;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.favicons.decoders.FaviconDecoder;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.util.ActivityResultHandler;
import org.mozilla.gecko.util.EventDispatcher;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.WebAppAllocator;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Bitmap;
import android.net.Uri;
import android.util.Log;

import java.io.File;
import java.util.List;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

public class EventListener implements GeckoEventListener {

    private static final String LOGTAG = "GeckoWebAppEventListener";

    private EventListener() { }

    private static EventListener mEventListener;

    private static EventListener getEventListener() {
        if (mEventListener == null) {
            mEventListener = new EventListener();
        }
        return mEventListener;
    }

    private static void registerEventListener(String event) {
        GeckoAppShell.getEventDispatcher().registerEventListener(event, EventListener.getEventListener());
    }

    private static void unregisterEventListener(String event) {
        GeckoAppShell.getEventDispatcher().unregisterEventListener(event, EventListener.getEventListener());
    }

    public static void registerEvents() {
        registerEventListener("WebApps:PreInstall");
        registerEventListener("WebApps:InstallApk");
        registerEventListener("WebApps:PostInstall");
        registerEventListener("WebApps:Open");
        registerEventListener("WebApps:Uninstall");
    }

    public static void unregisterEvents() {
        unregisterEventListener("WebApps:PreInstall");
        unregisterEventListener("WebApps:InstallApk");
        unregisterEventListener("WebApps:PostInstall");
        unregisterEventListener("WebApps:Open");
        unregisterEventListener("WebApps:Uninstall");
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        try {
            if (AppConstants.MOZ_ANDROID_SYNTHAPKS && event.equals("WebApps:InstallApk")) {
                installApk(GeckoAppShell.getGeckoInterface().getActivity(), message.getString("filePath"), message.getString("data"));
            } else if (event.equals("WebApps:PostInstall")) {
                if (AppConstants.MOZ_ANDROID_SYNTHAPKS) {
                    postInstallWebApp(message.getString("apkPackageName"), message.getString("origin"));
                } else {
                    postInstallWebApp(message.getString("name"),
                                      message.getString("manifestURL"),
                                      message.getString("origin"),
                                      message.getString("iconURL"),
                                      message.getString("originalOrigin"));
                }
            } else if (event.equals("WebApps:Open")) {
                Intent intent = GeckoAppShell.getWebAppIntent(message.getString("manifestURL"),
                                                              message.getString("origin"),
                                                              "", null);
                if (intent == null) {
                    return;
                }
                GeckoAppShell.getGeckoInterface().getActivity().startActivity(intent);
            } else if (!AppConstants.MOZ_ANDROID_SYNTHAPKS && event.equals("WebApps:Uninstall")) {
                uninstallWebApp(message.getString("origin"));
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    // Not used by MOZ_ANDROID_SYNTHAPKS.
    public static File preInstallWebApp(String aTitle, String aURI, String aOrigin) {
        int index = WebAppAllocator.getInstance(GeckoAppShell.getContext()).findAndAllocateIndex(aOrigin, aTitle, (String) null);
        GeckoProfile profile = GeckoProfile.get(GeckoAppShell.getContext(), "webapp" + index);
        return profile.getDir();
    }

    // Not used by MOZ_ANDROID_SYNTHAPKS.
    public static void postInstallWebApp(String aTitle, String aURI, String aOrigin, String aIconURL, String aOriginalOrigin) {
        WebAppAllocator allocator = WebAppAllocator.getInstance(GeckoAppShell.getContext());
        int index = allocator.getIndexForApp(aOriginalOrigin);

        assert aIconURL != null;

        final int preferredSize = GeckoAppShell.getPreferredIconSize();
        Bitmap icon = FaviconDecoder.getMostSuitableBitmapFromDataURI(aIconURL, preferredSize);

        assert aOrigin != null && index != -1;
        allocator.updateAppAllocation(aOrigin, index, icon);

        GeckoAppShell.createShortcut(aTitle, aURI, aOrigin, icon, "webapp");
    }

    // Used by MOZ_ANDROID_SYNTHAPKS.
    public static void postInstallWebApp(String aPackageName, String aOrigin) {
        Allocator allocator = Allocator.getInstance(GeckoAppShell.getContext());
        int index = allocator.findOrAllocatePackage(aPackageName);
        allocator.putOrigin(index, aOrigin);
    }

    public static void uninstallWebApp(final String uniqueURI) {
        // On uninstall, we need to do a couple of things:
        //   1. nuke the running app process.
        //   2. nuke the profile that was assigned to that webapp
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                int index;
                index = Allocator.getInstance(GeckoAppShell.getContext()).releaseIndexForApp(uniqueURI);

                // if -1, nothing to do; we didn't think it was installed anyway
                if (index == -1)
                    return;

                // kill the app if it's running
                String targetProcessName = GeckoAppShell.getContext().getPackageName();
                targetProcessName = targetProcessName + ":" + targetProcessName + ".WebApp" + index;

                ActivityManager am = (ActivityManager) GeckoAppShell.getContext().getSystemService(Context.ACTIVITY_SERVICE);
                List<ActivityManager.RunningAppProcessInfo> procs = am.getRunningAppProcesses();
                if (procs != null) {
                    for (ActivityManager.RunningAppProcessInfo proc : procs) {
                        if (proc.processName.equals(targetProcessName)) {
                            android.os.Process.killProcess(proc.pid);
                            break;
                        }
                    }
                }

                // then nuke the profile
                GeckoProfile.removeProfile(GeckoAppShell.getContext(), "webapp" + index);
            }
        });
    }

    public static void installApk(final Activity context, String filePath, String data) {
        // This is the data that mozApps.install sent to Webapps.jsm.
        JSONObject argsObj = null;

        // We get the manifest url out of javascript here so we can use it as a checksum
        // in a minute, when a package has been installed.
        String manifestUrl = null;
        try {
            argsObj = new JSONObject(data);
            manifestUrl = argsObj.getJSONObject("app").getString("manifestURL");
        } catch (JSONException e) {
            Log.e(LOGTAG, "can't get manifest URL from JSON data", e);
            // TODO: propagate the error back to the mozApps.install caller.
            return;
        }

        // We will check the manifestUrl from the one in the APK.
        // Thus, we can have a one-to-one mapping of apk to receiver.
        final InstallListener receiver = new InstallListener(manifestUrl, argsObj);

        // Listen for packages being installed.
        IntentFilter filter = new IntentFilter(Intent.ACTION_PACKAGE_ADDED);
        filter.addDataScheme("package");
        context.registerReceiver(receiver, filter);

        // Now call the package installer.
        File file = new File(filePath);
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setDataAndType(Uri.fromFile(file), "application/vnd.android.package-archive");

        GeckoAppShell.sActivityHelper.startIntentForActivity(context, intent, new ActivityResultHandler() {
            @Override
            public void onActivityResult(int resultCode, Intent data) {
                // The InstallListener will catch the case where the user pressed install.
                // Now deal with if the user pressed cancel.
                if (resultCode == Activity.RESULT_CANCELED) {
                    try {
                        context.unregisterReceiver(receiver);
                        receiver.cleanup();
                    } catch (java.lang.IllegalArgumentException e) {
                        // IllegalArgumentException happens because resultCode is RESULT_CANCELED
                        // when the user presses the Done button in the install confirmation dialog,
                        // even though the install has been successful (and InstallListener already
                        // unregistered the receiver).
                        Log.e(LOGTAG, "error unregistering install receiver: ", e);
                    }
                }
            }
        });
    }
}
