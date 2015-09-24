/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.b2gdroid;

import java.io.ByteArrayOutputStream;
import java.util.Iterator;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import android.net.Uri;
import android.util.Base64;
import android.util.Log;

import org.json.JSONObject;
import org.json.JSONArray;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.GeckoEventListener;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;

class Apps extends BroadcastReceiver
           implements GeckoEventListener {
    private static final String LOGTAG = "B2G:Apps";

    private Context mContext;

    Apps(Context context) {
        mContext = context;
        EventDispatcher.getInstance()
                       .registerGeckoThreadListener(this,
                                                    "Apps:GetList",
                                                    "Apps:Launch",
                                                    "Apps:Uninstall");

        // Observe app installation and removal.
        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_PACKAGE_ADDED);
        filter.addAction(Intent.ACTION_PACKAGE_REMOVED);
        filter.addDataScheme("package");
        mContext.registerReceiver(this, filter);
    }

    void destroy() {
        mContext.unregisterReceiver(this);
        EventDispatcher.getInstance()
                       .unregisterGeckoThreadListener(this,
                                                      "Apps:GetList",
                                                      "Apps:Launch",
                                                      "Apps:Uninstall");
    }

    JSONObject activityInfoToJson(ActivityInfo info, PackageManager pm) {
        JSONObject obj = new JSONObject();
        try {
            obj.put("name", info.loadLabel(pm).toString());
            obj.put("packagename", info.packageName);
            obj.put("classname", info.name);

            final ApplicationInfo appInfo = info.applicationInfo;
            // Pre-installed apps can't be uninstalled.
            final boolean removable =
                (appInfo.flags & ApplicationInfo.FLAG_SYSTEM) == 0;

            obj.put("removable", removable);
            obj.put("icon", "android://icon/" + appInfo.packageName);
        } catch(Exception ex) {
            Log.wtf(LOGTAG, "Error building ActivityInfo JSON", ex);
        }
        return obj;
    }

    public void handleMessage(String event, JSONObject message) {
        Log.w(LOGTAG, "Received " + event);

        if ("Apps:GetList".equals(event)) {
            JSONObject ret = new JSONObject();
            JSONArray array = new JSONArray();
            PackageManager pm = mContext.getPackageManager();
            final Intent mainIntent = new Intent(Intent.ACTION_MAIN, null);
            mainIntent.addCategory(Intent.CATEGORY_LAUNCHER);
            final Iterator<ResolveInfo> i = pm.queryIntentActivities(mainIntent, 0).iterator();
            try {
                while (i.hasNext()) {
                    ActivityInfo info = i.next().activityInfo;
                    array.put(activityInfoToJson(info, pm));
                }
                ret.put("apps", array);
            } catch(Exception ex) {
                Log.wtf(LOGTAG, "error, making list of apps", ex);
            }
            EventDispatcher.sendResponse(message, ret);
        } else if ("Apps:Launch".equals(event)) {
            try {
                String className = message.getString("classname");
                String packageName = message.getString("packagename");
                final Intent intent = new Intent(Intent.ACTION_MAIN, null);
                intent.addCategory(Intent.CATEGORY_LAUNCHER);
                intent.setClassName(packageName, className);
                mContext.startActivity(intent);
            } catch(Exception ex) {
                Log.wtf(LOGTAG, "Error launching app", ex);
            }
        } else if ("Apps:Uninstall".equals(event)) {
            try {
                String packageName = message.getString("packagename");
                Uri packageUri = Uri.parse("package:" + packageName);
                final Intent intent = new Intent(Intent.ACTION_UNINSTALL_PACKAGE, packageUri);
                mContext.startActivity(intent);
            } catch(Exception ex) {
                Log.wtf(LOGTAG, "Error uninstalling app", ex);
            }
        }
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        Log.d(LOGTAG, intent.getAction() + " " + intent.getDataString());

        String packageName = intent.getDataString().substring(8);
        String action = intent.getAction();
        if ("android.intent.action.PACKAGE_ADDED".equals(action)) {
            PackageManager pm = mContext.getPackageManager();
            Intent launch = pm.getLaunchIntentForPackage(packageName);
            if (launch == null) {
                Log.d(LOGTAG, "No launchable intent for " + packageName);
                return;
            }
            ActivityInfo info = launch.resolveActivityInfo(pm, 0);

            JSONObject obj = activityInfoToJson(info, pm);
            GeckoEvent e = GeckoEvent.createBroadcastEvent("Android:Apps:Installed", obj.toString());
            GeckoAppShell.sendEventToGecko(e);
        } else if ("android.intent.action.PACKAGE_REMOVED".equals(action)) {
            JSONObject obj = new JSONObject();
            try {
                obj.put("packagename", packageName);
            } catch(Exception ex) {
                Log.wtf(LOGTAG, "Error building PACKAGE_REMOVED JSON", ex);
            }
            GeckoEvent e = GeckoEvent.createBroadcastEvent("Android:Apps:Uninstalled", obj.toString());
            GeckoAppShell.sendEventToGecko(e);
        }
    }
}
