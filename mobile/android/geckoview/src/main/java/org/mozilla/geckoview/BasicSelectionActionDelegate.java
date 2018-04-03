/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.annotation.TargetApi;
import android.app.Activity;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import java.util.Arrays;
import java.util.List;

public class BasicSelectionActionDelegate implements ActionMode.Callback,
                                             GeckoSession.SelectionActionDelegate {
    private static final String LOGTAG = "GeckoBasicSelectionAction";

    private static final String[] FLOATING_TOOLBAR_ACTIONS = new String[] {
            ACTION_CUT, ACTION_COPY, ACTION_PASTE, ACTION_SELECT_ALL
    };
    private static final String[] FIXED_TOOLBAR_ACTIONS = new String[] {
            ACTION_SELECT_ALL, ACTION_CUT, ACTION_COPY, ACTION_PASTE
    };

    protected final Activity mActivity;
    protected final boolean mUseFloatingToolbar;
    protected final Matrix mTempMatrix = new Matrix();
    protected final RectF mTempRect = new RectF();

    protected ActionMode mActionMode;
    protected GeckoSession mSession;
    protected Selection mSelection;
    protected List<String> mActions;
    protected GeckoSession.Response<String> mResponse;

    @TargetApi(Build.VERSION_CODES.M)
    private class Callback2Wrapper extends ActionMode.Callback2 {
        @Override
        public boolean onCreateActionMode(final ActionMode actionMode, final Menu menu) {
            return BasicSelectionActionDelegate.this.onCreateActionMode(actionMode, menu);
        }

        @Override
        public boolean onPrepareActionMode(final ActionMode actionMode, final Menu menu) {
            return BasicSelectionActionDelegate.this.onPrepareActionMode(actionMode, menu);
        }

        @Override
        public boolean onActionItemClicked(final ActionMode actionMode, final MenuItem menuItem) {
            return BasicSelectionActionDelegate.this.onActionItemClicked(actionMode, menuItem);
        }

        @Override
        public void onDestroyActionMode(final ActionMode actionMode) {
            BasicSelectionActionDelegate.this.onDestroyActionMode(actionMode);
        }

        @Override
        public void onGetContentRect(final ActionMode mode, final View view, final Rect outRect) {
            super.onGetContentRect(mode, view, outRect);
            BasicSelectionActionDelegate.this.onGetContentRect(mode, view, outRect);
        }
    }

    public BasicSelectionActionDelegate(final Activity activity) {
        this(activity, Build.VERSION.SDK_INT >= 23);
    }

    public BasicSelectionActionDelegate(final Activity activity, final boolean useFloatingToolbar) {
        mActivity = activity;
        mUseFloatingToolbar = useFloatingToolbar;
    }

    /**
     * Return list of all actions in proper order, regardless of their availability at present.
     * Override to add to or remove from the default set.
     *
     * @return Array of action IDs in proper order.
     */
    protected String[] getAllActions() {
        return mUseFloatingToolbar ? FLOATING_TOOLBAR_ACTIONS
                                   : FIXED_TOOLBAR_ACTIONS;
    }

    /**
     * Return whether an action is presently available. Override to indicate
     * availability for custom actions.
     *
     * @param id Action ID.
     * @return True if the action is presently available.
     */
    protected boolean isActionAvailable(final String id) {
        return mActions.contains(id);
    }

    /**
     * Prepare a menu item corresponding to a certain action. Override to prepare
     * menu item for custom action.
     *
     * @param id Action ID.
     * @param item New menu item to prepare.
     */
    protected void prepareAction(final String id, final MenuItem item) {
        switch (id) {
            case ACTION_CUT:
                item.setTitle(android.R.string.cut);
                break;
            case ACTION_COPY:
                item.setTitle(android.R.string.copy);
                break;
            case ACTION_PASTE:
                item.setTitle(android.R.string.paste);
                break;
            case ACTION_SELECT_ALL:
                item.setTitle(android.R.string.selectAll);
                break;
        }
    }

    /**
     * Perform the specified action. Override to perform custom actions.
     *
     * @param id Action ID.
     * @return True if the action was performed.
     */
    protected boolean performAction(final String id) {
        mResponse.respond(id);

        // Android behavior is to clear selection on copy.
        if (ACTION_COPY.equals(id)) {
            if (mUseFloatingToolbar) {
                clearSelection();
            } else {
                mActionMode.finish();
            }
        }
        return true;
    }

    protected void clearSelection() {
        if (mResponse != null) {
            if (isActionAvailable(ACTION_COLLAPSE_TO_END)) {
                mResponse.respond(ACTION_COLLAPSE_TO_END);
            } else {
                mResponse.respond(ACTION_UNSELECT);
            }
        }
    }

    @Override
    public boolean onCreateActionMode(final ActionMode actionMode, final Menu menu) {
        final String[] allActions = getAllActions();
        for (final String actionId : allActions) {
            if (isActionAvailable(actionId)) {
                if (!mUseFloatingToolbar && (
                        Build.VERSION.SDK_INT == 22 || Build.VERSION.SDK_INT == 23)) {
                    // Android bug where onPrepareActionMode is not called initially.
                    onPrepareActionMode(actionMode, menu);
                }
                return true;
            }
        }
        return false;
    }

    @Override
    public boolean onPrepareActionMode(final ActionMode actionMode, final Menu menu) {
        final String[] allActions = getAllActions();
        boolean changed = false;

        // For each action, see if it's available at present, and if necessary,
        // add to or remove from menu.
        for (int menuId = 0; menuId < allActions.length; menuId++) {
            final String actionId = allActions[menuId];
            if (isActionAvailable(actionId)) {
                if (menu.findItem(menuId) == null) {
                    prepareAction(actionId, menu.add(/* group */ Menu.NONE, menuId,
                                                     menuId, /* title */ ""));
                    changed = true;
                }
            } else if (menu.findItem(menuId) != null) {
                menu.removeItem(menuId);
                changed = true;
            }
        }
        return changed;
    }

    @Override
    public boolean onActionItemClicked(final ActionMode actionMode, final MenuItem menuItem) {
        final String[] allActions = getAllActions();
        return performAction(allActions[menuItem.getItemId()]);
    }

    @Override
    public void onDestroyActionMode(final ActionMode actionMode) {
        if (!mUseFloatingToolbar) {
            clearSelection();
        }
        mSession = null;
        mSelection = null;
        mActions = null;
        mResponse = null;
        mActionMode = null;
    }

    public void onGetContentRect(final ActionMode mode, final View view, final Rect outRect) {
        if (mSelection.clientRect == null) {
            return;
        }
        mSession.getClientToScreenMatrix(mTempMatrix);
        mTempMatrix.mapRect(mTempRect, mSelection.clientRect);
        mTempRect.roundOut(outRect);
    }

    @TargetApi(Build.VERSION_CODES.M)
    @Override
    public void onShowActionRequest(final GeckoSession session, final Selection selection,
                                    final String[] actions,
                                    final GeckoSession.Response<String> response) {
        mSession = session;
        mSelection = selection;
        mActions = Arrays.asList(actions);
        mResponse = response;

        if (mActionMode != null) {
            if (actions.length > 0) {
                mActionMode.invalidate();
            } else {
                mActionMode.finish();
            }
            return;
        }

        if (mUseFloatingToolbar) {
            mActionMode = mActivity.startActionMode(new Callback2Wrapper(),
                                                    ActionMode.TYPE_FLOATING);
        } else {
            mActionMode = mActivity.startActionMode(this);
        }
    }

    @Override
    public void onHideAction(GeckoSession session, int reason) {
        if (mActionMode == null) {
            return;
        }

        switch (reason) {
            case HIDE_REASON_ACTIVE_SCROLL:
            case HIDE_REASON_ACTIVE_SELECTION:
            case HIDE_REASON_INVISIBLE_SELECTION:
                if (mUseFloatingToolbar) {
                    // Hide the floating toolbar when scrolling/selecting.
                    mActionMode.finish();
                }
                break;

            case HIDE_REASON_NO_SELECTION:
                mActionMode.finish();
                break;
        }
    }
}
