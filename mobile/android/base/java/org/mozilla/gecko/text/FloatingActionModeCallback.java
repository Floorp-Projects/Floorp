/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.text;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.os.Build;
import android.support.annotation.NonNull;
import android.text.TextPaint;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.R;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.WindowUtils;
import org.mozilla.geckoview.GeckoViewBridge;

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
            final String actionLabel = getOneLinerMenuText(action.getLabel());
            menu.add(Menu.NONE, i, action.getFloatingOrder(), actionLabel);
        }

        return true;
    }

    @Override
    public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
        final TextAction action = actions.get(item.getItemId());

        final GeckoBundle data = new GeckoBundle(1);
        data.putString("id", action.getId());
        GeckoViewBridge.getEventDispatcher(textSelection.geckoView).dispatch("TextSelection:Action", data);

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

    // There's currently a bug in Android's framework that manifests by placing the floating menu
    // off-screen if a menu label overflows the menu's width.
    // https://issuetracker.google.com/issues/137169336
    // To overcome this we'll manually check and truncate any menu label that could cause issues.
    private static @NonNull String getOneLinerMenuText(@NonNull final String text) {
        final int textLength = text.length();

        // Avoid heavy unneeded calculations if the text is small
        if (textLength < 30) {
            return text;
        }

        // Simulate as best as possible the floating menu button style used in Android framework
        // https://android.googlesource.com/platform/frameworks/base/+/eb101a7/core/java/com/android/internal/widget/FloatingToolbar.java
        final Context context = GeckoAppShell.getApplicationContext();
        final Resources resources = context.getResources();
        final TextPaint textPaint = new TextPaint();
        textPaint.setTextSize(resources.getDimensionPixelSize(R.dimen.floating_toolbar_text_size));
        textPaint.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
        final int screenWidth = WindowUtils.getScreenWidth(context);
        final int menuWidth = screenWidth
                - (2 * resources.getDimensionPixelSize(R.dimen.floating_toolbar_horizontal_margin))
                - (2 * resources.getDimensionPixelSize(R.dimen.floating_toolbar_menu_button_side_padding));

        // If the text cannot fit on one line ellipsize it manually
        final int charactersThatFit = textPaint.breakText(text, 0, textLength, true, menuWidth, null);
        final boolean shouldEllipsize = textLength > charactersThatFit;
        if (shouldEllipsize) {
            return text.substring(0, charactersThatFit - 3) + "...";
        } else {
            return text;
        }
    }
}
