/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.util.Log;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import org.mozilla.gecko.util.ThreadUtils;

import java.util.Arrays;
import java.util.List;

/**
 * Class that implements a basic SelectionActionDelegate. This class is used by GeckoView by
 * default if the consumer does not explicitly set a SelectionActionDelegate.
 *
 * To provide custom actions, extend this class and override the following methods,
 *
 * 1) Override {@link #getAllActions} to include custom action IDs in the returned array. This
 * array must include all actions, available or not, and must not change over the class lifetime.
 *
 * 2) Override {@link #isActionAvailable} to return whether a custom action is currently available.
 *
 * 3) Override {@link #prepareAction} to set custom title and/or icon for a custom action.
 *
 * 4) Override {@link #performAction} to perform a custom action when used.
 */
@UiThread
public class BasicSelectionActionDelegate implements ActionMode.Callback,
                                                     GeckoSession.SelectionActionDelegate {
    private static final String LOGTAG = "GeckoBasicSelectionAction";

    protected static final String ACTION_PROCESS_TEXT = Intent.ACTION_PROCESS_TEXT;

    private static final String[] FLOATING_TOOLBAR_ACTIONS = new String[] {
        ACTION_CUT, ACTION_COPY, ACTION_PASTE, ACTION_SELECT_ALL, ACTION_PROCESS_TEXT
    };
    private static final String[] FIXED_TOOLBAR_ACTIONS = new String[] {
        ACTION_SELECT_ALL, ACTION_CUT, ACTION_COPY, ACTION_PASTE
    };

    protected final @NonNull Activity mActivity;
    protected final boolean mUseFloatingToolbar;
    protected final @NonNull Matrix mTempMatrix = new Matrix();
    protected final @NonNull RectF mTempRect = new RectF();

    private boolean mExternalActionsEnabled;

    protected @Nullable ActionMode mActionMode;
    protected @Nullable GeckoSession mSession;
    protected @Nullable Selection mSelection;
    protected @Nullable List<String> mActions;
    protected @Nullable GeckoResponse<String> mResponse;
    protected boolean mRepopulatedMenu;

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

    public BasicSelectionActionDelegate(final @NonNull Activity activity) {
        this(activity, Build.VERSION.SDK_INT >= 23);
    }

    public BasicSelectionActionDelegate(final @NonNull Activity activity,
                                        final boolean useFloatingToolbar) {
        mActivity = activity;
        mUseFloatingToolbar = useFloatingToolbar;
        mExternalActionsEnabled = true;
    }

    /**
     * Set whether to include text actions from other apps in the floating toolbar.
     *
     * @param enable True if external actions should be enabled.
     */
    public void enableExternalActions(final boolean enable) {
        ThreadUtils.assertOnUiThread();
        mExternalActionsEnabled = enable;

        if (mActionMode != null) {
            mActionMode.invalidate();
        }
    }

    /**
     * Get whether text actions from other apps are enabled.
     *
     * @return True if external actions are enabled.
     */
    public boolean areExternalActionsEnabled() {
        return mExternalActionsEnabled;
    }

    /**
     * Return list of all actions in proper order, regardless of their availability at present.
     * Override to add to or remove from the default set.
     *
     * @return Array of action IDs in proper order.
     */
    protected @NonNull String[] getAllActions() {
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
    protected boolean isActionAvailable(final @NonNull String id) {
        if (mExternalActionsEnabled && !mSelection.text.isEmpty() &&
                ACTION_PROCESS_TEXT.equals(id)) {
            final PackageManager pm = mActivity.getPackageManager();
            return pm.resolveActivity(getProcessTextIntent(),
                                      PackageManager.MATCH_DEFAULT_ONLY) != null;
        }
        return mActions.contains(id);
    }

    /**
     * Prepare a menu item corresponding to a certain action. Override to prepare
     * menu item for custom action.
     *
     * @param id Action ID.
     * @param item New menu item to prepare.
     */
    protected void prepareAction(final @NonNull String id, final @NonNull MenuItem item) {
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
            case ACTION_PROCESS_TEXT:
                throw new IllegalStateException("Unexpected action");
        }
    }

    /**
     * Perform the specified action. Override to perform custom actions.
     *
     * @param id Action ID.
     * @param item Nenu item for the action.
     * @return True if the action was performed.
     */
    protected boolean performAction(final @NonNull String id, final @NonNull MenuItem item) {
        if (ACTION_PROCESS_TEXT.equals(id)) {
            try {
                mActivity.startActivity(item.getIntent());
            } catch (final ActivityNotFoundException e) {
                Log.e(LOGTAG, "Cannot perform action", e);
                return false;
            }
            return true;
        }

        if (mResponse == null) {
            return false;
        }
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

    /**
     * Clear the current selection, if possible.
     */
    protected void clearSelection() {
        if (mResponse != null) {
            if (isActionAvailable(ACTION_COLLAPSE_TO_END)) {
                mResponse.respond(ACTION_COLLAPSE_TO_END);
            } else if (isActionAvailable(ACTION_UNSELECT)) {
                mResponse.respond(ACTION_UNSELECT);
            } else {
                mResponse.respond(ACTION_HIDE);
            }
        }
    }

    private Intent getProcessTextIntent() {
        final Intent intent = new Intent(Intent.ACTION_PROCESS_TEXT);
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        intent.setType("text/plain");
        intent.putExtra(Intent.EXTRA_PROCESS_TEXT, mSelection.text);
        // TODO: implement ability to replace text in Gecko for editable selection (bug 1453137).
        intent.putExtra(Intent.EXTRA_PROCESS_TEXT_READONLY, true);
        return intent;
    }

    @Override
    public boolean onCreateActionMode(final ActionMode actionMode, final Menu menu) {
        ThreadUtils.assertOnUiThread();
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
        ThreadUtils.assertOnUiThread();
        final String[] allActions = getAllActions();
        boolean changed = false;

        // Whether we are repopulating an existing menu.
        mRepopulatedMenu = menu.size() != 0;

        // For each action, see if it's available at present, and if necessary,
        // add to or remove from menu.
        for (int i = 0; i < allActions.length; i++) {
            final String actionId = allActions[i];
            final int menuId = i + Menu.FIRST;

            if (ACTION_PROCESS_TEXT.equals(actionId)) {
                if (mExternalActionsEnabled && !mSelection.text.isEmpty()) {
                    menu.addIntentOptions(menuId, menuId, menuId,
                                          mActivity.getComponentName(),
                                          /* specifiec */ null, getProcessTextIntent(),
                                          /* flags */ 0, /* items */ null);
                    changed = true;
                } else if (menu.findItem(menuId) != null) {
                    menu.removeGroup(menuId);
                    changed = true;
                }
                continue;
            }

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
        ThreadUtils.assertOnUiThread();
        MenuItem realMenuItem = null;
        if (mRepopulatedMenu) {
            // When we repopulate an existing menu, Android can sometimes give us an old,
            // deleted MenuItem. Find the current MenuItem that corresponds to the old one.
            final Menu menu = actionMode.getMenu();
            final int size = menu.size();
            for (int i = 0; i < size; i++) {
                final MenuItem item = menu.getItem(i);
                if (item == menuItem || (item.getItemId() == menuItem.getItemId() &&
                        item.getTitle().equals(menuItem.getTitle()))) {
                    realMenuItem = item;
                    break;
                }
            }
        } else {
            realMenuItem = menuItem;
        }

        if (realMenuItem == null) {
            return false;
        }
        final String[] allActions = getAllActions();
        return performAction(allActions[realMenuItem.getItemId() - Menu.FIRST], realMenuItem);
    }

    @Override
    public void onDestroyActionMode(final ActionMode actionMode) {
        ThreadUtils.assertOnUiThread();
        if (!mUseFloatingToolbar) {
            clearSelection();
        }
        mSession = null;
        mSelection = null;
        mActions = null;
        mResponse = null;
        mActionMode = null;
    }

    public void onGetContentRect(final @Nullable ActionMode mode, final @Nullable View view,
                                 final @NonNull Rect outRect) {
        ThreadUtils.assertOnUiThread();
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
                                    final GeckoResponse<String> response) {
        ThreadUtils.assertOnUiThread();
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
    public void onHideAction(final GeckoSession session, final int reason) {
        ThreadUtils.assertOnUiThread();
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
