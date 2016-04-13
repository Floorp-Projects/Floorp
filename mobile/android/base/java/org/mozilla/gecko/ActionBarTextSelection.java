/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.gfx.BitmapUtils.BitmapLoader;
import org.mozilla.gecko.gfx.ImmutableViewportMetrics;
import org.mozilla.gecko.gfx.Layer;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.gfx.LayerView.DrawListener;
import org.mozilla.gecko.menu.GeckoMenuItem;
import org.mozilla.gecko.text.TextSelection;
import org.mozilla.gecko.util.FloatUtils;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.ActionModeCompat.Callback;
import org.mozilla.gecko.AppConstants.Versions;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.Menu;
import android.view.MenuItem;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.Timer;
import java.util.TimerTask;

import android.util.Log;
import android.view.View;

class ActionBarTextSelection extends Layer implements TextSelection, GeckoEventListener, LayerView.DynamicToolbarListener {
    private static final String LOGTAG = "GeckoTextSelection";
    private static final int SHUTDOWN_DELAY_MS = 250;

    private final TextSelectionHandle anchorHandle;
    private final TextSelectionHandle caretHandle;
    private final TextSelectionHandle focusHandle;

    private final DrawListener mDrawListener;
    private boolean mDraggingHandles;

    private String selectionID; // Unique ID provided for each selection action.
    private float mViewLeft;
    private float mViewTop;
    private float mViewZoom;
    private boolean mForceReposition;

    private String mCurrentItems;

    private TextSelectionActionModeCallback mCallback;

