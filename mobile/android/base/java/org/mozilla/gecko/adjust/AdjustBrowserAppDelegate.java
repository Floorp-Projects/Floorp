package org.mozilla.gecko.adjust;

import android.content.SharedPreferences;
import android.os.Bundle;

import org.mozilla.gecko.AdjustConstants;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.delegates.BrowserAppDelegate;
import org.mozilla.gecko.mozglue.SafeIntent;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.util.IntentUtils;

public class AdjustBrowserAppDelegate extends BrowserAppDelegate {
    private final AdjustHelperInterface adjustHelper;
    private final AttributionHelperListener attributionHelperListener;

    public AdjustBrowserAppDelegate(AttributionHelperListener attributionHelperListener) {
        this.adjustHelper = AdjustConstants.getAdjustHelper();
        this.attributionHelperListener = attributionHelperListener;
    }

    @Override
    public void onCreate(BrowserApp browserApp, Bundle savedInstanceState) {
        adjustHelper.onCreate(browserApp,
                AdjustConstants.MOZ_INSTALL_TRACKING_ADJUST_SDK_APP_TOKEN,
                attributionHelperListener);

        final boolean isInAutomation = IntentUtils.getIsInAutomationFromEnvironment(
                new SafeIntent(browserApp.getIntent()));

        final SharedPreferences prefs = GeckoSharedPrefs.forApp(browserApp);

        // Adjust stores enabled state so this is only necessary because users may have set
        // their data preferences before this feature was implemented and we need to respect
        // those before upload can occur in Adjust.onResume.
        adjustHelper.setEnabled(!isInAutomation
                && prefs.getBoolean(GeckoPreferences.PREFS_HEALTHREPORT_UPLOAD_ENABLED, true));
    }

    @Override
    public void onResume(BrowserApp browserApp) {
        // Needed for Adjust to get accurate session measurements
        adjustHelper.onResume();
    }

    @Override
    public void onPause(BrowserApp browserApp) {
        // Needed for Adjust to get accurate session measurements
        adjustHelper.onPause();
    }
}
