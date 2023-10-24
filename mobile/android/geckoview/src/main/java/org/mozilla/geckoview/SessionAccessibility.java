/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.content.Context;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.text.InputType;
import android.text.TextUtils;
import android.util.Log;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewParent;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.CollectionInfo;
import android.view.accessibility.AccessibilityNodeInfo.CollectionItemInfo;
import android.view.accessibility.AccessibilityNodeInfo.RangeInfo;
import android.view.accessibility.AccessibilityNodeProvider;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

@UiThread
public class SessionAccessibility {
  private static final String LOGTAG = "GeckoAccessibility";

  // This is the number BrailleBack uses to start indexing routing keys.
  private static final int BRAILLE_CLICK_BASE_INDEX = -275000000;
  private static final String ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE =
      "ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE";

  @WrapForJNI static final int FLAG_ACCESSIBILITY_FOCUSED = 0;
  @WrapForJNI static final int FLAG_CHECKABLE = 1 << 1;
  @WrapForJNI static final int FLAG_CHECKED = 1 << 2;
  @WrapForJNI static final int FLAG_CLICKABLE = 1 << 3;
  @WrapForJNI static final int FLAG_CONTENT_INVALID = 1 << 4;
  @WrapForJNI static final int FLAG_CONTEXT_CLICKABLE = 1 << 5;
  @WrapForJNI static final int FLAG_EDITABLE = 1 << 6;
  @WrapForJNI static final int FLAG_ENABLED = 1 << 7;
  @WrapForJNI static final int FLAG_FOCUSABLE = 1 << 8;
  @WrapForJNI static final int FLAG_FOCUSED = 1 << 9;
  @WrapForJNI static final int FLAG_LONG_CLICKABLE = 1 << 10;
  @WrapForJNI static final int FLAG_MULTI_LINE = 1 << 11;
  @WrapForJNI static final int FLAG_PASSWORD = 1 << 12;
  @WrapForJNI static final int FLAG_SCROLLABLE = 1 << 13;
  @WrapForJNI static final int FLAG_SELECTED = 1 << 14;
  @WrapForJNI static final int FLAG_VISIBLE_TO_USER = 1 << 15;
  @WrapForJNI static final int FLAG_SELECTABLE = 1 << 16;
  @WrapForJNI static final int FLAG_EXPANDABLE = 1 << 17;
  @WrapForJNI static final int FLAG_EXPANDED = 1 << 18;

  static final int CLASSNAME_UNKNOWN = -1;
  @WrapForJNI static final int CLASSNAME_VIEW = 0;
  @WrapForJNI static final int CLASSNAME_BUTTON = 1;
  @WrapForJNI static final int CLASSNAME_CHECKBOX = 2;
  @WrapForJNI static final int CLASSNAME_DIALOG = 3;
  @WrapForJNI static final int CLASSNAME_EDITTEXT = 4;
  @WrapForJNI static final int CLASSNAME_GRIDVIEW = 5;
  @WrapForJNI static final int CLASSNAME_IMAGE = 6;
  @WrapForJNI static final int CLASSNAME_LISTVIEW = 7;
  @WrapForJNI static final int CLASSNAME_MENUITEM = 8;
  @WrapForJNI static final int CLASSNAME_PROGRESSBAR = 9;
  @WrapForJNI static final int CLASSNAME_RADIOBUTTON = 10;
  @WrapForJNI static final int CLASSNAME_SEEKBAR = 11;
  @WrapForJNI static final int CLASSNAME_SPINNER = 12;
  @WrapForJNI static final int CLASSNAME_TABWIDGET = 13;
  @WrapForJNI static final int CLASSNAME_TOGGLEBUTTON = 14;
  @WrapForJNI static final int CLASSNAME_WEBVIEW = 15;

