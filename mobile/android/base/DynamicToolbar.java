package org.mozilla.gecko;

import java.util.EnumSet;

import org.mozilla.gecko.PrefsHelper.PrefHandlerBase;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.util.ThreadUtils;

import android.os.Bundle;

public class DynamicToolbar {
    private static final String STATE_ENABLED = "dynamic_toolbar";
    private static final String CHROME_PREF = "browser.chrome.dynamictoolbar";

    // DynamicToolbar is enabled iff prefEnabled is true *and* accessibilityEnabled is false,
    // so it is disabled by default on startup. We do not enable it until we explicitly get
    // the pref from Gecko telling us to turn it on.
    private volatile boolean prefEnabled;
    private boolean accessibilityEnabled;

    private final int prefObserverId;
    private final EnumSet<PinReason> pinFlags = EnumSet.noneOf(PinReason.class);
    private LayerView layerView;
    private OnEnabledChangedListener enabledChangedListener;

    public enum PinReason {
        RELAYOUT,
        ACTION_MODE
    }

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
        prefObserverId = PrefsHelper.getPref(CHROME_PREF, new PrefHandler());
    }

    public void destroy() {
        PrefsHelper.removeObserver(prefObserverId);
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

        final boolean immediate = transition == VisibilityTransition.IMMEDIATE;
        if (visible) {
            layerView.getLayerMarginsAnimator().showMargins(immediate);
        } else {
            layerView.getLayerMarginsAnimator().hideMargins(immediate);
        }
    }

    public void setPinned(boolean pinned, PinReason reason) {
        ThreadUtils.assertOnUiThread();

        if (layerView == null) {
            return;
        }

        if (pinned) {
            pinFlags.add(reason);
        } else {
            pinFlags.remove(reason);
        }

        layerView.getLayerMarginsAnimator().setMarginsPinned(!pinFlags.isEmpty());
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

        @Override
        public boolean isObserver() {
            // We want to be notified of changes to be able to switch mode
            // without restarting.
            return true;
        }
    }
}