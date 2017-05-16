package org.mozilla.focus.menu;

import android.app.PendingIntent;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.TextView;

import org.mozilla.focus.R;

/**
 * Custom tab menu items look just the same as any other menu items. The primary difference
 * is that they each have a custom pending intent (which we need to execute), as opposed to
 * simply redirecting to the BrowserFragment menu item handling (which is performed via the MenuItem
 * id).
 */
/* package-private */ class CustomTabMenuItemViewHolder extends MenuItemViewHolder {
    /* package-private */ static final int LAYOUT_ID = R.layout.custom_tab_menu_item;

    private BrowserMenu menu;
    private PendingIntent pendingIntent;

    /* package-private */ CustomTabMenuItemViewHolder(View itemView) {
        super(itemView);
    }

    @Override
    public void setMenu(BrowserMenu menu) {
        this.menu = menu;
    }

    @Override
    public void onClick(View view) {
        // Usually this is done in BrowserMenuViewHolder.onClick(), but that also tries to submit
        // the click up to the fragment, which we explicitly don't want here:
        if (menu != null) {
            menu.dismiss();
        }

        if (pendingIntent == null) {
            throw new IllegalStateException("No PendingIntent set for CustomTabMenuItemViewHolder");
        }

        try {
            pendingIntent.send();
        } catch (PendingIntent.CanceledException e) {
            // There's really nothing we can do here...
        }
    }

    /* package-private */ void bind(BrowserMenuAdapter.MenuItem menuItem) {
        super.bind(menuItem);

        pendingIntent = menuItem.pendingIntent;
    }
}
