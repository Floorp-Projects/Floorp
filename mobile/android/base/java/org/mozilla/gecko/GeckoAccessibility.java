/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.ViewParent;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeProvider;

import com.googlecode.eyesfree.braille.selfbraille.SelfBrailleClient;
import com.googlecode.eyesfree.braille.selfbraille.WriteData;

public class GeckoAccessibility {
    private static final String LOGTAG = "GeckoAccessibility";
    private static final int VIRTUAL_ENTRY_POINT_BEFORE = 1;
    private static final int VIRTUAL_CURSOR_POSITION = 2;
    private static final int VIRTUAL_ENTRY_POINT_AFTER = 3;

    private static boolean sEnabled;
    // Used to store the JSON message and populate the event later in the code path.
    private static GeckoBundle sHoverEnter;
    private static AccessibilityNodeInfo sVirtualCursorNode;
    private static int sCurrentNode;

    // This is the number Brailleback uses to start indexing routing keys.
    private static final int BRAILLE_CLICK_BASE_INDEX = -275000000;
    private static SelfBrailleClient sSelfBrailleClient;

    public static void updateAccessibilitySettings (final Context context) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final AccessibilityManager accessibilityManager = (AccessibilityManager)
                        context.getSystemService(Context.ACCESSIBILITY_SERVICE);
                sEnabled = accessibilityManager.isEnabled() &&
                           accessibilityManager.isTouchExplorationEnabled();

                if (Build.VERSION.SDK_INT >= 16 && sEnabled && sSelfBrailleClient == null) {
                    sSelfBrailleClient = new SelfBrailleClient(context, false);
                }

