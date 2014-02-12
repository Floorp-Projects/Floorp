/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.webapp;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStreamReader;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;

public class ApkResources {
    private static final String LOGTAG = "GeckoWebappApkResources";
    private final String mPackageName;
    private final ApplicationInfo mInfo;
    private final Context mContext;

    public ApkResources(Context context, String packageName) throws NameNotFoundException {
        mPackageName = packageName;
        mInfo = context.getPackageManager().getApplicationInfo(
                    mPackageName, PackageManager.GET_META_DATA);
        mContext = context;
    }

    private ApplicationInfo info() {
        return mInfo;
    }

    public String getPackageName() {
        return mPackageName;
    }

    private Bundle metadata() {
        return mInfo.metaData;
    }

    public String getManifest(Context context) {
        return readResource(context, "manifest");
    }

    public String getMiniManifest(Context context) {
        return readResource(context, "mini");
    }

    public String getManifestUrl() {
        return metadata().getString("manifestUrl");
    }

    public boolean isPackaged() {
        return "packaged".equals(getWebappType());
    }

    public boolean isDebuggable() {
        return (mInfo.flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0;
    }

    private String readResource(Context context, String resourceName) {
        Uri resourceUri = Uri.parse("android.resource://" + mPackageName
                + "/raw/" + resourceName);
        StringBuilder fileContent = new StringBuilder();
        try {
            BufferedReader r = new BufferedReader(new InputStreamReader(context
                    .getContentResolver().openInputStream(resourceUri)));
            String line;

            while ((line = r.readLine()) != null) {
                fileContent.append(line);
            }
        } catch (FileNotFoundException e) {
            Log.e(LOGTAG, String.format("File not found: \"%s\"", resourceName));
        } catch (IOException e) {
            Log.e(LOGTAG, String.format("Couldn't read file: \"%s\"", resourceName));
        }

        return fileContent.toString();
    }

    public Uri getAppIconUri() {
        return Uri.parse("android.resource://" + mPackageName + "/" + info().icon);
    }

    public Drawable getAppIcon() {
        return info().loadIcon(mContext.getPackageManager());
    }

    public String getWebappType() {
        return metadata().getString("webapp");
    }

    public String getAppName() {
        return info().name;
    }

    /**
     * Which APK installer installed this APK.
     *
     * For OEM backed marketplaces, this will be non-<code>null</code>. Otherwise, <code>null</code>.
     *
     * TODO check that the G+ package installer gives us non-null results.
     *
     * @return the package name of the APK that installed this.
     */
    public String getPackageInstallerName() {
        return mContext.getPackageManager().getInstallerPackageName(mPackageName);
    }

    public Uri getZipFileUri() {
        return Uri.parse("android.resource://" + mPackageName + "/raw/application");
    }

    public File getFileDirectory() {
        File dir = mContext.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS);
        String path = dir.getAbsolutePath().replace(mContext.getPackageName(), mPackageName);

        dir = new File(path);

        if (!dir.exists()) {
            dir.mkdirs();
        }

        return dir;
    }
}