    // These timers are used to avoid flicker caused by selection handles showing/hiding quickly.
    // For instance when moving between single handle caret mode and two handle selection mode.
    private final Timer mActionModeTimer = new Timer("actionMode");
    private class ActionModeTimerTask extends TimerTask {
        @Override
        public void run() {
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    endActionMode();
                }
            });
        }
    };
    private ActionModeTimerTask mActionModeTimerTask;

    ActionBarTextSelection(TextSelectionHandle anchorHandle,
                           TextSelectionHandle caretHandle,
                           TextSelectionHandle focusHandle) {
        this.anchorHandle = anchorHandle;
        this.caretHandle = caretHandle;
        this.focusHandle = focusHandle;

        mDrawListener = new DrawListener() {
            @Override
            public void drawFinished() {
                if (!mDraggingHandles) {
                    GeckoAppShell.notifyObservers("TextSelection:LayerReflow", "");
                }
            }
        };
    }

    @Override
    public void create() {
        // Only register listeners if we have valid start/middle/end handles
        if (anchorHandle == null || caretHandle == null || focusHandle == null) {
            Log.e(LOGTAG, "Failed to initialize text selection because at least one handle is null");
        } else {
            EventDispatcher.getInstance().registerGeckoThreadListener(this,
                "TextSelection:ActionbarInit",
                "TextSelection:ActionbarStatus",
                "TextSelection:ActionbarUninit",
                "TextSelection:ShowHandles",
                "TextSelection:HideHandles",
                "TextSelection:PositionHandles",
                "TextSelection:Update",
                "TextSelection:DraggingHandle");
        }
    }

    @Override
    public boolean dismiss() {
        // We do not call endActionMode() here because this is already handled by the activity.
        return false;
    }

    @Override
    public void destroy() {
        EventDispatcher.getInstance().unregisterGeckoThreadListener(this,
            "TextSelection:ActionbarInit",
            "TextSelection:ActionbarStatus",
            "TextSelection:ActionbarUninit",
            "TextSelection:ShowHandles",
            "TextSelection:HideHandles",
            "TextSelection:PositionHandles",
            "TextSelection:Update",
            "TextSelection:DraggingHandle");
    }

    private TextSelectionHandle getHandle(String name) {
        switch (TextSelectionHandle.HandleType.valueOf(name)) {
            case ANCHOR:
                return anchorHandle;
            case CARET:
                return caretHandle;
            case FOCUS:
                return focusHandle;

            default:
                throw new IllegalArgumentException("TextSelectionHandle is invalid type.");
        }
    }

    @Override
    public void handleMessage(final String event, final JSONObject message) {
        if ("TextSelection:DraggingHandle".equals(event)) {
            mDraggingHandles = message.optBoolean("dragging", false);
            return;
        }

        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    if (event.equals("TextSelection:ShowHandles")) {
                        selectionID = message.getString("selectionID");
                        final JSONArray handles = message.getJSONArray("handles");
                        for (int i = 0; i < handles.length(); i++) {
                            String handle = handles.getString(i);
                            getHandle(handle).setVisibility(View.VISIBLE);
                        }

                        mViewLeft = 0.0f;
                        mViewTop = 0.0f;
                        mViewZoom = 0.0f;

                        // Create text selection layer and add draw-listener for positioning on reflows
                        LayerView layerView = GeckoAppShell.getLayerView();
                        if (layerView != null) {
                            layerView.addDrawListener(mDrawListener);
                            layerView.addLayer(ActionBarTextSelection.this);
                            layerView.getDynamicToolbarAnimator().addTranslationListener(ActionBarTextSelection.this);
                        }

                        if (handles.length() > 1)
                            GeckoAppShell.performHapticFeedback(true);
                    } else if (event.equals("TextSelection:Update")) {
                        if (mActionModeTimerTask != null)
                            mActionModeTimerTask.cancel();
                        showActionMode(message.getJSONArray("actions"));
                    } else if (event.equals("TextSelection:HideHandles")) {
                        // Remove draw-listener and text selection layer
                        LayerView layerView = GeckoAppShell.getLayerView();
                        if (layerView != null) {
                            layerView.removeDrawListener(mDrawListener);
                            layerView.removeLayer(ActionBarTextSelection.this);
                            layerView.getDynamicToolbarAnimator().removeTranslationListener(ActionBarTextSelection.this);
                        }

                        mActionModeTimerTask = new ActionModeTimerTask();
                        mActionModeTimer.schedule(mActionModeTimerTask, SHUTDOWN_DELAY_MS);

                        anchorHandle.setVisibility(View.GONE);
                        caretHandle.setVisibility(View.GONE);
                        focusHandle.setVisibility(View.GONE);

                    } else if (event.equals("TextSelection:PositionHandles")) {
                        final JSONArray positions = message.getJSONArray("positions");
                        for (int i = 0; i < positions.length(); i++) {
                            JSONObject position = positions.getJSONObject(i);
                            final int left = position.getInt("left");
                            final int top = position.getInt("top");
                            final boolean rtl = position.getBoolean("rtl");

                            TextSelectionHandle handle = getHandle(position.getString("handle"));
                            handle.setVisibility(position.getBoolean("hidden") ? View.GONE : View.VISIBLE);
                            handle.positionFromGecko(left, top, rtl);
                        }

                    } else if (event.equals("TextSelection:ActionbarInit")) {
                        // Init / Open the action bar. Note the current selectionID,
                        // cancel any pending actionBar close.
                        selectionID = message.getString("selectionID");
                        mCurrentItems = null;
                        if (mActionModeTimerTask != null) {
                            mActionModeTimerTask.cancel();
                        }

                    } else if (event.equals("TextSelection:ActionbarStatus")) {
                        // Update the actionBar actions as provided by Gecko.
                        showActionMode(message.getJSONArray("actions"));

                    } else if (event.equals("TextSelection:ActionbarUninit")) {
                        // Uninit the actionbar. Schedule a cancellable close
                        // action to avoid UI jank. (During SelectionAll for ex).
                        mCurrentItems = null;
                        mActionModeTimerTask = new ActionModeTimerTask();
                        mActionModeTimer.schedule(mActionModeTimerTask, SHUTDOWN_DELAY_MS);
                    }

                } catch (JSONException e) {
                    Log.e(LOGTAG, "JSON exception", e);
                }
            }
        });
    }

    private void showActionMode(final JSONArray items) {
        String itemsString = items.toString();
        if (itemsString.equals(mCurrentItems)) {
            return;
        }
        mCurrentItems = itemsString;

        if (mCallback != null) {
            mCallback.updateItems(items);
            return;
        }

        final Context context = anchorHandle.getContext();
        if (context instanceof ActionModeCompat.Presenter) {
            final ActionModeCompat.Presenter presenter = (ActionModeCompat.Presenter) context;
            mCallback = new TextSelectionActionModeCallback(items);
            presenter.startActionModeCompat(mCallback);
        }
    }

    private void endActionMode() {
        Context context = anchorHandle.getContext();
        if (context instanceof ActionModeCompat.Presenter) {
            final ActionModeCompat.Presenter presenter = (ActionModeCompat.Presenter) context;
            presenter.endActionModeCompat();
        }
        mCurrentItems = null;
    }

    @Override
    public void draw(final RenderContext context) {
        // cache the relevant values from the context and bail out if they are the same. we do this
        // because this draw function gets called a lot (once per compositor frame) and we want to
        // avoid doing a lot of extra work in cases where it's not needed.
        final float viewLeft = context.viewport.left;
        final float viewTop = context.viewport.top;
        final float viewZoom = context.zoomFactor;

        if (!mForceReposition
            && FloatUtils.fuzzyEquals(mViewLeft, viewLeft)
            && FloatUtils.fuzzyEquals(mViewTop, viewTop)
            && FloatUtils.fuzzyEquals(mViewZoom, viewZoom)) {
            return;
        }
        mForceReposition = false;
        mViewLeft = viewLeft;
        mViewTop = viewTop;
        mViewZoom = viewZoom;

        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                anchorHandle.repositionWithViewport(viewLeft, viewTop, viewZoom);
                caretHandle.repositionWithViewport(viewLeft, viewTop, viewZoom);
                focusHandle.repositionWithViewport(viewLeft, viewTop, viewZoom);
            }
        });
    }

    @Override
    public void onTranslationChanged(float aToolbarTranslation, float aLayerViewTranslation) {
        mForceReposition = true;
    }

    @Override
    public void onPanZoomStopped() {
        // no-op
    }

    @Override
    public void onMetricsChanged(ImmutableViewportMetrics viewport) {
        mForceReposition = true;
    }

    private class TextSelectionActionModeCallback implements Callback {
        private JSONArray mItems;
        private ActionModeCompat mActionMode;

        public TextSelectionActionModeCallback(JSONArray items) {
            mItems = items;
        }

        public void updateItems(JSONArray items) {
            mItems = items;
            if (mActionMode != null) {
                mActionMode.invalidate();
            }
        }

        @Override
        public boolean onPrepareActionMode(final ActionModeCompat mode, final Menu menu) {
            // Android would normally expect us to only update the state of menu items here
            // To make the js-java interaction a bit simpler, we just wipe out the menu here and recreate all
            // the javascript menu items in onPrepare instead. This will be called any time invalidate() is called on the
            // action mode.
            menu.clear();

            int length = mItems.length();
            for (int i = 0; i < length; i++) {
                try {
                    final JSONObject obj = mItems.getJSONObject(i);
                    final GeckoMenuItem menuitem = (GeckoMenuItem) menu.add(0, i, 0, obj.optString("label"));
                    final int actionEnum = obj.optBoolean("showAsAction") ? GeckoMenuItem.SHOW_AS_ACTION_ALWAYS : GeckoMenuItem.SHOW_AS_ACTION_NEVER;
                    menuitem.setShowAsAction(actionEnum, R.attr.menuItemActionModeStyle);

                    final String iconString = obj.optString("icon");
                    BitmapUtils.getDrawable(anchorHandle.getContext(), iconString, new BitmapLoader() {
                        @Override
                        public void onBitmapFound(Drawable d) {
                            if (d != null) {
                                menuitem.setIcon(d);
                            }
                        }
                    });
                } catch (Exception ex) {
                    Log.i(LOGTAG, "Exception building menu", ex);
                }
            }
            return true;
        }

        @Override
        public boolean onCreateActionMode(ActionModeCompat mode, Menu menu) {
            mActionMode = mode;
            return true;
        }

        @Override
        public boolean onActionItemClicked(ActionModeCompat mode, MenuItem item) {
            try {
                final JSONObject obj = mItems.getJSONObject(item.getItemId());
                GeckoAppShell.notifyObservers("TextSelection:Action", obj.optString("id"));
                return true;
            } catch (Exception ex) {
                Log.i(LOGTAG, "Exception calling action", ex);
            }
            return false;
        }

        // Called when the user exits the action mode
        @Override
        public void onDestroyActionMode(ActionModeCompat mode) {
            mActionMode = null;
            mCallback = null;
            final JSONObject args = new JSONObject();
            try {
                args.put("selectionID", selectionID);
            } catch (JSONException e) {
                Log.e(LOGTAG, "Error building JSON arguments for TextSelection:End", e);
                return;
            }

            GeckoAppShell.notifyObservers("TextSelection:End", args.toString());
        }
    }
}
