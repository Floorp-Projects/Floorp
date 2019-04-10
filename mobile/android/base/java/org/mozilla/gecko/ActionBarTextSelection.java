/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.ResourceDrawableUtils;
import org.mozilla.gecko.text.TextSelection;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.widget.ActionModePresenter;
import org.mozilla.geckoview.GeckoView;
import org.mozilla.geckoview.GeckoViewBridge;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;

import java.util.Arrays;
import java.util.Timer;
import java.util.TimerTask;

import android.util.Log;

public class ActionBarTextSelection implements TextSelection, BundleEventListener {
    private static final String LOGTAG = "GeckoTextSelection";
    private static final int SHUTDOWN_DELAY_MS = 250;

    private final GeckoView geckoView;
    private final ActionModePresenter presenter;

    private int selectionID; // Unique ID provided for each selection action.

    private GeckoBundle[] mCurrentItems;

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

    public ActionBarTextSelection(@NonNull final GeckoView geckoView,
                                  @Nullable final ActionModePresenter presenter) {
        this.geckoView = geckoView;
        this.presenter = presenter;
    }

    @Override
    public void create() {
        GeckoViewBridge.getEventDispatcher(geckoView).registerUiThreadListener(this,
                "TextSelection:ActionbarInit",
                "TextSelection:ActionbarStatus",
                "TextSelection:ActionbarUninit");
    }

    @Override
    public boolean dismiss() {
        // We do not call endActionMode() here because this is already handled by the activity.
        return false;
    }

    @Override
    public void destroy() {
        GeckoViewBridge.getEventDispatcher(geckoView).unregisterUiThreadListener(this,
                "TextSelection:ActionbarInit",
                "TextSelection:ActionbarStatus",
                "TextSelection:ActionbarUninit");
    }

    @Override
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if ("TextSelection:ActionbarInit".equals(event)) {
            // Init / Open the action bar. Note the current selectionID,
            // cancel any pending actionBar close.
            Telemetry.sendUIEvent(TelemetryContract.Event.SHOW,
                TelemetryContract.Method.CONTENT, "text_selection");

            selectionID = message.getInt("selectionID");
            mCurrentItems = null;
            if (mActionModeTimerTask != null) {
                mActionModeTimerTask.cancel();
            }

        } else if ("TextSelection:ActionbarStatus".equals(event)) {
            // Ensure async updates from SearchService for example are valid.
            if (selectionID != message.getInt("selectionID")) {
                return;
            }

            // Update the actionBar actions as provided by Gecko.
            showActionMode(message.getBundleArray("actions"));

        } else if ("TextSelection:ActionbarUninit".equals(event)) {
            // Uninit the actionbar. Schedule a cancellable close
            // action to avoid UI jank. (During SelectionAll for ex).
            mCurrentItems = null;
            mActionModeTimerTask = new ActionModeTimerTask();
            mActionModeTimer.schedule(mActionModeTimerTask, SHUTDOWN_DELAY_MS);
        }
    }

    private void showActionMode(final GeckoBundle[] items) {
        if (Arrays.equals(items, mCurrentItems)) {
            return;
        }
        mCurrentItems = items;

        if (mCallback != null) {
            mCallback.updateItems(items);
            return;
        }

        if (presenter != null) {
            mCallback = new TextSelectionActionModeCallback(items);
            presenter.startActionMode(mCallback);
        }
    }

    private void endActionMode() {
        if (presenter != null) {
            presenter.endActionMode();
        }
        mCurrentItems = null;
    }

    private class TextSelectionActionModeCallback implements ActionMode.Callback {
        private GeckoBundle[] mItems;
        private ActionMode mActionMode;

        public TextSelectionActionModeCallback(final GeckoBundle[] items) {
            mItems = items;
        }

        public void updateItems(final GeckoBundle[] items) {
            mItems = items;
            if (mActionMode != null) {
                mActionMode.invalidate();
            }
        }

        @SuppressLint("AlwaysShowAction")
        @Override
        public boolean onPrepareActionMode(final ActionMode mode, final Menu menu) {
            // Android would normally expect us to only update the state of menu items
            // here To make the js-java interaction a bit simpler, we just wipe out the
            // menu here and recreate all the javascript menu items in onPrepare instead.
            // This will be called any time invalidate() is called on the action mode.
            menu.clear();

            final int length = mItems.length;
            for (int i = 0; i < length; i++) {
                final GeckoBundle obj = mItems[i];
                final MenuItem menuitem = menu.add(0, i, 0, obj.getString("label", ""));
                final int actionEnum = obj.getBoolean("showAsAction")
                        ? MenuItem.SHOW_AS_ACTION_ALWAYS
                        : MenuItem.SHOW_AS_ACTION_NEVER;
                menuitem.setShowAsAction(actionEnum);

                final String iconString = obj.getString("icon", "");
                ResourceDrawableUtils.getDrawable(geckoView.getContext(), iconString,
                        new ResourceDrawableUtils.BitmapLoader() {
                    @Override
                    public void onBitmapFound(Drawable d) {
                        if (d != null) {
                            menuitem.setIcon(d);
                        }
                    }
                });
            }
            return true;
        }

        @Override
        public boolean onCreateActionMode(ActionMode mode, Menu unused) {
            mActionMode = mode;
            return true;
        }

        @Override
        public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
            final GeckoBundle obj = mItems[item.getItemId()];
            final GeckoBundle data = new GeckoBundle(1);
            data.putString("id", obj.getString("id", ""));
            GeckoViewBridge.getEventDispatcher(geckoView).dispatch("TextSelection:Action", data);
            return true;
        }

        // Called when the user exits the action mode
        @Override
        public void onDestroyActionMode(ActionMode mode) {
            mActionMode = null;
            mCallback = null;

            final GeckoBundle data = new GeckoBundle(1);
            data.putInt("selectionID", selectionID);
            GeckoViewBridge.getEventDispatcher(geckoView).dispatch("TextSelection:End", data);
        }
    }
}
