/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UIAsyncTask;

import android.content.Context;
import android.graphics.Rect;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
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
    private static JSONObject sHoverEnter;
    private static AccessibilityNodeInfo sVirtualCursorNode;
    private static int sCurrentNode;

    // This is the number Brailleback uses to start indexing routing keys.
    private static final int BRAILLE_CLICK_BASE_INDEX = -275000000;
    private static SelfBrailleClient sSelfBrailleClient;

    public static void updateAccessibilitySettings (final Context context) {
        new UIAsyncTask.WithoutParams<Void>(ThreadUtils.getBackgroundHandler()) {
                @Override
                public Void doInBackground() {
                    JSONObject ret = new JSONObject();
                    sEnabled = false;
                    AccessibilityManager accessibilityManager =
                        (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);
                    sEnabled = accessibilityManager.isEnabled() && accessibilityManager.isTouchExplorationEnabled();
                    if (Versions.feature16Plus && sEnabled && sSelfBrailleClient == null) {
                        sSelfBrailleClient = new SelfBrailleClient(GeckoAppShell.getContext(), false);
                    }

                    try {
                        ret.put("enabled", sEnabled);
                    } catch (Exception ex) {
                        Log.e(LOGTAG, "Error building JSON arguments for Accessibility:Settings:", ex);
                    }

                    GeckoAppShell.notifyObservers("Accessibility:Settings", ret.toString());
                    return null;
                }

                @Override
                public void onPostExecute(Void args) {
                    final GeckoAppShell.GeckoInterface geckoInterface = GeckoAppShell.getGeckoInterface();
                    if (geckoInterface == null) {
                        return;
                    }
                    geckoInterface.setAccessibilityEnabled(sEnabled);
                }
            }.execute();
    }

    private static void populateEventFromJSON (AccessibilityEvent event, JSONObject message) {
        final JSONArray textArray = message.optJSONArray("text");
        if (textArray != null) {
            for (int i = 0; i < textArray.length(); i++)
                event.getText().add(textArray.optString(i));
        }

        event.setContentDescription(message.optString("description"));
        event.setEnabled(message.optBoolean("enabled", true));
        event.setChecked(message.optBoolean("checked"));
        event.setPassword(message.optBoolean("password"));
        event.setAddedCount(message.optInt("addedCount", -1));
        event.setRemovedCount(message.optInt("removedCount", -1));
        event.setFromIndex(message.optInt("fromIndex", -1));
        event.setItemCount(message.optInt("itemCount", -1));
        event.setCurrentItemIndex(message.optInt("currentItemIndex", -1));
        event.setBeforeText(message.optString("beforeText"));
        if (Versions.feature14Plus) {
            event.setToIndex(message.optInt("toIndex", -1));
            event.setScrollable(message.optBoolean("scrollable"));
            event.setScrollX(message.optInt("scrollX", -1));
            event.setScrollY(message.optInt("scrollY", -1));
        }
        if (Versions.feature15Plus) {
            event.setMaxScrollX(message.optInt("maxScrollX", -1));
            event.setMaxScrollY(message.optInt("maxScrollY", -1));
        }
    }

    private static void sendDirectAccessibilityEvent(int eventType, JSONObject message) {
        final AccessibilityEvent accEvent = AccessibilityEvent.obtain(eventType);
        accEvent.setClassName(GeckoAccessibility.class.getName());
        accEvent.setPackageName(GeckoAppShell.getContext().getPackageName());
        populateEventFromJSON(accEvent, message);
        AccessibilityManager accessibilityManager =
            (AccessibilityManager) GeckoAppShell.getContext().getSystemService(Context.ACCESSIBILITY_SERVICE);
        try {
            accessibilityManager.sendAccessibilityEvent(accEvent);
        } catch (IllegalStateException e) {
            // Accessibility is off.
        }
    }

    public static boolean isEnabled() {
        return sEnabled;
    }

    public static void sendAccessibilityEvent (final JSONObject message) {
        if (!sEnabled)
            return;

        final int eventType = message.optInt("eventType", -1);
        if (eventType < 0) {
            Log.e(LOGTAG, "No accessibility event type provided");
            return;
        }

        sendAccessibilityEvent(message, eventType);
    }

    public static void sendAccessibilityEvent (final JSONObject message, final int eventType) {
        if (!sEnabled)
            return;

        final String exitView = message.optString("exitView");
        if (exitView.equals("moveNext")) {
            sCurrentNode = VIRTUAL_ENTRY_POINT_AFTER;
        } else if (exitView.equals("movePrevious")) {
            sCurrentNode = VIRTUAL_ENTRY_POINT_BEFORE;
        } else {
            sCurrentNode = VIRTUAL_CURSOR_POSITION;
        }

        if (Versions.preJB) {
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
            final LayerView view = GeckoAppShell.getLayerView();
            if (view == null)
                return;

            if (sVirtualCursorNode == null)
                sVirtualCursorNode = AccessibilityNodeInfo.obtain(view, VIRTUAL_CURSOR_POSITION);
            sVirtualCursorNode.setEnabled(message.optBoolean("enabled", true));
            sVirtualCursorNode.setClickable(message.optBoolean("clickable"));
            sVirtualCursorNode.setCheckable(message.optBoolean("checkable"));
            sVirtualCursorNode.setChecked(message.optBoolean("checked"));
            sVirtualCursorNode.setPassword(message.optBoolean("password"));

            final JSONArray textArray = message.optJSONArray("text");
            StringBuilder sb = new StringBuilder();
            if (textArray != null && textArray.length() > 0) {
                sb.append(textArray.optString(0));
                for (int i = 1; i < textArray.length(); i++) {
                    sb.append(" ").append(textArray.optString(i));
                }
                sVirtualCursorNode.setText(sb.toString());
            }
            sVirtualCursorNode.setContentDescription(message.optString("description"));

            JSONObject bounds = message.optJSONObject("bounds");
            if (bounds != null) {
                Rect relativeBounds = new Rect(bounds.optInt("left"), bounds.optInt("top"),
                                               bounds.optInt("right"), bounds.optInt("bottom"));
                sVirtualCursorNode.setBoundsInParent(relativeBounds);
                int[] locationOnScreen = new int[2];
                view.getLocationOnScreen(locationOnScreen);
                Rect screenBounds = new Rect(relativeBounds);
                screenBounds.offset(locationOnScreen[0], locationOnScreen[1]);
                sVirtualCursorNode.setBoundsInScreen(screenBounds);
            }

            final JSONObject braille = message.optJSONObject("brailleOutput");
            if (braille != null) {
                sendBrailleText(view, braille.optString("text"),
                                braille.optInt("selectionStart"), braille.optInt("selectionEnd"));
            }

            if (eventType == AccessibilityEvent.TYPE_VIEW_HOVER_ENTER) {
                sHoverEnter = message;
            }

            ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        final AccessibilityEvent event = AccessibilityEvent.obtain(eventType);
                        event.setPackageName(GeckoAppShell.getContext().getPackageName());
                        event.setClassName(GeckoAccessibility.class.getName());
                        if (eventType == AccessibilityEvent.TYPE_ANNOUNCEMENT ||
                            eventType == AccessibilityEvent.TYPE_VIEW_SCROLLED) {
                            event.setSource(view, View.NO_ID);
                        } else {
                            event.setSource(view, VIRTUAL_CURSOR_POSITION);
                        }
                        populateEventFromJSON(event, message);
                        view.requestSendAccessibilityEvent(view, event);
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

    public static void setDelegate(LayerView layerview) {
        // Only use this delegate in Jelly Bean.
        if (Versions.feature16Plus) {
            layerview.setAccessibilityDelegate(new GeckoAccessibilityDelegate());
            layerview.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
        }
    }

    public static void setAccessibilityManagerListeners(final Context context) {
        AccessibilityManager accessibilityManager =
            (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);

        accessibilityManager.addAccessibilityStateChangeListener(new AccessibilityManager.AccessibilityStateChangeListener() {
            @Override
            public void onAccessibilityStateChanged(boolean enabled) {
                updateAccessibilitySettings(context);
            }
        });

        if (Versions.feature19Plus) {
            accessibilityManager.addTouchExplorationStateChangeListener(new AccessibilityManager.TouchExplorationStateChangeListener() {
                @Override
                public void onTouchExplorationStateChanged(boolean enabled) {
                    updateAccessibilitySettings(context);
                }
            });
        }
    }

    public static void onLayerViewFocusChanged(LayerView layerview, boolean gainFocus) {
        if (sEnabled)
            GeckoAppShell.notifyObservers("Accessibility:Focus", gainFocus ? "true" : "false");
    }

    public static class GeckoAccessibilityDelegate extends View.AccessibilityDelegate {
        AccessibilityNodeProvider mAccessibilityNodeProvider;

        @Override
        public AccessibilityNodeProvider getAccessibilityNodeProvider(final View host) {
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
                                info.setPackageName(GeckoAppShell.getContext().getPackageName());
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
                                    GeckoAccessibility.sendAccessibilityEvent(sHoverEnter, AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED);
                                } else {
                                    GeckoAppShell.notifyObservers("Accessibility:Focus", "true");
                                }
                                return true;
                            } else if (action == AccessibilityNodeInfo.ACTION_CLICK && virtualViewId == VIRTUAL_CURSOR_POSITION) {
                                GeckoAppShell.notifyObservers("Accessibility:ActivateObject", null);
                                return true;
                            } else if (action == AccessibilityNodeInfo.ACTION_LONG_CLICK && virtualViewId == VIRTUAL_CURSOR_POSITION) {
                                GeckoAppShell.notifyObservers("Accessibility:LongPress", null);
                                return true;
                            } else if (action == AccessibilityNodeInfo.ACTION_SCROLL_FORWARD && virtualViewId == VIRTUAL_CURSOR_POSITION) {
                                GeckoAppShell.notifyObservers("Accessibility:ScrollForward", null);
                                return true;
                            } else if (action == AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD && virtualViewId == VIRTUAL_CURSOR_POSITION) {
                                GeckoAppShell.notifyObservers("Accessibility:ScrollBackward", null);
                                return true;
                            } else if (action == AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT && virtualViewId == VIRTUAL_CURSOR_POSITION) {
                                String traversalRule = "";
                                if (arguments != null) {
                                    traversalRule = arguments.getString(AccessibilityNodeInfo.ACTION_ARGUMENT_HTML_ELEMENT_STRING);
                                }
                                GeckoAppShell.notifyObservers("Accessibility:NextObject", traversalRule);
                                return true;
                            } else if (action == AccessibilityNodeInfo.ACTION_PREVIOUS_HTML_ELEMENT && virtualViewId == VIRTUAL_CURSOR_POSITION) {
                                String traversalRule = "";
                                if (arguments != null) {
                                    traversalRule = arguments.getString(AccessibilityNodeInfo.ACTION_ARGUMENT_HTML_ELEMENT_STRING);
                                }
                                GeckoAppShell.notifyObservers("Accessibility:PreviousObject", traversalRule);
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
                                    JSONObject activationData = new JSONObject();
                                    try {
                                        activationData.put("keyIndex", keyIndex);
                                    } catch (JSONException e) {
                                        return true;
                                    }
                                    GeckoAppShell.notifyObservers("Accessibility:ActivateObject", activationData.toString());
                                } else if (granularity > 0) {
                                    JSONObject movementData = new JSONObject();
                                    try {
                                        movementData.put("direction", "Next");
                                        movementData.put("granularity", granularity);
                                    } catch (JSONException e) {
                                        return true;
                                    }
                                    GeckoAppShell.notifyObservers("Accessibility:MoveByGranularity", movementData.toString());
                                }
                                return true;
                            } else if (action == AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY &&
                                       virtualViewId == VIRTUAL_CURSOR_POSITION) {
                                JSONObject movementData = new JSONObject();
                                int granularity = arguments.getInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT);
                                try {
                                    movementData.put("direction", "Previous");
                                    movementData.put("granularity", granularity);
                                } catch (JSONException e) {
                                    return true;
                                }
                                if (granularity > 0) {
                                    GeckoAppShell.notifyObservers("Accessibility:MoveByGranularity", movementData.toString());
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