  private static final String[] CLASSNAMES = {
    "android.view.View",
    "android.widget.Button",
    "android.widget.CheckBox",
    "android.app.Dialog",
    "android.widget.EditText",
    "android.widget.GridView",
    "android.widget.Image",
    "android.widget.ListView",
    "android.view.MenuItem",
    "android.widget.ProgressBar",
    "android.widget.RadioButton",
    "android.widget.SeekBar",
    "android.widget.Spinner",
    "android.widget.TabWidget",
    "android.widget.ToggleButton",
    "android.webkit.WebView"
  };

  @WrapForJNI static final int HTML_GRANULARITY_DEFAULT = -1;
  @WrapForJNI static final int HTML_GRANULARITY_ARTICLE = 0;
  @WrapForJNI static final int HTML_GRANULARITY_BUTTON = 1;
  @WrapForJNI static final int HTML_GRANULARITY_CHECKBOX = 2;
  @WrapForJNI static final int HTML_GRANULARITY_COMBOBOX = 3;
  @WrapForJNI static final int HTML_GRANULARITY_CONTROL = 4;
  @WrapForJNI static final int HTML_GRANULARITY_FOCUSABLE = 5;
  @WrapForJNI static final int HTML_GRANULARITY_FRAME = 6;
  @WrapForJNI static final int HTML_GRANULARITY_GRAPHIC = 7;
  @WrapForJNI static final int HTML_GRANULARITY_H1 = 8;
  @WrapForJNI static final int HTML_GRANULARITY_H2 = 9;
  @WrapForJNI static final int HTML_GRANULARITY_H3 = 10;
  @WrapForJNI static final int HTML_GRANULARITY_H4 = 11;
  @WrapForJNI static final int HTML_GRANULARITY_H5 = 12;
  @WrapForJNI static final int HTML_GRANULARITY_H6 = 13;
  @WrapForJNI static final int HTML_GRANULARITY_HEADING = 14;
  @WrapForJNI static final int HTML_GRANULARITY_LANDMARK = 15;
  @WrapForJNI static final int HTML_GRANULARITY_LINK = 16;
  @WrapForJNI static final int HTML_GRANULARITY_LIST = 17;
  @WrapForJNI static final int HTML_GRANULARITY_LIST_ITEM = 18;
  @WrapForJNI static final int HTML_GRANULARITY_MAIN = 19;
  @WrapForJNI static final int HTML_GRANULARITY_MEDIA = 20;
  @WrapForJNI static final int HTML_GRANULARITY_RADIO = 21;
  @WrapForJNI static final int HTML_GRANULARITY_SECTION = 22;
  @WrapForJNI static final int HTML_GRANULARITY_TABLE = 23;
  @WrapForJNI static final int HTML_GRANULARITY_TEXT_FIELD = 24;
  @WrapForJNI static final int HTML_GRANULARITY_UNVISITED_LINK = 25;
  @WrapForJNI static final int HTML_GRANULARITY_VISITED_LINK = 26;

  private static String[] sHtmlGranularities = {
    "ARTICLE",
    "BUTTON",
    "CHECKBOX",
    "COMBOBOX",
    "CONTROL",
    "FOCUSABLE",
    "FRAME",
    "GRAPHIC",
    "H1",
    "H2",
    "H3",
    "H4",
    "H5",
    "H6",
    "HEADING",
    "LANDMARK",
    "LINK",
    "LIST",
    "LIST_ITEM",
    "MAIN",
    "MEDIA",
    "RADIO",
    "SECTION",
    "TABLE",
    "TEXT_FIELD",
    "UNVISITED_LINK",
    "VISITED_LINK"
  };

  private static String getClassName(final int index) {
    if (index >= 0 && index < CLASSNAMES.length) {
      return CLASSNAMES[index];
    }

    Log.e(LOGTAG, "Index " + index + " our of CLASSNAME bounds.");
    return "android.view.View"; // Fallback class is View
  }

