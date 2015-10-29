/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.Map;

import org.mozilla.gecko.tests.BaseRobocopTest;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

/**
 * An Activity that extracts Robocop settings from robotium.config, launches
 * Fennec with the Robocop testing parameters, and finishes itself.
 * <p>
 * This is intended to be used by local testers using |mach robocop --serve|.
 */
public class LaunchFennecWithConfigurationActivity extends Activity {
    @Override
    public void onCreate(Bundle arguments) {
        super.onCreate(arguments);
    }

    @Override
    public void onResume() {
        super.onResume();

        final String configFile = FennecNativeDriver.getFile(BaseRobocopTest.DEFAULT_ROOT_PATH + "/robotium.config");
        final Map<String, String> config = FennecNativeDriver.convertTextToTable(configFile);
        final Intent intent = BaseRobocopTest.createActivityIntent(config);

        intent.setClassName(AppConstants.ANDROID_PACKAGE_NAME, AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);

        this.finish();
        this.startActivity(intent);
    }
}
