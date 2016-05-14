/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.text;

import android.annotation.TargetApi;
import android.app.Activity;
import android.graphics.Rect;
import android.os.Build;
import android.util.Log;
import android.view.ActionMode;

import org.json.JSONException;
import org.json.JSONObject;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;

import java.util.List;

import ch.boye.httpclientandroidlib.util.TextUtils;

/**
 * Floating toolbar for text selection actions. Only on Android 6+.
 */
@TargetApi(Build.VERSION_CODES.M)
public class FloatingToolbarTextSelection implements TextSelection, GeckoEventListener {
    private static final String LOGTAG = "GeckoFloatTextSelection";

    private Activity activity;
    private ActionMode actionMode;
    private FloatingActionModeCallback actionModeCallback;
    private LayerView layerView;
    private int[] locationInWindow;

    private String selectionID;
    /* package-private */ Rect contentRect;

    public FloatingToolbarTextSelection(Activity activity, LayerView layerView) {
        this.activity = activity;
        this.layerView = layerView;
        this.locationInWindow = new int[2];
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
        if (TextUtils.isEmpty(selectionID)) {
            return;
        }

        final JSONObject args = new JSONObject();
        try {
            args.put("selectionID", selectionID);
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error building JSON arguments for TextSelection:End", e);
            return;
        }

        GeckoAppShell.notifyObservers("TextSelection:End", args.toString());
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
        EventDispatcher.getInstance().registerGeckoThreadListener(this,
                "TextSelection:ActionbarInit",
                "TextSelection:ActionbarStatus",
                "TextSelection:ActionbarUninit",
                "TextSelection:Update",
                "TextSelection:Visibility");
    }

    private void unregisterFromEvents() {
        EventDispatcher.getInstance().unregisterGeckoThreadListener(this,
                "TextSelection:ActionbarInit",
                "TextSelection:ActionbarStatus",
                "TextSelection:ActionbarUninit",
                "TextSelection:Update",
                "TextSelection:Visibility");
    }

    @Override
    public void handleMessage(final String event, final JSONObject message) {
        Log.w("SKDBG", "Received event " + event + " with message: " + message.toString());

        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                handleOnMainThread(event, message);
            }
        });
    }

    private void handleOnMainThread(final String event, final JSONObject message) {
        if ("TextSelection:ActionbarInit".equals(event)) {
            Telemetry.sendUIEvent(TelemetryContract.Event.SHOW,
                TelemetryContract.Method.CONTENT, "text_selection");

            selectionID = message.optString("selectionID");
        } else if ("TextSelection:ActionbarStatus".equals(event)) {
            // Ensure async updates from SearchService for example are valid.
            if (selectionID != message.optString("selectionID")) {
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
        } else if ("TextSelection:Update".equals(event)) {
            startActionMode(TextAction.fromEventMessage(message));
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

    private void updateRect(JSONObject message) {
        try {
            final double x = message.getDouble("x");
            final double y = (int) message.getDouble("y");
            final double width = (int) message.getDouble("width");
            final double height = (int) message.getDouble("height");

            final float zoomFactor = layerView.getZoomFactor();
            layerView.getLocationInWindow(locationInWindow);

            contentRect = new Rect(
                    (int) (x * zoomFactor + locationInWindow[0]),
                    (int) (y * zoomFactor + locationInWindow[1]  + layerView.getSurfaceTranslation()),
                    (int) ((x + width) * zoomFactor + locationInWindow[0]),
                    (int) ((y + height) * zoomFactor + locationInWindow[1] + layerView.getSurfaceTranslation()));
        } catch (JSONException e) {
            Log.w(LOGTAG, "Could not calculate content rect", e);
        }
    }
}