  /* package */ final class NodeProvider extends AccessibilityNodeProvider {
    @Override
    public AccessibilityNodeInfo createAccessibilityNodeInfo(final int virtualDescendantId) {
      AccessibilityNodeInfo node = null;
      if (mAttached) {
        node = getNodeFromGecko(virtualDescendantId);
      }

      if (node == null) {
        Log.w(
            LOGTAG,
            "Failed to retrieve accessible node virtualDescendantId="
                + virtualDescendantId
                + " mAttached="
                + mAttached);
        node = AccessibilityNodeInfo.obtain(mView, View.NO_ID);
        if (mView.getDisplay() != null) {
          // When running junit tests we don't have a display
          mView.onInitializeAccessibilityNodeInfo(node);
        }
        node.setClassName("android.webkit.WebView");
      }

      return node;
    }

    @Override
    public boolean performAction(
        final int virtualViewId, final int action, final Bundle arguments) {
      final GeckoBundle data;

      switch (action) {
        case AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS:
          sendEvent(
              AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED,
              virtualViewId,
              CLASSNAME_UNKNOWN,
              null);
          return true;
        case AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS:
          sendEvent(
              AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED,
              virtualViewId,
              virtualViewId == View.NO_ID ? CLASSNAME_WEBVIEW : CLASSNAME_UNKNOWN,
              null);
          return true;
        case AccessibilityNodeInfo.ACTION_CLICK:
        case AccessibilityNodeInfo.ACTION_EXPAND:
        case AccessibilityNodeInfo.ACTION_COLLAPSE:
          nativeProvider.click(virtualViewId);
          return true;
        case AccessibilityNodeInfo.ACTION_LONG_CLICK:
          // XXX: Implement long press.
          return true;
        case AccessibilityNodeInfo.ACTION_SCROLL_FORWARD:
          if (virtualViewId == View.NO_ID) {
            // Scroll the viewport forwards by approximately 80%.
            mSession
                .getPanZoomController()
                .scrollBy(
                    ScreenLength.zero(),
                    ScreenLength.fromVisualViewportHeight(0.8),
                    PanZoomController.SCROLL_BEHAVIOR_AUTO);
          } else {
            // XXX: It looks like we never call scroll on virtual views.
            // If we did, we should synthesize a wheel event on it's center coordinate.
          }
          return true;
        case AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD:
          if (virtualViewId == View.NO_ID) {
            // Scroll the viewport backwards by approximately 80%.
            mSession
                .getPanZoomController()
                .scrollBy(
                    ScreenLength.zero(),
                    ScreenLength.fromVisualViewportHeight(-0.8),
                    PanZoomController.SCROLL_BEHAVIOR_AUTO);
          } else {
            // XXX: It looks like we never call scroll on virtual views.
            // If we did, we should synthesize a wheel event on it's center coordinate.
          }
          return true;
        case AccessibilityNodeInfo.ACTION_SELECT:
          nativeProvider.click(virtualViewId);
          return true;
        case AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT:
          requestViewFocus();
          return pivot(
              virtualViewId,
              arguments != null
                  ? arguments.getString(AccessibilityNodeInfo.ACTION_ARGUMENT_HTML_ELEMENT_STRING)
                  : "",
              true,
              false);
        case AccessibilityNodeInfo.ACTION_PREVIOUS_HTML_ELEMENT:
          requestViewFocus();
          return pivot(
              virtualViewId,
              arguments != null
                  ? arguments.getString(AccessibilityNodeInfo.ACTION_ARGUMENT_HTML_ELEMENT_STRING)
                  : "",
              false,
              false);
        case AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY:
        case AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY:
          // XXX: Self brailling gives this action with a bogus argument instead of an actual click
          // action;
          // the argument value is the BRAILLE_CLICK_BASE_INDEX - the index of the routing key that
          // was hit.
          // Other negative values are used by ChromeVox, but we don't support them.
          // FAKE_GRANULARITY_READ_CURRENT = -1
          // FAKE_GRANULARITY_READ_TITLE = -2
          // FAKE_GRANULARITY_STOP_SPEECH = -3
          // FAKE_GRANULARITY_CHANGE_SHIFTER = -4
          if (arguments == null) {
            return false;
          }
          final int granularity =
              arguments.getInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT);
          if (granularity <= BRAILLE_CLICK_BASE_INDEX) {
            // XXX: Use click offset to update caret position in editables (BRAILLE_CLICK_BASE_INDEX
            // - granularity).
            nativeProvider.click(virtualViewId);
          } else if (granularity > 0) {
            final boolean extendSelection =
                arguments.getBoolean(
                    AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN);
            final boolean next =
                action == AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY;
            return nativeProvider.navigateText(
                virtualViewId, granularity, mStartOffset, mEndOffset, next, extendSelection);
          }
          return true;
        case AccessibilityNodeInfo.ACTION_SET_SELECTION:
          if (arguments == null) {
            return false;
          }
          final int selectionStart =
              arguments.getInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT);
          final int selectionEnd =
              arguments.getInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT);
          nativeProvider.setSelection(virtualViewId, selectionStart, selectionEnd);
          return true;
        case AccessibilityNodeInfo.ACTION_CUT:
          nativeProvider.cut(virtualViewId);
          return true;
        case AccessibilityNodeInfo.ACTION_COPY:
          nativeProvider.copy(virtualViewId);
          return true;
        case AccessibilityNodeInfo.ACTION_PASTE:
          nativeProvider.paste(virtualViewId);
          return true;
        case AccessibilityNodeInfo.ACTION_SET_TEXT:
          if (arguments == null) {
            return false;
          }
          final String value =
              arguments.getString(AccessibilityNodeInfo.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE);
          if (mAttached) {
            nativeProvider.setText(virtualViewId, value);
          }
          return true;
      }

      return mView.performAccessibilityAction(action, arguments);
    }

    @Override
    public AccessibilityNodeInfo findFocus(final int focus) {
      switch (focus) {
        case AccessibilityNodeInfo.FOCUS_ACCESSIBILITY:
          if (mAccessibilityFocusedNode != 0) {
            return createAccessibilityNodeInfo(mAccessibilityFocusedNode);
          }
          break;
        case AccessibilityNodeInfo.FOCUS_INPUT:
          if (mFocusedNode != 0) {
            return createAccessibilityNodeInfo(mFocusedNode);
          }
          break;
      }

      return super.findFocus(focus);
    }

    private AccessibilityNodeInfo getNodeFromGecko(final int virtualViewId) {
      ThreadUtils.assertOnUiThread();
      final AccessibilityNodeInfo node = AccessibilityNodeInfo.obtain(mView, virtualViewId);
      nativeProvider.getNodeInfo(virtualViewId, node);

      // We set the bounds in parent here because we need to use the client-to-screen matrix
      // and it is only available in the UI thread.
      final Rect bounds = new Rect();
      node.getBoundsInParent(bounds);

      final Matrix matrix = new Matrix();
      mSession.getClientToScreenMatrix(matrix);
      final float[] origin = new float[2];
      matrix.mapPoints(origin);
      bounds.offset((int) origin[0], (int) origin[1]);
      node.setBoundsInScreen(bounds);

      return node;
    }
  }

  // Gecko session we are proxying
  /* package */ final GeckoSession mSession;
  // This is the view that delegates accessibility to us. We also sends event through it.
  private View mView;
  // The native portion of the node provider.
  /* package */ final NativeProvider nativeProvider = new NativeProvider();
  private boolean mAttached = false;
  // The current node with accessibility focus
  private int mAccessibilityFocusedNode = 0;
  // The current node with focus
  private int mFocusedNode = 0;
  private int mStartOffset = -1;
  private int mEndOffset = -1;
  private boolean mViewFocusRequested = false;

  /* package */ SessionAccessibility(final GeckoSession session) {
    mSession = session;
    Settings.updateAccessibilitySettings();
  }

  /* package */ static void setForceEnabled(final boolean forceEnabled) {
    Settings.setForceEnabled(forceEnabled);
  }

  /**
   * Get the View instance that delegates accessibility to this session.
   *
   * @return View instance.
   */
  public @Nullable View getView() {
    ThreadUtils.assertOnUiThread();

    return mView;
  }

  /**
   * Set the View instance that should delegate accessibility to this session.
   *
   * @param view View instance.
   */
  @UiThread
  public void setView(final @Nullable View view) {
    ThreadUtils.assertOnUiThread();

    if (mView != null) {
      mView.setAccessibilityDelegate(null);
    }

    mView = view;

    if (mView == null) {
      return;
    }

    mView.setAccessibilityDelegate(
        new View.AccessibilityDelegate() {
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

          @Override
          public void sendAccessibilityEvent(final View host, final int eventType) {
            if (eventType == AccessibilityEvent.TYPE_VIEW_FOCUSED) {
              // We rely on the focus events sent from Gecko.
              return;
            }

            super.sendAccessibilityEvent(host, eventType);
          }
        });
  }

  private boolean isInTest() {
    return mView != null && mView.getDisplay() == null;
  }

  private void requestViewFocus() {
    if (!mView.isFocused() && !isInTest()) {
      mViewFocusRequested = true;
      mView.requestFocus();
    }
  }

  private static class Settings {
    private static volatile boolean sEnabled;
    private static volatile boolean sTouchExplorationEnabled;
    private static volatile boolean sForceEnabled;

    public static void setForceEnabled(final boolean forceEnabled) {
      sForceEnabled = forceEnabled;
      dispatch();
    }

    static {
      final Context context = GeckoAppShell.getApplicationContext();
      final AccessibilityManager accessibilityManager =
          (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);

      accessibilityManager.addAccessibilityStateChangeListener(
          enabled -> updateAccessibilitySettings());

      accessibilityManager.addTouchExplorationStateChangeListener(
          enabled -> updateAccessibilitySettings());
    }

    public static boolean isEnabled() {
      return sEnabled || sForceEnabled;
    }

    public static boolean isTouchExplorationEnabled() {
      return sTouchExplorationEnabled || sForceEnabled;
    }

    public static void updateAccessibilitySettings() {
      final AccessibilityManager accessibilityManager =
          (AccessibilityManager)
              GeckoAppShell.getApplicationContext().getSystemService(Context.ACCESSIBILITY_SERVICE);
      sEnabled = accessibilityManager.isEnabled();
      sTouchExplorationEnabled = sEnabled && accessibilityManager.isTouchExplorationEnabled();
      dispatch();
    }

    /* package */ static void dispatch() {
      if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
        toggleNativeAccessibility(isEnabled());
      } else {
        GeckoThread.queueNativeCallUntil(
            GeckoThread.State.PROFILE_READY,
            Settings.class,
            "toggleNativeAccessibility",
            isEnabled());
      }
    }

    @WrapForJNI(dispatchTo = "gecko")
    private static native void toggleNativeAccessibility(boolean enable);
  }

  @SuppressWarnings("checkstyle:javadocmethod")
  public boolean onMotionEvent(final @NonNull MotionEvent event) {
    ThreadUtils.assertOnUiThread();

    if (!Settings.isTouchExplorationEnabled()) {
      return false;
    }

    if (event.getSource() != InputDevice.SOURCE_TOUCHSCREEN) {
      return false;
    }

    final int action = event.getActionMasked();
    if ((action != MotionEvent.ACTION_HOVER_MOVE)
        && (action != MotionEvent.ACTION_HOVER_ENTER)
        && (action != MotionEvent.ACTION_HOVER_EXIT)) {
      return false;
    }

    requestViewFocus();

    nativeProvider.exploreByTouch(
        mAccessibilityFocusedNode != 0 ? mAccessibilityFocusedNode : View.NO_ID,
        event.getX(),
        event.getY());

    return true;
  }

  /* package */ void sendEvent(
      final int eventType, final int sourceId, final int className, final GeckoBundle eventData) {
    ThreadUtils.assertOnUiThread();
    if (mView == null || !mAttached) {
      return;
    }

    if (mViewFocusRequested && className == CLASSNAME_WEBVIEW) {
      // If the view was focused from an accessiblity action or
      // explore-by-touch, we supress this focus event to avoid noise.
      mViewFocusRequested = false;
      return;
    }

    final AccessibilityEvent event = AccessibilityEvent.obtain(eventType);
    event.setPackageName(GeckoAppShell.getApplicationContext().getPackageName());
    event.setSource(mView, sourceId);
    event.setEnabled(true);

    int eventClassName = className;
    if (eventClassName == CLASSNAME_UNKNOWN) {
      eventClassName = nativeProvider.getNodeClassName(sourceId);
    }
    event.setClassName(getClassName(eventClassName));

    if (eventData != null) {
      if (eventData.containsKey("text")) {
        event.getText().add(eventData.getString("text"));
      }
      event.setContentDescription(eventData.getString("description", ""));
      event.setAddedCount(eventData.getInt("addedCount", -1));
      event.setRemovedCount(eventData.getInt("removedCount", -1));
      event.setFromIndex(eventData.getInt("fromIndex", -1));
      event.setItemCount(eventData.getInt("itemCount", -1));
      event.setCurrentItemIndex(eventData.getInt("currentItemIndex", -1));
      event.setBeforeText(eventData.getString("beforeText", ""));
      event.setToIndex(eventData.getInt("toIndex", -1));
      event.setScrollX(eventData.getInt("scrollX", -1));
      event.setScrollY(eventData.getInt("scrollY", -1));
      event.setMaxScrollX(eventData.getInt("maxScrollX", -1));
      event.setMaxScrollY(eventData.getInt("maxScrollY", -1));
      event.setChecked((eventData.getInt("flags") & FLAG_CHECKED) != 0);
    }

    // Update stored state from this event.
    switch (eventType) {
      case AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED:
        if (mAccessibilityFocusedNode == sourceId) {
          mAccessibilityFocusedNode = 0;
        }
        break;
      case AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED:
        mStartOffset = -1;
        mEndOffset = -1;
        mAccessibilityFocusedNode = sourceId;
        break;
      case AccessibilityEvent.TYPE_VIEW_FOCUSED:
        mFocusedNode = sourceId;
        if (!mView.isFocused() && !isInTest()) {
          // Don't dispatch a focus event if the parent view is not focused
          return;
        }
        break;
      case AccessibilityEvent.TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY:
        mStartOffset = event.getFromIndex();
        mEndOffset = event.getToIndex();
        break;
    }

    try {
      ((ViewParent) mView).requestSendAccessibilityEvent(mView, event);
    } catch (final IllegalStateException ex) {
      // Accessibility could be activated in Gecko via xpcom, for example when using a11y
      // devtools. Events that are forwarded to the platform will throw an exception.
    }
  }

  private boolean pivot(
      final int id, final String granularity, final boolean forward, final boolean inclusive) {
    if (!forward && id == View.NO_ID) {
      // If attempting to pivot backwards from the root view, return false.
      return false;
    }

    final int gran = java.util.Arrays.asList(sHtmlGranularities).indexOf(granularity);
    final boolean success = nativeProvider.pivotNative(id, gran, forward, inclusive);
    if (!success && !forward) {
      // If we failed to pivot backwards set the root view as the a11y focus.
      sendEvent(
          AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED, View.NO_ID, CLASSNAME_WEBVIEW, null);
      return true;
    }

    return success;
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

    @WrapForJNI(dispatchTo = "current")
    public native void getNodeInfo(int id, AccessibilityNodeInfo nodeInfo);

    @WrapForJNI(dispatchTo = "current")
    public native int getNodeClassName(int id);

    @WrapForJNI(dispatchTo = "gecko")
    public native void setText(int id, String text);

    @WrapForJNI(dispatchTo = "gecko")
    public native void click(int id);

    @WrapForJNI(dispatchTo = "current", stubName = "Pivot")
    public native boolean pivotNative(int id, int granularity, boolean forward, boolean inclusive);

    @WrapForJNI(dispatchTo = "gecko")
    public native void exploreByTouch(int id, float x, float y);

    @WrapForJNI(dispatchTo = "current")
    public native boolean navigateText(
        int id, int granularity, int startOffset, int endOffset, boolean forward, boolean select);

    @WrapForJNI(dispatchTo = "gecko")
    public native void setSelection(int id, int start, int end);

    @WrapForJNI(dispatchTo = "gecko")
    public native void cut(int id);

    @WrapForJNI(dispatchTo = "gecko")
    public native void copy(int id);

    @WrapForJNI(dispatchTo = "gecko")
    public native void paste(int id);

    @WrapForJNI(calledFrom = "gecko", stubName = "SendEvent")
    private void sendEventNative(
        final int eventType, final int sourceId, final int className, final GeckoBundle eventData) {
      ThreadUtils.runOnUiThread(
          new Runnable() {
            @Override
            public void run() {
              sendEvent(eventType, sourceId, className, eventData);
            }
          });
    }

    @WrapForJNI
    private void populateNodeInfo(
        final AccessibilityNodeInfo node,
        final int id,
        final int parentId,
        final int[] children,
        final int flags,
        final int className,
        final int[] bounds,
        @Nullable final String text,
        @Nullable final String description,
        @Nullable final String hint,
        @Nullable final String geckoRole,
        @Nullable final String roleDescription,
        @Nullable final String viewIdResourceName,
        final int inputType) {
      if (mView == null) {
        return;
      }

      final boolean isRoot = id == View.NO_ID;
      if (isRoot) {
        if (mView.getDisplay() != null) {
          // When running junit tests we don't have a display
          mView.onInitializeAccessibilityNodeInfo(node);
        }
        node.addAction(AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD);
        node.addAction(AccessibilityNodeInfo.ACTION_SCROLL_FORWARD);
      } else {
        node.setParent(mView, parentId);
      }

      // The basics
      node.setPackageName(GeckoAppShell.getApplicationContext().getPackageName());
      node.setClassName(getClassName(className));

      if (text != null) {
        node.setText(text);
      }

      if (description != null) {
        node.setContentDescription(description);
      }

      // Add actions
      node.addAction(AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT);
      node.addAction(AccessibilityNodeInfo.ACTION_PREVIOUS_HTML_ELEMENT);
      node.addAction(AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY);
      node.addAction(AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY);
      node.setMovementGranularities(
          AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER
              | AccessibilityNodeInfo.MOVEMENT_GRANULARITY_WORD
              | AccessibilityNodeInfo.MOVEMENT_GRANULARITY_LINE
              | AccessibilityNodeInfo.MOVEMENT_GRANULARITY_PARAGRAPH);
      if ((flags & FLAG_CLICKABLE) != 0) {
        node.addAction(AccessibilityNodeInfo.ACTION_CLICK);
      }

      // Set boolean properties
      node.setCheckable((flags & FLAG_CHECKABLE) != 0);
      node.setChecked((flags & FLAG_CHECKED) != 0);
      node.setClickable((flags & FLAG_CLICKABLE) != 0);
      node.setEnabled((flags & FLAG_ENABLED) != 0);
      node.setFocusable((flags & FLAG_FOCUSABLE) != 0);
      node.setLongClickable((flags & FLAG_LONG_CLICKABLE) != 0);
      node.setPassword((flags & FLAG_PASSWORD) != 0);
      node.setScrollable((flags & FLAG_SCROLLABLE) != 0);
      node.setSelected((flags & FLAG_SELECTED) != 0);
      node.setVisibleToUser((flags & FLAG_VISIBLE_TO_USER) != 0);
      // Other boolean properties to consider later:
      // setHeading, setImportantForAccessibility, setScreenReaderFocusable, setShowingHintText,
      // setDismissable

      if (mAccessibilityFocusedNode == id) {
        node.addAction(AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS);
        node.setAccessibilityFocused(true);
      } else {
        node.addAction(AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS);
      }
      node.setFocused(mFocusedNode == id);

      final Rect parentBounds = new Rect(bounds[0], bounds[1], bounds[2], bounds[3]);
      node.setBoundsInParent(parentBounds);

      for (final int childId : children) {
        node.addChild(mView, childId);
      }

      node.setViewIdResourceName(viewIdResourceName);

      if ((flags & FLAG_EDITABLE) != 0) {
        node.addAction(AccessibilityNodeInfo.ACTION_SET_SELECTION);
        node.addAction(AccessibilityNodeInfo.ACTION_CUT);
        node.addAction(AccessibilityNodeInfo.ACTION_COPY);
        node.addAction(AccessibilityNodeInfo.ACTION_PASTE);
        node.setEditable(true);
      }

      node.setMultiLine((flags & FLAG_MULTI_LINE) != 0);
      node.setContentInvalid((flags & FLAG_CONTENT_INVALID) != 0);

      // Set bundle keys like role and hint
      final Bundle bundle = node.getExtras();
      if (hint != null) {
        bundle.putCharSequence("AccessibilityNodeInfo.hint", hint);
        if (Build.VERSION.SDK_INT >= 26) {
          node.setHintText(hint);
        }
      }
      if (geckoRole != null) {
        bundle.putCharSequence("AccessibilityNodeInfo.geckoRole", geckoRole);
      }
      if (roleDescription != null) {
        bundle.putCharSequence("AccessibilityNodeInfo.roleDescription", roleDescription);
      }
      if (isRoot) {
        // Argument values for ACTION_NEXT_HTML_ELEMENT/ACTION_PREVIOUS_HTML_ELEMENT.
        // This is mostly here to let TalkBack know we are a legit "WebView".
        bundle.putCharSequence(
            "ACTION_ARGUMENT_HTML_ELEMENT_STRING_VALUES", TextUtils.join(",", sHtmlGranularities));
      }

      if (inputType != InputType.TYPE_NULL) {
        node.setInputType(inputType);
      }

      if ((flags & FLAG_EXPANDABLE) != 0) {
        if ((flags & FLAG_EXPANDED) != 0) {
          node.removeAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_EXPAND);
          node.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_COLLAPSE);
        } else {
          node.removeAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_COLLAPSE);
          node.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_EXPAND);
        }
      }

      // SDK 23 and above
      if (Build.VERSION.SDK_INT >= 23) {
        node.setContextClickable((flags & FLAG_CONTEXT_CLICKABLE) != 0);
      }
    }

    @WrapForJNI
    private void populateNodeCollectionItemInfo(
        final AccessibilityNodeInfo node,
        final int rowIndex,
        final int rowSpan,
        final int columnIndex,
        final int columnSpan) {
      final CollectionItemInfo collectionItemInfo =
          CollectionItemInfo.obtain(rowIndex, rowSpan, columnIndex, columnSpan, false);
      node.setCollectionItemInfo(collectionItemInfo);
    }

    @WrapForJNI
    private void populateNodeCollectionInfo(
        final AccessibilityNodeInfo node,
        final int rowCount,
        final int columnCount,
        final int selectionMode,
        final boolean isHierarchical) {
      final CollectionInfo collectionInfo =
          CollectionInfo.obtain(rowCount, columnCount, isHierarchical, selectionMode);
      node.setCollectionInfo(collectionInfo);
    }

    @WrapForJNI
    private void populateNodeRangeInfo(
        final AccessibilityNodeInfo node,
        final int rangeType,
        final float min,
        final float max,
        final float current) {
      final RangeInfo rangeInfo = RangeInfo.obtain(rangeType, min, max, current);
      node.setRangeInfo(rangeInfo);
    }
  }
}