                final GeckoBundle ret = new GeckoBundle(1);
                ret.putBoolean("enabled", sEnabled);
                // "Accessibility:Settings" is dispatched to the Gecko thread.
                EventDispatcher.getInstance().dispatch("Accessibility:Settings", ret);
                // "Accessibility:Enabled" is dispatched to the UI thread.
                EventDispatcher.getInstance().dispatch("Accessibility:Enabled", ret);
            }
        });
    }

    private static void populateEventFromJSON (AccessibilityEvent event, GeckoBundle message) {
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

    private static void sendDirectAccessibilityEvent(int eventType, GeckoBundle message) {
        final Context context = GeckoAppShell.getApplicationContext();
        final AccessibilityEvent accEvent = AccessibilityEvent.obtain(eventType);
        accEvent.setClassName(GeckoAccessibility.class.getName());
        accEvent.setPackageName(context.getPackageName());
        populateEventFromJSON(accEvent, message);
        AccessibilityManager accessibilityManager =
            (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);
        try {
            accessibilityManager.sendAccessibilityEvent(accEvent);
        } catch (IllegalStateException e) {
            // Accessibility is off.
        }
    }

    public static boolean isEnabled() {
        return sEnabled;
    }

    public static void sendAccessibilityEvent(final GeckoView view,
                                              final GeckoBundle message) {
        if (!sEnabled)
            return;

        final int eventType = message.getInt("eventType", -1);
        if (eventType < 0) {
            Log.e(LOGTAG, "No accessibility event type provided");
            return;
        }

        sendAccessibilityEvent(view, message, eventType);
    }

    public static void sendAccessibilityEvent(final GeckoView view, final GeckoBundle message,
                                              final int eventType) {
        if (!sEnabled)
            return;

        final String exitView = message.getString("exitView", "");
        if (exitView.equals("moveNext")) {
            sCurrentNode = VIRTUAL_ENTRY_POINT_AFTER;
        } else if (exitView.equals("movePrevious")) {
            sCurrentNode = VIRTUAL_ENTRY_POINT_BEFORE;
        } else {
            sCurrentNode = VIRTUAL_CURSOR_POSITION;
        }

        if (Build.VERSION.SDK_INT < 16) {
            // Before Jelly Bean we send events directly from here while spoofing the source by setting
            // the package and class name manually.
            ThreadUtils.postToBackgroundThread(new Runnable() {
                    @Override
                    public void run() {
                        sendDirectAccessibilityEvent(eventType, message);
                }
            });
        } else {
            // In Jelly Bean we populate an AccessibilityNodeInfo with the minimal amount of data to have
            // it work with TalkBack.
            if (sVirtualCursorNode == null) {
                sVirtualCursorNode = AccessibilityNodeInfo.obtain(view, VIRTUAL_CURSOR_POSITION);
            }
            sVirtualCursorNode.setEnabled(message.getBoolean("enabled", true));
            sVirtualCursorNode.setClickable(message.getBoolean("clickable"));
            sVirtualCursorNode.setCheckable(message.getBoolean("checkable"));
            sVirtualCursorNode.setChecked(message.getBoolean("checked"));
            sVirtualCursorNode.setPassword(message.getBoolean("password"));

            final String[] textArray = message.getStringArray("text");
            StringBuilder sb = new StringBuilder();
            if (textArray != null && textArray.length > 0) {
                sb.append(textArray[0] != null ? textArray[0] : "");
                for (int i = 1; i < textArray.length; i++) {
                    sb.append(' ').append(textArray[i] != null ? textArray[i] : "");
                }
                sVirtualCursorNode.setText(sb.toString());
            }
            sVirtualCursorNode.setContentDescription(message.getString("description", ""));

            final GeckoBundle bounds = message.getBundle("bounds");
            if (bounds != null) {
                Rect relativeBounds = new Rect(bounds.getInt("left"), bounds.getInt("top"),
                                               bounds.getInt("right"), bounds.getInt("bottom"));
                sVirtualCursorNode.setBoundsInParent(relativeBounds);
                int[] locationOnScreen = new int[2];
                view.getLocationOnScreen(locationOnScreen);
                locationOnScreen[1] += view.getCurrentToolbarHeight();
                Rect screenBounds = new Rect(relativeBounds);
                screenBounds.offset(locationOnScreen[0], locationOnScreen[1]);
                sVirtualCursorNode.setBoundsInScreen(screenBounds);
            }

            final GeckoBundle braille = message.getBundle("brailleOutput");
            if (braille != null) {
                sendBrailleText(view, braille.getString("text", ""),
                                braille.getInt("selectionStart"), braille.getInt("selectionEnd"));
            }

            if (eventType == AccessibilityEvent.TYPE_VIEW_HOVER_ENTER) {
                sHoverEnter = message;
            }

            ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        final AccessibilityEvent event = AccessibilityEvent.obtain(eventType);
                        event.setPackageName(GeckoAppShell.getApplicationContext().getPackageName());
                        event.setClassName(GeckoAccessibility.class.getName());
                        if (eventType == AccessibilityEvent.TYPE_ANNOUNCEMENT ||
                            eventType == AccessibilityEvent.TYPE_VIEW_SCROLLED) {
                            event.setSource(view, View.NO_ID);
                        } else {
                            event.setSource(view, VIRTUAL_CURSOR_POSITION);
                        }
                        populateEventFromJSON(event, message);
                        ((ViewParent) view).requestSendAccessibilityEvent(view, event);
                    }
                });

        }
    }

    private static void sendBrailleText(final View view, final String text, final int selectionStart, final int selectionEnd) {
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain(view, VIRTUAL_CURSOR_POSITION);
        WriteData data = WriteData.forInfo(info);
        data.setText(text);
        // Set either the focus blink or the current caret position/selection
        data.setSelectionStart(selectionStart);
        data.setSelectionEnd(selectionEnd);
        sSelfBrailleClient.write(data);
    }

    public static void setDelegate(View view) {
        // Only use this delegate in Jelly Bean.
        if (Build.VERSION.SDK_INT >= 16) {
            view.setAccessibilityDelegate(new GeckoAccessibilityDelegate());
            view.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
        }

        view.setOnFocusChangeListener(new View.OnFocusChangeListener() {
            @Override
            public void onFocusChange(final View v, final boolean hasFocus) {
                onLayerViewFocusChanged(hasFocus);
            }
        });
    }

    @TargetApi(19)
    public static void setAccessibilityManagerListeners(final Context context) {
        AccessibilityManager accessibilityManager =
            (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);

        accessibilityManager.addAccessibilityStateChangeListener(new AccessibilityManager.AccessibilityStateChangeListener() {
            @Override
            public void onAccessibilityStateChanged(boolean enabled) {
                updateAccessibilitySettings(context);
            }
        });

        if (Build.VERSION.SDK_INT >= 19) {
            accessibilityManager.addTouchExplorationStateChangeListener(new AccessibilityManager.TouchExplorationStateChangeListener() {
                @Override
                public void onTouchExplorationStateChanged(boolean enabled) {
                    updateAccessibilitySettings(context);
                }
            });
        }
    }

    public static void onLayerViewFocusChanged(boolean gainFocus) {
        if (sEnabled) {
            final GeckoBundle data = new GeckoBundle(1);
            data.putBoolean("gainFocus", gainFocus);
            EventDispatcher.getInstance().dispatch("Accessibility:Focus", data);
        }
    }

    public static class GeckoAccessibilityDelegate extends View.AccessibilityDelegate {
        AccessibilityNodeProvider mAccessibilityNodeProvider;

        @Override
        public AccessibilityNodeProvider getAccessibilityNodeProvider(final View hostView) {
            if (!(hostView instanceof GeckoView)) {
                return super.getAccessibilityNodeProvider(hostView);
            }
            final GeckoView host = (GeckoView) hostView;

            if (mAccessibilityNodeProvider == null)
                // The accessibility node structure for web content consists of 3 LayerView child nodes:
                // 1. VIRTUAL_ENTRY_POINT_BEFORE: Represents the entry point before the LayerView.
                // 2. VIRTUAL_CURSOR_POSITION: Represents the current position of the virtual cursor.
                // 3. VIRTUAL_ENTRY_POINT_AFTER: Represents the entry point after the LayerView.
                mAccessibilityNodeProvider = new AccessibilityNodeProvider() {
                        @Override
                        public AccessibilityNodeInfo createAccessibilityNodeInfo(int virtualDescendantId) {
                            AccessibilityNodeInfo info = (virtualDescendantId == VIRTUAL_CURSOR_POSITION && sVirtualCursorNode != null) ?
                                AccessibilityNodeInfo.obtain(sVirtualCursorNode) :
                                AccessibilityNodeInfo.obtain(host, virtualDescendantId);

                            switch (virtualDescendantId) {
                            case View.NO_ID:
                                // This is the parent LayerView node, populate it with children.
                                onInitializeAccessibilityNodeInfo(host, info);
                                info.addChild(host, VIRTUAL_ENTRY_POINT_BEFORE);
                                info.addChild(host, VIRTUAL_CURSOR_POSITION);
                                info.addChild(host, VIRTUAL_ENTRY_POINT_AFTER);
                                break;
                            default:
                                info.setParent(host);
                                info.setSource(host, virtualDescendantId);
                                info.setVisibleToUser(host.isShown());
                                info.setPackageName(GeckoAppShell.getApplicationContext().getPackageName());
                                info.setClassName(host.getClass().getName());
                                info.setEnabled(true);
                                info.addAction(AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS);
                                info.addAction(AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS);
                                info.addAction(AccessibilityNodeInfo.ACTION_CLICK);
                                info.addAction(AccessibilityNodeInfo.ACTION_LONG_CLICK);
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
                        public boolean performAction (int virtualViewId, int action, Bundle arguments) {
                            if (action == AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS) {
                                // The accessibility focus is permanently on the middle node, VIRTUAL_CURSOR_POSITION.
                                // When we enter the view forward or backward we just ask Gecko to get focus, keeping the current position.
                                if (virtualViewId == VIRTUAL_CURSOR_POSITION && sHoverEnter != null) {
                                    GeckoAccessibility.sendAccessibilityEvent(host, sHoverEnter, AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED);
                                } else {
                                    final GeckoBundle data = new GeckoBundle(1);
                                    data.putBoolean("gainFocus", true);
                                    EventDispatcher.getInstance().dispatch("Accessibility:Focus", data);
                                }
                                return true;
                            } else if (action == AccessibilityNodeInfo.ACTION_CLICK && virtualViewId == VIRTUAL_CURSOR_POSITION) {
                                EventDispatcher.getInstance().dispatch("Accessibility:ActivateObject", null);
                                return true;
                            } else if (action == AccessibilityNodeInfo.ACTION_LONG_CLICK && virtualViewId == VIRTUAL_CURSOR_POSITION) {
                                EventDispatcher.getInstance().dispatch("Accessibility:LongPress", null);
                                return true;
                            } else if (action == AccessibilityNodeInfo.ACTION_SCROLL_FORWARD && virtualViewId == VIRTUAL_CURSOR_POSITION) {
                                EventDispatcher.getInstance().dispatch("Accessibility:ScrollForward", null);
                                return true;
                            } else if (action == AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD && virtualViewId == VIRTUAL_CURSOR_POSITION) {
                                EventDispatcher.getInstance().dispatch("Accessibility:ScrollBackward", null);
                                return true;
                            } else if ((action == AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT ||
                                        action == AccessibilityNodeInfo.ACTION_PREVIOUS_HTML_ELEMENT) && virtualViewId == VIRTUAL_CURSOR_POSITION) {
                                final GeckoBundle data;
                                if (arguments != null) {
                                    data = new GeckoBundle(1);
                                    data.putString("rule", arguments.getString(AccessibilityNodeInfo.ACTION_ARGUMENT_HTML_ELEMENT_STRING));
                                } else {
                                    data = null;
                                }
                                EventDispatcher.getInstance().dispatch(action == AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT ?
                                        "Accessibility:NextObject" : "Accessibility:PreviousObject", data);
                                return true;
                            } else if (action == AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY &&
                                       virtualViewId == VIRTUAL_CURSOR_POSITION) {
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
                                    final GeckoBundle data = new GeckoBundle(1);
                                    data.putInt("keyIndex", keyIndex);
                                    EventDispatcher.getInstance().dispatch("Accessibility:ActivateObject", data);
                                } else if (granularity > 0) {
                                    final GeckoBundle data = new GeckoBundle(2);
                                    data.putString("direction", "Next");
                                    data.putInt("granularity", granularity);
                                    EventDispatcher.getInstance().dispatch("Accessibility:MoveByGranularity", data);
                                }
                                return true;
                            } else if (action == AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY &&
                                       virtualViewId == VIRTUAL_CURSOR_POSITION) {
                                int granularity = arguments.getInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT);
                                final GeckoBundle data = new GeckoBundle(2);
                                data.putString("direction", "Previous");
                                data.putInt("granularity", granularity);
                                if (granularity > 0) {
                                    EventDispatcher.getInstance().dispatch("Accessibility:MoveByGranularity", data);
                                }
                                return true;
                            }
                            return host.performAccessibilityAction(action, arguments);
                        }
                    };

            return mAccessibilityNodeProvider;
        }
    }
}
