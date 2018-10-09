/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.mozglue.JNIObject;

import android.content.Context;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.text.InputType;
import android.util.Log;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewParent;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeProvider;

public class SessionAccessibility {
    private static final String LOGTAG = "GeckoAccessibility";
    private static final boolean DEBUG = false;

    // This is the number BrailleBack uses to start indexing routing keys.
    private static final int BRAILLE_CLICK_BASE_INDEX = -275000000;

    private static final int ACTION_SET_TEXT = 0x200000;
    private static final String ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE =
            "ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE";

    /* package */ final class NodeProvider extends AccessibilityNodeProvider {
        @Override
        public AccessibilityNodeInfo createAccessibilityNodeInfo(int virtualDescendantId) {
            AccessibilityNodeInfo node = AccessibilityNodeInfo.obtain(mView, virtualDescendantId);
            if (Build.VERSION.SDK_INT < 17 || mView.getDisplay() != null) {
                // When running junit tests we don't have a display
                mView.onInitializeAccessibilityNodeInfo(node);
            }
            node.setClassName("android.webkit.WebView");
            return node;
        }

        @Override
        public boolean performAction(final int virtualViewId, int action, Bundle arguments) {
            final GeckoBundle data;
            switch (action) {
            case AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS:
                final AccessibilityEvent event = AccessibilityEvent.obtain(AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED);
                event.setPackageName(GeckoAppShell.getApplicationContext().getPackageName());
                event.setSource(mView, virtualViewId);
                ((ViewParent) mView).requestSendAccessibilityEvent(mView, event);
                return true;
            case AccessibilityNodeInfo.ACTION_CLICK:
                mSession.getEventDispatcher().dispatch("GeckoView:AccessibilityActivate", null);
                return true;
            case AccessibilityNodeInfo.ACTION_LONG_CLICK:
                mSession.getEventDispatcher().dispatch("GeckoView:AccessibilityLongPress", null);
                return true;
            case AccessibilityNodeInfo.ACTION_SCROLL_FORWARD:
                mSession.getEventDispatcher().dispatch("GeckoView:AccessibilityScrollForward", null);
                return true;
            case AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD:
                mSession.getEventDispatcher().dispatch("GeckoView:AccessibilityScrollBackward", null);
                return true;
            case AccessibilityNodeInfo.ACTION_SELECT:
                mSession.getEventDispatcher().dispatch("GeckoView:AccessibilitySelect", null);
                return true;
            case AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT:
            case AccessibilityNodeInfo.ACTION_PREVIOUS_HTML_ELEMENT:
                if (arguments != null) {
                    data = new GeckoBundle(1);
                    data.putString("rule", arguments.getString(AccessibilityNodeInfo.ACTION_ARGUMENT_HTML_ELEMENT_STRING));
                } else {
                    data = null;
                }
                mSession.getEventDispatcher().dispatch(action == AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT ?
                                                       "GeckoView:AccessibilityNext" : "GeckoView:AccessibilityPrevious", data);
                return true;
            case AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY:
            case AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY:
                // XXX: Self brailling gives this action with a bogus argument instead of an actual click action;
                // the argument value is the BRAILLE_CLICK_BASE_INDEX - the index of the routing key that was hit.
                // Other negative values are used by ChromeVox, but we don't support them.
                // FAKE_GRANULARITY_READ_CURRENT = -1
                // FAKE_GRANULARITY_READ_TITLE = -2
                // FAKE_GRANULARITY_STOP_SPEECH = -3
                // FAKE_GRANULARITY_CHANGE_SHIFTER = -4
                int granularity = arguments.getInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT);
                if (granularity <= BRAILLE_CLICK_BASE_INDEX) {
                    int keyIndex = BRAILLE_CLICK_BASE_INDEX - granularity;
                    data = new GeckoBundle(1);
                    data.putInt("keyIndex", keyIndex);
                    mSession.getEventDispatcher().dispatch("GeckoView:AccessibilityActivate", data);
                } else if (granularity > 0) {
                    boolean extendSelection = arguments.getBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN);
                    data = new GeckoBundle(3);
                    data.putString("direction", action == AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY ? "Next" : "Previous");
                    data.putInt("granularity", granularity);
                    data.putBoolean("select", extendSelection);
                    mSession.getEventDispatcher().dispatch("GeckoView:AccessibilityByGranularity", data);
                }
                return true;
            case AccessibilityNodeInfo.ACTION_SET_SELECTION:
                if (arguments == null) {
                    return false;
                }
                int selectionStart = arguments.getInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT);
                int selectionEnd = arguments.getInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT);
                data = new GeckoBundle(2);
                data.putInt("start", selectionStart);
                data.putInt("end", selectionEnd);
                mSession.getEventDispatcher().dispatch("GeckoView:AccessibilitySetSelection", data);
                return true;
            case AccessibilityNodeInfo.ACTION_CUT:
            case AccessibilityNodeInfo.ACTION_COPY:
            case AccessibilityNodeInfo.ACTION_PASTE:
                data = new GeckoBundle(1);
                data.putInt("action", action);
                mSession.getEventDispatcher().dispatch("GeckoView:AccessibilityClipboard", data);
                return true;
            }

            return mView.performAccessibilityAction(action, arguments);
        }
    };

    // Gecko session we are proxying
    /* package */  final GeckoSession mSession;
    // This is the view that delegates accessibility to us. We also sends event through it.
    private View mView;
    // The native portion of the node provider.
    /* package */ final NativeProvider nativeProvider = new NativeProvider();

    private boolean mAttached = false;

    /* package */ SessionAccessibility(final GeckoSession session) {
        mSession = session;

        Settings.updateAccessibilitySettings();
    }

    /**
      * Get the View instance that delegates accessibility to this session.
      *
      * @return View instance.
      */
    public View getView() {
        return mView;
    }

    /**
      * Set the View instance that should delegate accessibility to this session.
      *
      * @param view View instance.
      */
    public void setView(final View view) {
        if (mView != null) {
            mView.setAccessibilityDelegate(null);
        }

        mView = view;

        if (mView == null) {
            return;
        }

        mView.setAccessibilityDelegate(new View.AccessibilityDelegate() {
            private NodeProvider mProvider;

            @Override
            public AccessibilityNodeProvider getAccessibilityNodeProvider(final View hostView) {
                if (hostView != mView) {
                    return null;
                }
                if (mProvider == null) {
                    mProvider = new NodeProvider();
                }
                return mProvider;
            }
        });
    }

    private static class Settings {
        private static final String FORCE_ACCESSIBILITY_PREF = "accessibility.force_disabled";

        private static volatile boolean sEnabled;
        private static volatile boolean sTouchExplorationEnabled;
        /* package */ static volatile boolean sForceEnabled;

        static {
            final Context context = GeckoAppShell.getApplicationContext();
            AccessibilityManager accessibilityManager =
                (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);

            accessibilityManager.addAccessibilityStateChangeListener(
            new AccessibilityManager.AccessibilityStateChangeListener() {
                @Override
                public void onAccessibilityStateChanged(boolean enabled) {
                    updateAccessibilitySettings();
                }
            }
            );

            if (Build.VERSION.SDK_INT >= 19) {
                accessibilityManager.addTouchExplorationStateChangeListener(
                new AccessibilityManager.TouchExplorationStateChangeListener() {
                    @Override
                    public void onTouchExplorationStateChanged(boolean enabled) {
                        updateAccessibilitySettings();
                    }
                }
                );
            }

            PrefsHelper.PrefHandler prefHandler = new PrefsHelper.PrefHandlerBase() {
                @Override
                public void prefValue(String pref, int value) {
                    if (pref.equals(FORCE_ACCESSIBILITY_PREF)) {
                        sForceEnabled = value < 0;
                        dispatch();
                    }
                }
            };
            PrefsHelper.addObserver(new String[]{ FORCE_ACCESSIBILITY_PREF }, prefHandler);
        }

        public static boolean isEnabled() {
            return sEnabled || sForceEnabled;
        }

        public static boolean isTouchExplorationEnabled() {
            return sTouchExplorationEnabled || sForceEnabled;
        }

        public static void updateAccessibilitySettings() {
            final AccessibilityManager accessibilityManager = (AccessibilityManager)
                    GeckoAppShell.getApplicationContext().getSystemService(Context.ACCESSIBILITY_SERVICE);
            sEnabled = accessibilityManager.isEnabled();
            sTouchExplorationEnabled = sEnabled && accessibilityManager.isTouchExplorationEnabled();
            dispatch();
        }

        /* package */ static void dispatch() {
            final GeckoBundle ret = new GeckoBundle(1);
            ret.putBoolean("enabled", isTouchExplorationEnabled());
            // "GeckoView:AccessibilitySettings" is dispatched to the Gecko thread.
            EventDispatcher.getInstance().dispatch("GeckoView:AccessibilitySettings", ret);
            // "GeckoView:AccessibilityEnabled" is dispatched to the UI thread.
            EventDispatcher.getInstance().dispatch("GeckoView:AccessibilityEnabled", ret);
        }
    }

    public boolean onMotionEvent(final MotionEvent event) {
        if (!Settings.isTouchExplorationEnabled()) {
            return false;
        }

        if (event.getSource() != InputDevice.SOURCE_TOUCHSCREEN) {
            return false;
        }

        final int action = event.getActionMasked();
        if ((action != MotionEvent.ACTION_HOVER_MOVE) &&
                (action != MotionEvent.ACTION_HOVER_ENTER) &&
                (action != MotionEvent.ACTION_HOVER_EXIT)) {
            return false;
        }

        final GeckoBundle data = new GeckoBundle(2);
        data.putDoubleArray("coordinates", new double[] {event.getRawX(), event.getRawY()});
        mSession.getEventDispatcher().dispatch("GeckoView:AccessibilityExploreByTouch", data);
        return true;
    }

    /* package */ final class NativeProvider extends JNIObject {
        @WrapForJNI(calledFrom = "ui")
        private void setAttached(final boolean attached) {
            mAttached = attached;
        }

        @Override // JNIObject
        protected void disposeNative() {
            // Disposal happens in native code.
            throw new UnsupportedOperationException();
        }
    }
}
