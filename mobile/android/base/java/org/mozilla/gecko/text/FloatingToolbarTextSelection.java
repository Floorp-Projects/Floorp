/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.text;

import android.annotation.TargetApi;
import android.app.Activity;
import android.graphics.Rect;
import android.os.Build;
import android.util.Log;
import android.util.TypedValue;
import android.view.ActionMode;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

import java.util.List;

/**
 * Floating toolbar for text selection actions. Only on Android 6+.
 */
@TargetApi(Build.VERSION_CODES.M)
public class FloatingToolbarTextSelection implements TextSelection, BundleEventListener {
    private static final String LOGTAG = "GeckoFloatTextSelection";

    // This is an additional offset we add to the height of the selection. This will avoid that the
    // floating toolbar overlays the bottom handle(s).
    private static final int HANDLES_OFFSET_DP = 20;

    private final Activity activity;
    private final LayerView layerView;
    private final int[] locationInWindow;
    private final float handlesOffset;

    private ActionMode actionMode;
    private FloatingActionModeCallback actionModeCallback;
    private int selectionID;
    /* package-private */ Rect contentRect;

    public FloatingToolbarTextSelection(Activity activity, LayerView layerView) {
        this.activity = activity;
        this.layerView = layerView;
        this.locationInWindow = new int[2];

        this.handlesOffset = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                HANDLES_OFFSET_DP, activity.getResources().getDisplayMetrics());
    }

    @Override
    public boolean dismiss() {
        if (finishActionMode()) {
            endTextSelection();
            return true;
        }

        return false;
    }

    private void endTextSelection() {
        if (selectionID == 0) {
            return;
        }

        final GeckoBundle data = new GeckoBundle(1);
        data.putInt("selectionID", selectionID);
        GeckoApp.getEventDispatcher().dispatch("TextSelection:End", data);
    }

    @Override
    public void create() {
        registerForEvents();
    }

    @Override
    public void destroy() {
        unregisterFromEvents();
    }

    private void registerForEvents() {
        GeckoApp.getEventDispatcher().registerUiThreadListener(this,
                "TextSelection:ActionbarInit",
                "TextSelection:ActionbarStatus",
                "TextSelection:ActionbarUninit",
                "TextSelection:Visibility");
    }

    private void unregisterFromEvents() {
        GeckoApp.getEventDispatcher().unregisterUiThreadListener(this,
                "TextSelection:ActionbarInit",
                "TextSelection:ActionbarStatus",
                "TextSelection:ActionbarUninit",
                "TextSelection:Visibility");
    }

    @Override // BundleEventListener
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if ("TextSelection:ActionbarInit".equals(event)) {
            Telemetry.sendUIEvent(TelemetryContract.Event.SHOW,
                TelemetryContract.Method.CONTENT, "text_selection");

            selectionID = message.getInt("selectionID");

        } else if ("TextSelection:ActionbarStatus".equals(event)) {
            // Ensure async updates from SearchService for example are valid.
            if (selectionID != message.getInt("selectionID")) {
                return;
            }

            updateRect(message);

            if (!isRectVisible()) {
                finishActionMode();
            } else {
                startActionMode(TextAction.fromEventMessage(message));
            }

        } else if ("TextSelection:ActionbarUninit".equals(event)) {
            finishActionMode();

        } else if ("TextSelection:Visibility".equals(event)) {
            finishActionMode();
        }
    }

    private void startActionMode(List<TextAction> actions) {
        if (actionMode != null) {
            actionModeCallback.updateActions(actions);
            actionMode.invalidate();
            return;
        }

        actionModeCallback = new FloatingActionModeCallback(this, actions);
        actionMode = activity.startActionMode(actionModeCallback, ActionMode.TYPE_FLOATING);
    }

    private boolean finishActionMode() {
        if (actionMode != null) {
            actionMode.finish();
            actionMode = null;
            actionModeCallback = null;
            return true;
        }

        return false;
    }

    /**
     * If the content rect is a point (left == right and top == bottom) then this means that the
     * content rect is not in the currently visible part.
     */
    private boolean isRectVisible() {
        // There's another case of an empty rect where just left == right but not top == bottom.
        // That's the rect for a collapsed selection. While technically this rect isn't visible too
        // we are not interested in this case because we do not want to hide the toolbar.
        return contentRect.left != contentRect.right || contentRect.top != contentRect.bottom;
    }

    private void updateRect(final GeckoBundle message) {
        final double x = message.getDouble("x");
        final double y = (int) message.getDouble("y");
        final double width = (int) message.getDouble("width");
        final double height = (int) message.getDouble("height");

        final float toolbarOffset = layerView.getCurrentToolbarHeight();
        final float zoomFactor = layerView.getZoomFactor();
        layerView.getLocationInWindow(locationInWindow);

        contentRect = new Rect(
                (int) (x * zoomFactor + locationInWindow[0]),
                (int) (y * zoomFactor + locationInWindow[1] + toolbarOffset),
                (int) ((x + width) * zoomFactor + locationInWindow[0]),
                (int) ((y + height) * zoomFactor + locationInWindow[1] +
                       (height > 0 ? handlesOffset : 0)));
    }
}
