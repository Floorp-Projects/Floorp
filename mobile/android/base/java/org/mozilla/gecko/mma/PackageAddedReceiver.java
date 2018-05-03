package org.mozilla.gecko.mma;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.support.annotation.NonNull;


/**
 * Used to inform as soon as possible of any applications being installed.
 */
public class PackageAddedReceiver extends BroadcastReceiver {

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent.getAction() != null && intent.getData() != null) {
            final String updatedPackage = getInstalledPackageName(intent.getData());
            try {
                MmaDelegate.trackJustInstalledPackage(context, updatedPackage,
                        getIfFirstTimeInstall(context, updatedPackage));
            } catch (PackageManager.NameNotFoundException e) {
                /* Nothing to do */
            }
        }
    }

    // Our intent filter uses the "package" scheme
    // So the intent we receive would be in the form package:org.mozilla.klar
    private String getInstalledPackageName(@NonNull Uri intentData) {
        return intentData.getSchemeSpecificPart();
    }

    private boolean getIfFirstTimeInstall(@NonNull Context context, @NonNull final String packageName)
            throws PackageManager.NameNotFoundException {

        // The situation of an invalid package name(very unlikely) will be handled by the caller
        final PackageInfo packageInfo = context.getPackageManager().getPackageInfo(packageName, 0);
        return packageInfo.firstInstallTime == packageInfo.lastUpdateTime;
    }
}
