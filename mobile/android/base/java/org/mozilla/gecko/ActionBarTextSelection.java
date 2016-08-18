/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.gfx.BitmapUtils.BitmapLoader;
import org.mozilla.gecko.menu.GeckoMenu;
import org.mozilla.gecko.menu.GeckoMenuItem;
import org.mozilla.gecko.text.TextSelection;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.ActionModeCompat.Callback;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;

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

class ActionBarTextSelection implements TextSelection, GeckoEventListener {
    private static final String LOGTAG = "GeckoTextSelection";
    private static final int SHUTDOWN_DELAY_MS = 250;

    private final TextSelectionHandle anchorHandle;

    private boolean mDraggingHandles;

    private String selectionID; // Unique ID provided for each selection action.

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

    ActionBarTextSelection(TextSelectionHandle anchorHandle) {
        this.anchorHandle = anchorHandle;
    }

    @Override
    public void create() {
        // Only register listeners if we have valid start/middle/end handles
        if (anchorHandle == null) {
            Log.e(LOGTAG, "Failed to initialize text selection because at least one handle is null");
        } else {
            EventDispatcher.getInstance().registerGeckoThreadListener(this,
                "TextSelection:ActionbarInit",
                "TextSelection:ActionbarStatus",
                "TextSelection:ActionbarUninit",
                "TextSelection:Update");
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
            "TextSelection:Update");
    }

    @Override
    public void handleMessage(final String event, final JSONObject message) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    if (event.equals("TextSelection:Update")) {
                        if (mActionModeTimerTask != null)
                            mActionModeTimerTask.cancel();
                        showActionMode(message.getJSONArray("actions"));
                    } else if (event.equals("TextSelection:ActionbarInit")) {
                        // Init / Open the action bar. Note the current selectionID,
                        // cancel any pending actionBar close.
                        Telemetry.sendUIEvent(TelemetryContract.Event.SHOW,
                            TelemetryContract.Method.CONTENT, "text_selection");

                        selectionID = message.getString("selectionID");
                        mCurrentItems = null;
                        if (mActionModeTimerTask != null) {
                            mActionModeTimerTask.cancel();
                        }

                    } else if (event.equals("TextSelection:ActionbarStatus")) {
                        // Ensure async updates from SearchService for example are valid.
                        if (selectionID != message.optString("selectionID")) {
                            return;
                        }

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
            mCallback.animateIn();
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

        public void animateIn() {
            if (mActionMode != null) {
                mActionMode.animateIn();
            }
        }

        @Override
        public boolean onPrepareActionMode(final ActionModeCompat mode, final GeckoMenu menu) {
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
        public boolean onCreateActionMode(ActionModeCompat mode, GeckoMenu unused) {
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
