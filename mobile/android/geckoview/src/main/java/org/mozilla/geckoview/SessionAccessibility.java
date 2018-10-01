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
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.InputType;
import android.util.Log;
import android.util.SparseArray;
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

    // This is a special ID we use for nodes that are event sources.
    // We expose it as a fragment and not an actual child of the View node.
    private static final int VIRTUAL_CONTENT_ID = -2;

    // This is the number BrailleBack uses to start indexing routing keys.
    private static final int BRAILLE_CLICK_BASE_INDEX = -275000000;

    private static final int ACTION_SET_TEXT = 0x200000;
    private static final String ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE =
            "ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE";

    /* package */ final class NodeProvider extends AccessibilityNodeProvider {
        @Override
        public AccessibilityNodeInfo createAccessibilityNodeInfo(int virtualDescendantId) {
            AccessibilityNodeInfo info = getAutoFillNode(virtualDescendantId);
            if (info != null) {
                // Try auto-fill nodes first.
                return info;
            }

            info = (virtualDescendantId == VIRTUAL_CONTENT_ID && mVirtualContentNode != null)
                   ? AccessibilityNodeInfo.obtain(mVirtualContentNode)
                   : AccessibilityNodeInfo.obtain(mView, virtualDescendantId);

            switch (virtualDescendantId) {
            case View.NO_ID:
                // This is the parent View node.
                // We intentionally don't add VIRTUAL_CONTENT_ID
                // as a child. It is a source for events,
                // but not a member of the tree you
                // can get to by traversing down.
                if (Build.VERSION.SDK_INT < 17 || mView.getDisplay() != null) {
                    // When running junit tests we don't have a display
                    mView.onInitializeAccessibilityNodeInfo(info);
                }
                info.setPackageName(GeckoAppShell.getApplicationContext().getPackageName());
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

                if (mAutoFillRoots != null) {
                    // Add auto-fill nodes.
                    if (DEBUG) {
                        Log.d(LOGTAG, "Adding roots " + mAutoFillRoots);
                    }
                    for (int i = 0; i < mAutoFillRoots.size(); i++) {
                        info.addChild(mView, mAutoFillRoots.keyAt(i));
                    }
                }
                break;
            default:
                info.setParent(mView);
                info.setSource(mView, virtualDescendantId);
                info.setVisibleToUser(mView.isShown());
                info.setPackageName(GeckoAppShell.getApplicationContext().getPackageName());
                info.setEnabled(true);
                info.addAction(AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY);
                info.addAction(AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY);
                info.addAction(AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD);
                info.addAction(AccessibilityNodeInfo.ACTION_SCROLL_FORWARD);
                info.addAction(AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT);
                info.addAction(AccessibilityNodeInfo.ACTION_PREVIOUS_HTML_ELEMENT);
                info.setMovementGranularities(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER |
                                              AccessibilityNodeInfo.MOVEMENT_GRANULARITY_WORD |
                                              AccessibilityNodeInfo.MOVEMENT_GRANULARITY_LINE);
                break;
            }
            return info;
        }

        @Override
        public boolean performAction(int virtualViewId, int action, Bundle arguments) {
            if (virtualViewId == View.NO_ID) {
                return performRootAction(action, arguments);
            }
            if (action == AccessibilityNodeInfo.ACTION_SET_TEXT) {
                final String value = arguments.getString(Build.VERSION.SDK_INT >= 21
                        ? AccessibilityNodeInfo.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE
                        : ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE);
                return performAutoFill(virtualViewId, value);
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

        @SuppressWarnings("fallthrough")
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
            case AccessibilityNodeInfo.ACTION_SELECT:
                mSession.getEventDispatcher().dispatch("GeckoView:AccessibilitySelect", null);
                return true;
            case AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT:
                if (mLastItem) {
                    return false;
                }
                // fall-through
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
    // Have we reached the last item in content?
    private boolean mLastItem;
    // Used to store the JSON message and populate the event later in the code path.
    private AccessibilityNodeInfo mVirtualContentNode;
    // Auto-fill nodes.
    private SparseArray<GeckoBundle> mAutoFillNodes;
    private SparseArray<EventCallback> mAutoFillRoots;
    private int mAutoFillFocusedId = View.NO_ID;

    private boolean mAttached = false;

    /* package */ SessionAccessibility(final GeckoSession session) {
        mSession = session;

        Settings.updateAccessibilitySettings();

        session.getEventDispatcher().registerUiThreadListener(new BundleEventListener() {
                @Override
                public void handleMessage(final String event, final GeckoBundle message,
                                          final EventCallback callback) {
                    if ("GeckoView:AccessibilityEvent".equals(event)) {
                        sendAccessibilityEvent(message);
                    } else if ("GeckoView:AddAutoFill".equals(event)) {
                        addAutoFill(message, callback);
                    } else if ("GeckoView:ClearAutoFill".equals(event)) {
                        clearAutoFill();
                    } else if ("GeckoView:OnAutoFillFocus".equals(event)) {
                        onAutoFillFocus(message);
                    }
                }
            },
            "GeckoView:AccessibilityEvent",
            "GeckoView:AddAutoFill",
            "GeckoView:ClearAutoFill",
            "GeckoView:OnAutoFillFocus",
            null);
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

    private AccessibilityEvent obtainEvent(final int eventType, final int sourceId) {
        AccessibilityEvent event = AccessibilityEvent.obtain(eventType);
        event.setPackageName(GeckoAppShell.getApplicationContext().getPackageName());
        event.setSource(mView, sourceId);

        return event;
    }

    private static void populateEventFromJSON(AccessibilityEvent event, final GeckoBundle message) {
        final String[] textArray = message.getStringArray("text");
        if (textArray != null) {
            for (int i = 0; i < textArray.length; i++)
                event.getText().add(textArray[i] != null ? textArray[i] : "");
        }

        if (message.containsKey("className"))
            event.setClassName(message.getString("className"));
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
        node.setCheckable(message.getBoolean("checkable"));
        node.setChecked(message.getBoolean("checked"));
        node.setPassword(message.getBoolean("password"));
        node.setFocusable(message.getBoolean("focusable"));
        node.setFocused(message.getBoolean("focused"));
        node.setSelected(message.getBoolean("selected"));

        node.setClassName(message.getString("className", "android.view.View"));

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

        if (Build.VERSION.SDK_INT >= 18 && message.getBoolean("editable")) {
            node.addAction(AccessibilityNodeInfo.ACTION_SET_SELECTION);
            node.addAction(AccessibilityNodeInfo.ACTION_CUT);
            node.addAction(AccessibilityNodeInfo.ACTION_COPY);
            node.addAction(AccessibilityNodeInfo.ACTION_PASTE);
            node.setEditable(true);
        }

        if (message.getBoolean("clickable")) {
            node.setClickable(true);
            node.addAction(AccessibilityNodeInfo.ACTION_CLICK);
        }

        if (Build.VERSION.SDK_INT >= 19 && message.containsKey("hint")) {
            Bundle bundle = node.getExtras();
            bundle.putCharSequence("AccessibilityNodeInfo.hint", message.getString("hint"));
        }
    }

    private void updateBounds(final AccessibilityNodeInfo node, final GeckoBundle message) {
        final GeckoBundle bounds = message.getBundle("bounds");
        if (bounds == null) {
            return;
        }

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

    private void updateState(final AccessibilityNodeInfo node, final GeckoBundle message) {
        if (message.containsKey("checked")) {
            node.setChecked(message.getBoolean("checked"));
        }
        if (message.containsKey("selected")) {
            node.setSelected(message.getBoolean("selected"));
        }
    }

    private void sendAccessibilityEvent(final GeckoBundle message) {
        if (mView == null || !Settings.isTouchExplorationEnabled())
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

        if (eventSource != View.NO_ID &&
                (eventType == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED ||
                 eventType == AccessibilityEvent.TYPE_VIEW_FOCUSED ||
                 eventType == AccessibilityEvent.TYPE_VIEW_HOVER_ENTER)) {
            // In Jelly Bean we populate an AccessibilityNodeInfo with the minimal amount of data to have
            // it work with TalkBack.
            if (mVirtualContentNode != null) {
                mVirtualContentNode.recycle();
            }
            mVirtualContentNode = AccessibilityNodeInfo.obtain(mView, eventSource);
            populateNodeInfoFromJSON(mVirtualContentNode, message);
        }

        if (mVirtualContentNode != null) {
            // Bounds for the virtual content can be updated from any event.
            updateBounds(mVirtualContentNode, message);

            // State for the virtual content can be updated when view is clicked/selected.
            if (eventType == AccessibilityEvent.TYPE_VIEW_CLICKED ||
                eventType == AccessibilityEvent.TYPE_VIEW_SELECTED) {
                updateState(mVirtualContentNode, message);
            }
        }

        final AccessibilityEvent accessibilityEvent = obtainEvent(eventType, eventSource);
        populateEventFromJSON(accessibilityEvent, message);
        ((ViewParent) mView).requestSendAccessibilityEvent(mView, accessibilityEvent);
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

    private int getAutoFillRootId(final int id) {
        int root = View.NO_ID;
        for (int newId = id; newId != View.NO_ID;) {
            root = newId;
            newId = mAutoFillNodes.get(newId).getInt("parent", View.NO_ID);
        }
        return root;
    }

    /* package */ AccessibilityNodeInfo getAutoFillNode(final int id) {
        if (mView == null || mAutoFillRoots == null) {
            return null;
        }

        final GeckoBundle bundle = mAutoFillNodes.get(id);
        if (bundle == null) {
            return null;
        }

        if (DEBUG) {
            Log.d(LOGTAG, "getAutoFillNode(" + id + ')');
        }

        final AccessibilityNodeInfo node = AccessibilityNodeInfo.obtain(mView, id);
        node.setPackageName(GeckoAppShell.getApplicationContext().getPackageName());
        node.setParent(mView, bundle.getInt("parent", View.NO_ID));
        node.setEnabled(true);

        if (getAutoFillRootId(mAutoFillFocusedId) == getAutoFillRootId(id)) {
            // Some auto-fill clients require a dummy rect for the focused View.
            final Rect rect = new Rect();
            mSession.getSurfaceBounds(rect);
            node.setVisibleToUser(!rect.isEmpty());
            node.setBoundsInParent(rect);

            final int[] offset = new int[2];
            mView.getLocationOnScreen(offset);
            rect.offset(offset[0], offset[1]);
            node.setBoundsInScreen(rect);
        }

        final GeckoBundle[] children = bundle.getBundleArray("children");
        if (children != null) {
            for (final GeckoBundle child : children) {
                final int childId = child.getInt("id");
                node.addChild(mView, childId);
                mAutoFillNodes.append(childId, child);
            }
        }

        String tag = bundle.getString("tag", "");
        final String type = bundle.getString("type", "text");
        final GeckoBundle attrs = bundle.getBundle("attributes");

        if ("INPUT".equals(tag) && !bundle.getBoolean("editable", false)) {
            tag = ""; // Don't process non-editable inputs (e.g. type="button").
        }
        switch (tag) {
            case "INPUT":
            case "TEXTAREA": {
                final boolean disabled = bundle.getBoolean("disabled");
                node.setClassName("android.widget.EditText");
                node.setEnabled(!disabled);
                node.setFocusable(!disabled);
                node.setFocused(id == mAutoFillFocusedId);

                if ("password".equals(type)) {
                    node.setPassword(true);
                }
                if (Build.VERSION.SDK_INT >= 18) {
                    node.setEditable(!disabled);
                }
                if (Build.VERSION.SDK_INT >= 19) {
                    node.setMultiLine("TEXTAREA".equals(tag));
                }
                if (Build.VERSION.SDK_INT >= 21) {
                    try {
                        node.setMaxTextLength(Integer.parseInt(
                                String.valueOf(attrs.get("maxlength"))));
                    } catch (final NumberFormatException ignore) {
                    }
                }

                if (!disabled) {
                    if (Build.VERSION.SDK_INT >= 21) {
                        node.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_SET_TEXT);
                    } else {
                        node.addAction(ACTION_SET_TEXT);
                    }
                }
                break;
            }
            default:
                if (children != null) {
                    node.setClassName("android.view.ViewGroup");
                } else {
                    node.setClassName("android.view.View");
                }
                break;
        }

        if (Build.VERSION.SDK_INT >= 19 && "INPUT".equals(tag)) {
            switch (type) {
                case "email":
                    node.setInputType(InputType.TYPE_CLASS_TEXT |
                                      InputType.TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS);
                    break;
                case "number":
                    node.setInputType(InputType.TYPE_CLASS_NUMBER);
                    break;
                case "password":
                    node.setInputType(InputType.TYPE_CLASS_TEXT |
                                      InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD);
                    break;
                case "tel":
                    node.setInputType(InputType.TYPE_CLASS_PHONE);
                    break;
                case "text":
                    node.setInputType(InputType.TYPE_CLASS_TEXT |
                                      InputType.TYPE_TEXT_VARIATION_WEB_EDIT_TEXT);
                    break;
                case "url":
                    node.setInputType(InputType.TYPE_CLASS_TEXT |
                                      InputType.TYPE_TEXT_VARIATION_URI);
                    break;
            }
        }
        return node;
    }

    /* package */ boolean performAutoFill(final int id, final String value) {
        if (mAutoFillRoots == null) {
            return false;
        }

        int rootId = id;
        for (int currentId = id; currentId != View.NO_ID;) {
            final GeckoBundle bundle = mAutoFillNodes.get(currentId);
            if (bundle == null) {
                return false;
            }
            rootId = currentId;
            currentId = bundle.getInt("parent", View.NO_ID);
        }

        if (DEBUG) {
            Log.d(LOGTAG, "performAutoFill(" + id + ", " + value + ')');
        }

        final EventCallback callback = mAutoFillRoots.get(rootId);
        if (callback == null) {
            return false;
        }

        final GeckoBundle response = new GeckoBundle(1);
        response.putString(String.valueOf(id), value);
        callback.sendSuccess(response);
        return true;
    }

    private void fireWindowChangedEvent(final int id) {
        if (Settings.isEnabled() && mView instanceof ViewParent) {
            final AccessibilityEvent event = obtainEvent(
                    AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED, id);
            if (Build.VERSION.SDK_INT >= 19) {
                event.setContentChangeTypes(AccessibilityEvent.CONTENT_CHANGE_TYPE_SUBTREE);
            }
            ((ViewParent) mView).requestSendAccessibilityEvent(mView, event);
        }
    }

    /* package */ void addAutoFill(final GeckoBundle message, final EventCallback callback) {
        if (!Settings.isEnabled()) {
            return;
        }

        if (mAutoFillRoots == null) {
            mAutoFillRoots = new SparseArray<>();
            mAutoFillNodes = new SparseArray<>();
        }

        final int id = message.getInt("id");
        if (DEBUG) {
            Log.d(LOGTAG, "addAutoFill(" + id + ')');
        }

        mAutoFillRoots.append(id, callback);
        mAutoFillNodes.append(id, message);
        fireWindowChangedEvent(id);
    }

    /* package */ void clearAutoFill() {
        if (mAutoFillRoots != null) {
            if (DEBUG) {
                Log.d(LOGTAG, "clearAutoFill()");
            }
            mAutoFillRoots = null;
            mAutoFillNodes = null;
            mAutoFillFocusedId = View.NO_ID;
            fireWindowChangedEvent(View.NO_ID);
        }
    }

    /* package */ void onAutoFillFocus(final GeckoBundle message) {
        if (!Settings.isEnabled() || !(mView instanceof ViewParent) || mAutoFillNodes == null) {
            return;
        }

        final int id;
        if (message != null) {
            id = message.getInt("id");
            mAutoFillNodes.put(id, message);
        } else {
            id = View.NO_ID;
        }

        if (DEBUG) {
            Log.d(LOGTAG, "onAutoFillFocus(" + id + ')');
        }
        if (mAutoFillFocusedId == id) {
            return;
        }
        mAutoFillFocusedId = id;

        // We already send "TYPE_VIEW_FOCUSED" in touch exploration mode,
        // so in that case don't send it here.
        if (!Settings.isTouchExplorationEnabled()) {
            AccessibilityEvent event = obtainEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED, id);
            ((ViewParent) mView).requestSendAccessibilityEvent(mView, event);
        }
    }

    /* package */ void onWindowFocus() {
        // Auto-fill clients expect a state change event on focus.
        if (Settings.isEnabled() && mView instanceof ViewParent) {
            if (DEBUG) {
                Log.d(LOGTAG, "onWindowFocus()");
            }
            final AccessibilityEvent event = obtainEvent(
                    AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED, View.NO_ID);
            ((ViewParent) mView).requestSendAccessibilityEvent(mView, event);
        }
    }

    /* package */ final class NativeProvider extends JNIObject {
        @WrapForJNI(calledFrom = "ui")
        private void setAttached(final boolean attached) {
            if (attached) {
                mAttached = true;
            } else if (mAttached) {
                mAttached = false;
                disposeNative();
            }
        }

        @WrapForJNI(calledFrom = "ui", dispatchTo = "gecko")
        @Override
        protected native void disposeNative();
    }
}
