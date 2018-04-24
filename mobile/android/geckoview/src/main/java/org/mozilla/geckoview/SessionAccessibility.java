/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewParent;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeProvider;

public class SessionAccessibility {
    private static final String LOGTAG = "GeckoAccessibility";
    // This is a special ID we use for nodes that are eent sources.
    // We expose it as a fragment and not an actual child of the View node.
    private static final int VIRTUAL_CONTENT_ID = -2;

    // This is the number BrailleBack uses to start indexing routing keys.
    private static final int BRAILLE_CLICK_BASE_INDEX = -275000000;

    // Gecko session we are proxying
    /* package */  final GeckoSession mSession;
    // This is the view that delegates accessibility to us. We also sends event through it.
    private View mView;
    // Aave we reached the last item in content?
    private boolean mLastItem;
    // Used to store the JSON message and populate the event later in the code path.
    private AccessibilityNodeInfo mVirtualContentNode;

    /* package */ SessionAccessibility(final GeckoSession session) {
        mSession = session;

        Settings.getInstance().dispatch();

        session.getEventDispatcher().registerUiThreadListener(new BundleEventListener() {
            @Override
            public void handleMessage(final String event, final GeckoBundle message,
                                      final EventCallback callback) {
                sendAccessibilityEvent(message);
            }
        }, "GeckoView:AccessibilityEvent", null);
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
        mLastItem = false;

        if (mView == null) {
            return;
        }

        mView.setAccessibilityDelegate(new View.AccessibilityDelegate() {
            private AccessibilityNodeProvider mAccessibilityNodeProvider;

            @Override
            public AccessibilityNodeProvider getAccessibilityNodeProvider(final View hostView) {

                if (mAccessibilityNodeProvider == null)
                    mAccessibilityNodeProvider = new AccessibilityNodeProvider() {
                    @Override
                    public AccessibilityNodeInfo createAccessibilityNodeInfo(int virtualDescendantId) {
                        assertAttachedView(hostView);

                        AccessibilityNodeInfo info = (virtualDescendantId == VIRTUAL_CONTENT_ID && mVirtualContentNode != null) ?
                                                     AccessibilityNodeInfo.obtain(mVirtualContentNode) :
                                                     AccessibilityNodeInfo.obtain(mView, virtualDescendantId);

                        switch (virtualDescendantId) {
                        case View.NO_ID:
                            // This is the parent View node.
                            // We intentionally don't add VIRTUAL_CONTENT_ID
                            // as a child. It is a source for events,
                            // but not a member of the tree you
                            // can get to by traversing down.
                            onInitializeAccessibilityNodeInfo(mView, info);
                            info.setClassName("android.webkit.WebView"); // TODO: WTF
                            if (Build.VERSION.SDK_INT >= 19) {
                                Bundle bundle = info.getExtras();
                                bundle.putCharSequence(
                                    "ACTION_ARGUMENT_HTML_ELEMENT_STRING_VALUES",
                                    "ARTICLE,BUTTON,CHECKBOX,COMBOBOX,CONTROL," +
                                    "FOCUSABLE,FRAME,GRAPHIC,H1,H2,H3,H4,H5,H6," +
                                    "HEADING,LANDMARK,LINK,LIST,LIST_ITEM,MAIN," +
                                    "MEDIA,RADIO,SECTION,TABLE,TEXT_FIELD," +
                                    "UNVISITED_LINK,VISITED_LINK");
                            }
                            info.addAction(AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT);
                            info.addChild(hostView, VIRTUAL_CONTENT_ID);
                            break;
                        default:
                            info.setParent(mView);
                            info.setSource(mView, virtualDescendantId);
                            info.setVisibleToUser(mView.isShown());
                            info.setPackageName(GeckoAppShell.getApplicationContext().getPackageName());
                            info.setClassName(mView.getClass().getName());
                            info.setEnabled(true);
                            info.addAction(AccessibilityNodeInfo.ACTION_CLICK);
                            info.addAction(AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY);
                            info.addAction(AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY);
                            info.addAction(AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD);
                            info.addAction(AccessibilityNodeInfo.ACTION_SCROLL_FORWARD);
                            info.addAction(AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT);
                            info.addAction(AccessibilityNodeInfo.ACTION_PREVIOUS_HTML_ELEMENT);
                            info.setMovementGranularities(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER |
                                                          AccessibilityNodeInfo.MOVEMENT_GRANULARITY_WORD |
                                                          AccessibilityNodeInfo.MOVEMENT_GRANULARITY_LINE |
                                                          AccessibilityNodeInfo.MOVEMENT_GRANULARITY_PARAGRAPH);
                            break;
                        }
                        return info;
                    }

                    @Override
                    public boolean performAction(int virtualViewId, int action, Bundle arguments) {
                        assertAttachedView(hostView);

                        if (virtualViewId == View.NO_ID) {
                            return performRootAction(action, arguments);
                        }
                        return performContentAction(action, arguments);
                    }

                    private boolean performRootAction(int action, Bundle arguments) {
                        switch (action) {
                        case AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS:
                        case AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS:
                            final GeckoBundle data = new GeckoBundle(1);
                            data.putBoolean("gainFocus", action == AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS);
                            mSession.getEventDispatcher().dispatch("GeckoView:AccessibilityViewFocused", data);
                            return true;
                        }

                        return mView.performAccessibilityAction(action, arguments);
                    }

                    private boolean performContentAction(int action, Bundle arguments) {
                        final GeckoBundle data;
                        switch (action) {
                        case AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS:
                            final AccessibilityEvent event = obtainEvent(AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED, VIRTUAL_CONTENT_ID);
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
                        case AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT:
                            if (mLastItem) {
                                return false;
                            }
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
                                data = new GeckoBundle(2);
                                data.putString("direction", action == AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY ? "Next" : "Previous");
                                data.putInt("granularity", granularity);
                                mSession.getEventDispatcher().dispatch("GeckoView:AccessibilityByGranularity", data);
                            }
                            return true;
                        }

                        return mView.performAccessibilityAction(action, arguments);
                    }

                    private void assertAttachedView(final View view) {
                        if (view != mView) {
                            throw new AssertionError("delegate used with wrong view.");
                        }
                    }
                };

                return mAccessibilityNodeProvider;
            }

        });
    }

    public static class Settings {
        private static final Settings INSTANCE = new Settings();
        private boolean mEnabled;

        public Settings() {
            EventDispatcher.getInstance().registerUiThreadListener(new BundleEventListener() {
                @Override
                public void handleMessage(final String event, final GeckoBundle message,
                                          final EventCallback callback) {
                    updateAccessibilitySettings();
                }
            }, "GeckoView:AccessibilityReady", null);

            final Context context = GeckoAppShell.getApplicationContext();
            AccessibilityManager accessibilityManager =
                (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);

            mEnabled = accessibilityManager.isEnabled() &&
                       accessibilityManager.isTouchExplorationEnabled();

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
        }

        public static Settings getInstance() {
            return INSTANCE;
        }

        public static boolean isEnabled() {
            return INSTANCE.mEnabled;
        }

        private void updateAccessibilitySettings() {
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    final AccessibilityManager accessibilityManager = (AccessibilityManager)
                            GeckoAppShell.getApplicationContext().getSystemService(Context.ACCESSIBILITY_SERVICE);
                    mEnabled = accessibilityManager.isEnabled() &&
                               accessibilityManager.isTouchExplorationEnabled();

                    dispatch();
                }
            });
        }

        private void dispatch() {
            final GeckoBundle ret = new GeckoBundle(1);
            ret.putBoolean("enabled", mEnabled);
            // "GeckoView:AccessibilitySettings" is dispatched to the Gecko thread.
            EventDispatcher.getInstance().dispatch("GeckoView:AccessibilitySettings", ret);
            // "GeckoView:AccessibilityEnabled" is dispatched to the UI thread.
            EventDispatcher.getInstance().dispatch("GeckoView:AccessibilityEnabled", ret);
        }
    }

    private AccessibilityEvent obtainEvent(final int eventType, final int sourceId) {
        AccessibilityEvent event = AccessibilityEvent.obtain(eventType);
        event.setPackageName(GeckoAppShell.getApplicationContext().getPackageName());
        event.setClassName(SessionAccessibility.class.getName());
        event.setSource(mView, sourceId);

        return event;
    }

    private static void populateEventFromJSON(AccessibilityEvent event, final GeckoBundle message) {
        final String[] textArray = message.getStringArray("text");
        if (textArray != null) {
            for (int i = 0; i < textArray.length; i++)
                event.getText().add(textArray[i] != null ? textArray[i] : "");
        }

        event.setContentDescription(message.getString("description", ""));
        event.setEnabled(message.getBoolean("enabled", true));
        event.setChecked(message.getBoolean("checked"));
        event.setPassword(message.getBoolean("password"));
        event.setAddedCount(message.getInt("addedCount", -1));
        event.setRemovedCount(message.getInt("removedCount", -1));
        event.setFromIndex(message.getInt("fromIndex", -1));
        event.setItemCount(message.getInt("itemCount", -1));
        event.setCurrentItemIndex(message.getInt("currentItemIndex", -1));
        event.setBeforeText(message.getString("beforeText", ""));
        event.setToIndex(message.getInt("toIndex", -1));
        event.setScrollable(message.getBoolean("scrollable"));
        event.setScrollX(message.getInt("scrollX", -1));
        event.setScrollY(message.getInt("scrollY", -1));
        event.setMaxScrollX(message.getInt("maxScrollX", -1));
        event.setMaxScrollY(message.getInt("maxScrollY", -1));
    }

    private void populateNodeInfoFromJSON(AccessibilityNodeInfo node, final GeckoBundle message) {
        node.setEnabled(message.getBoolean("enabled", true));
        node.setClickable(message.getBoolean("clickable"));
        node.setCheckable(message.getBoolean("checkable"));
        node.setChecked(message.getBoolean("checked"));
        node.setPassword(message.getBoolean("password"));

        final String[] textArray = message.getStringArray("text");
        StringBuilder sb = new StringBuilder();
        if (textArray != null && textArray.length > 0) {
            sb.append(textArray[0] != null ? textArray[0] : "");
            for (int i = 1; i < textArray.length; i++) {
                sb.append(' ').append(textArray[i] != null ? textArray[i] : "");
            }
            node.setText(sb.toString());
        }
        node.setContentDescription(message.getString("description", ""));

        final GeckoBundle bounds = message.getBundle("bounds");
        if (bounds != null) {
            Rect screenBounds = new Rect(bounds.getInt("left"), bounds.getInt("top"),
                                         bounds.getInt("right"), bounds.getInt("bottom"));
            node.setBoundsInScreen(screenBounds);

            final Matrix matrix = new Matrix();
            final float[] origin = new float[2];
            mSession.getClientToScreenMatrix(matrix);
            matrix.mapPoints(origin);

            screenBounds.offset((int) -origin[0], (int) -origin[1]);
            node.setBoundsInParent(screenBounds);
        }

    }

    private void sendAccessibilityEvent(final GeckoBundle message) {
        if (mView == null || !Settings.isEnabled())
            return;

        final int eventType = message.getInt("eventType", -1);
        if (eventType < 0) {
            Log.e(LOGTAG, "No accessibility event type provided");
            return;
        }

        int eventSource = VIRTUAL_CONTENT_ID;

        if (eventType == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED) {
            final String exitView = message.getString("exitView", "");

            mLastItem = exitView.equals("moveNext");
            if (mLastItem) {
                return;
            }

            if (exitView.equals("movePrevious")) {
                eventSource = View.NO_ID;
            }
        }

        if (eventSource != View.NO_ID) {
            // In Jelly Bean we populate an AccessibilityNodeInfo with the minimal amount of data to have
            // it work with TalkBack.
            if (mVirtualContentNode == null) {
                mVirtualContentNode = AccessibilityNodeInfo.obtain(mView, eventSource);
            }
            populateNodeInfoFromJSON(mVirtualContentNode, message);
        }

        final AccessibilityEvent accessibilityEvent = obtainEvent(eventType, eventSource);
        populateEventFromJSON(accessibilityEvent, message);
        ((ViewParent) mView).requestSendAccessibilityEvent(mView, accessibilityEvent);
    }

    public void onExploreByTouch(final MotionEvent event) {
      final GeckoBundle data = new GeckoBundle(2);
      data.putDoubleArray("coordinates", new double[] {event.getRawX(), event.getRawY()});
      mSession.getEventDispatcher().dispatch("GeckoView:AccessibilityExploreByTouch", data);
    }
}
