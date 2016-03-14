/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.text;

import android.annotation.TargetApi;
import android.graphics.Rect;
import android.os.Build;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import org.mozilla.gecko.GeckoAppShell;

import java.util.List;

@TargetApi(Build.VERSION_CODES.M)
public class FloatingActionModeCallback extends ActionMode.Callback2 {
    private FloatingToolbarTextSelection textSelection;
    private List<TextAction> actions;

    public FloatingActionModeCallback(FloatingToolbarTextSelection textSelection, List<TextAction> actions) {
        this.textSelection = textSelection;
        this.actions = actions;
    }

    public void updateActions(List<TextAction> actions) {
        this.actions = actions;
    }

    @Override
    public boolean onCreateActionMode(ActionMode mode, Menu menu) {
        return true;
    }

    @Override
    public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
        menu.clear();

        for (int i = 0; i < actions.size(); i++) {
            final TextAction action = actions.get(i);
            menu.add(Menu.NONE, i, action.getFloatingOrder(), action.getLabel());
        }

        return true;
    }

    @Override
    public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
        final TextAction action = actions.get(item.getItemId());

        GeckoAppShell.notifyObservers("TextSelection:Action", action.getId());

        return true;
    }

    @Override
    public void onDestroyActionMode(ActionMode mode) {}

    @Override
    public void onGetContentRect(ActionMode mode, View view, Rect outRect) {
        final Rect contentRect = textSelection.contentRect;
        if (contentRect != null) {
            outRect.set(contentRect);
        }
    }
}
