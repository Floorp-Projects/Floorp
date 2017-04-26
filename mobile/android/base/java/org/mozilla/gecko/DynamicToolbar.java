package org.mozilla.gecko;

import org.mozilla.gecko.PrefsHelper.PrefHandlerBase;
import org.mozilla.gecko.gfx.DynamicToolbarAnimator.PinReason;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.util.ThreadUtils;

import android.os.Build;
import android.os.Bundle;
import android.util.Log;

public class DynamicToolbar {
    private static final String LOGTAG = "DynamicToolbar";

    private static final String STATE_ENABLED = "dynamic_toolbar";
    private static final String CHROME_PREF = "browser.chrome.dynamictoolbar";

    // DynamicToolbar is enabled iff prefEnabled is true *and* accessibilityEnabled is false,
    // so it is disabled by default on startup. We do not enable it until we explicitly get
    // the pref from Gecko telling us to turn it on.
    private volatile boolean prefEnabled;
    private boolean accessibilityEnabled;
    // On some device we have to force-disable the dynamic toolbar because of
    // bugs in the Android code. See bug 1231554.
    private final boolean forceDisabled;

    private final PrefsHelper.PrefHandler prefObserver;
    private LayerView layerView;
    private OnEnabledChangedListener enabledChangedListener;
    private boolean temporarilyVisible;

    public enum VisibilityTransition {
        IMMEDIATE,
        ANIMATE
    }

    /**
     * Listener for changes to the dynamic toolbar's enabled state.
     */
    public interface OnEnabledChangedListener {
        /**
         * This callback is executed on the UI thread.
         */
        public void onEnabledChanged(boolean enabled);
    }

    public DynamicToolbar() {
        // Listen to the dynamic toolbar pref
        prefObserver = new PrefHandler();
        PrefsHelper.addObserver(new String[] { CHROME_PREF }, prefObserver);
        forceDisabled = isForceDisabled();
        if (forceDisabled) {
            Log.i(LOGTAG, "Force-disabling dynamic toolbar for " + Build.MODEL + " (" + Build.DEVICE + "/" + Build.PRODUCT + ")");
        }
    }

    public static boolean isForceDisabled() {
        // Force-disable dynamic toolbar on the variants of the Galaxy Note 10.1
        // and Note 8.0 running Android 4.1.2. (Bug 1231554). This includes
        // the following model numbers:
        //  GT-N8000, GT-N8005, GT-N8010, GT-N8013, GT-N8020
        //  GT-N5100, GT-N5110, GT-N5120
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.JELLY_BEAN
            && (Build.MODEL.startsWith("GT-N80") ||
                Build.MODEL.startsWith("GT-N51"))) {
            return true;
        }
        // Also disable variants of the Galaxy Note 4 on Android 5.0.1 (Bug 1301593)
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP
            && (Build.MODEL.startsWith("SM-N910"))) {
            return true;
        }
        return false;
    }

    public void destroy() {
        PrefsHelper.removeObserver(prefObserver);
    }

    public void setLayerView(LayerView layerView) {
        ThreadUtils.assertOnUiThread();

        this.layerView = layerView;
    }

    public void setEnabledChangedListener(OnEnabledChangedListener listener) {
        ThreadUtils.assertOnUiThread();

        enabledChangedListener = listener;
    }

    public void onSaveInstanceState(Bundle outState) {
        ThreadUtils.assertOnUiThread();

        outState.putBoolean(STATE_ENABLED, prefEnabled);
    }

    public void onRestoreInstanceState(Bundle savedInstanceState) {
        ThreadUtils.assertOnUiThread();

        if (savedInstanceState != null) {
            prefEnabled = savedInstanceState.getBoolean(STATE_ENABLED);
        }
    }

    public boolean isEnabled() {
        ThreadUtils.assertOnUiThread();

        if (forceDisabled) {
            return false;
        }

        return prefEnabled && !accessibilityEnabled;
    }

    public void setAccessibilityEnabled(boolean enabled) {
        ThreadUtils.assertOnUiThread();

        if (accessibilityEnabled == enabled) {
            return;
        }

        // Disable the dynamic toolbar when accessibility features are enabled,
        // and re-read the preference when they're disabled.
        accessibilityEnabled = enabled;
        if (prefEnabled) {
            triggerEnabledListener();
        }
    }

    public void setVisible(boolean visible, VisibilityTransition transition) {
        ThreadUtils.assertOnUiThread();

        if (layerView == null) {
            return;
        }

        // Don't hide the ActionBar/Toolbar, if it's pinned open by TextSelection.
        if (visible == false &&
            layerView.getDynamicToolbarAnimator().isPinnedBy(PinReason.ACTION_MODE)) {
            return;
        }

        final boolean isImmediate = transition == VisibilityTransition.IMMEDIATE;
        if (visible) {
            layerView.getDynamicToolbarAnimator().showToolbar(isImmediate);
        } else {
            layerView.getDynamicToolbarAnimator().hideToolbar(isImmediate);
        }
    }

    public void setPinned(boolean pinned, PinReason reason) {
        ThreadUtils.assertOnUiThread();
        if (layerView == null) {
            return;
        }

        layerView.getDynamicToolbarAnimator().setPinned(pinned, reason);
    }

    private void triggerEnabledListener() {
        if (enabledChangedListener != null) {
            enabledChangedListener.onEnabledChanged(isEnabled());
        }
    }

    private class PrefHandler extends PrefHandlerBase {
        @Override
        public void prefValue(String pref, boolean value) {
            if (value == prefEnabled) {
                return;
            }

            prefEnabled = value;

            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    // If accessibility is enabled, the dynamic toolbar is
                    // forced to be off.
                    if (!accessibilityEnabled) {
                        triggerEnabledListener();
                    }
                }
            });
        }
    }
}
