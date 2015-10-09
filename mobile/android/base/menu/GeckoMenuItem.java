/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.menu;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.R;
import org.mozilla.gecko.widget.GeckoActionProvider;

import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.ActionProvider;
import android.view.ContextMenu;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;

public class GeckoMenuItem implements MenuItem {
    // These values mirror MenuItem values that are only available on API >= 11.
    public static final int SHOW_AS_ACTION_NEVER = 0;
    public static final int SHOW_AS_ACTION_IF_ROOM = 1;
    public static final int SHOW_AS_ACTION_ALWAYS = 2;
    public static final int SHOW_AS_ACTION_IF_ROOM_WITH_TEXT = 5;

    // A View that can show a MenuItem should be able to initialize from
    // the properties of the MenuItem.
    public static interface Layout {
        public void initialize(GeckoMenuItem item);
        public void setShowIcon(boolean show);
    }

    public static interface OnShowAsActionChangedListener {
        public boolean hasActionItemBar();
        public void onShowAsActionChanged(GeckoMenuItem item);
    }

    private final int mId;
    private final int mOrder;
    private View mActionView;
    private int mActionEnum;
    private CharSequence mTitle;
    private CharSequence mTitleCondensed;
    private boolean mCheckable;
    private boolean mChecked;
    private boolean mVisible = true;
    private boolean mEnabled = true;
    private Drawable mIcon;
    private int mIconRes;
    private GeckoActionProvider mActionProvider;
    private GeckoSubMenu mSubMenu;
    private MenuItem.OnMenuItemClickListener mMenuItemClickListener;
    final GeckoMenu mMenu;
    OnShowAsActionChangedListener mShowAsActionChangedListener;

    private volatile boolean mShouldDispatchChanges = true;
    private volatile boolean mDidChange;

    public GeckoMenuItem(GeckoMenu menu, int id, int order, int titleRes) {
        mMenu = menu;
        mId = id;
        mOrder = order;
        mTitle = mMenu.getResources().getString(titleRes);
    }

    public GeckoMenuItem(GeckoMenu menu, int id, int order, CharSequence title) {
        mMenu = menu;
        mId = id;
        mOrder = order;
        mTitle = title;
    }

    /**
     * Stop dispatching item changed events to presenters until
     * [start|resume]DispatchingItemsChanged() is called. Useful when
     * many menu operations are going to be performed as a batch.
     */
    public void stopDispatchingChanges() {
        mDidChange = false;
        mShouldDispatchChanges = false;
    }

    /**
     * Resume dispatching item changed events to presenters. This method
     * will NOT call onItemChanged if any menu operations were queued.
     * Only future menu operations will call onItemChanged. Useful for
     * sequelching presenter updates.
     */
    public void resumeDispatchingChanges() {
        mShouldDispatchChanges = true;
    }

    /**
     * Start dispatching item changed events to presenters. This method
     * will call onItemChanged if any menu operations were queued.
     */
    public void startDispatchingChanges() {
        if (mDidChange) {
            mMenu.onItemChanged(this);
        }
        mShouldDispatchChanges = true;
    }

    @Override
    public boolean collapseActionView() {
        return false;
    }

    @Override
    public boolean expandActionView() {
        return false;
    }

    public boolean hasActionProvider() {
        if (Versions.preICS) {
            return false;
        }

        return (mActionProvider != null);
    }

    public int getActionEnum() {
        return mActionEnum;
    }

    public GeckoActionProvider getGeckoActionProvider() {
        return mActionProvider;
    }

    @Override
    public ActionProvider getActionProvider() {
        return null;
    }

    @Override
    public View getActionView() {
        if (mActionProvider != null) {
            return mActionProvider.onCreateActionView(GeckoActionProvider.ActionViewType.DEFAULT);
        }

        return mActionView;
    }

    @Override
    public char getAlphabeticShortcut() {
        return 0;
    }

    @Override
    public int getGroupId() {
        return 0;
    }

    @Override
    public Drawable getIcon() {
        if (mIcon == null) {
            if (mIconRes != 0)
                return mMenu.getResources().getDrawable(mIconRes);
            else
                return null;
        } else {
            return mIcon;
        }
    }

    @Override
    public Intent getIntent() {
        return null;
    }

    @Override
    public int getItemId() {
        return mId;
    }

    @Override
    public ContextMenu.ContextMenuInfo getMenuInfo() {
        return null;
    }

    @Override
    public char getNumericShortcut() {
        return 0;
    }

    @Override
    public int getOrder() {
        return mOrder;
    }

    @Override
    public SubMenu getSubMenu() {
        return mSubMenu;
    }

    @Override
    public CharSequence getTitle() {
        return mTitle;
    }

    @Override
    public CharSequence getTitleCondensed() {
        return mTitleCondensed;
    }

    @Override
    public boolean hasSubMenu() {
        if (mActionProvider != null)
            return mActionProvider.hasSubMenu();

        return (mSubMenu != null);
    }

    public boolean isActionItem() {
        return (mActionEnum > 0);
    }

    @Override
    public boolean isActionViewExpanded() {
        return false;
    }

    @Override
    public boolean isCheckable() {
        return mCheckable;
    }

    @Override
    public boolean isChecked() {
        return mChecked;
    }

    @Override
    public boolean isEnabled() {
        return mEnabled;
    }

    @Override
    public boolean isVisible() {
        return mVisible;
    }

    @Override
    public MenuItem setActionProvider(ActionProvider actionProvider) {
        return this;
    }

    public MenuItem setActionProvider(GeckoActionProvider actionProvider) {
        mActionProvider = actionProvider;
        if (mActionProvider != null) {
            actionProvider.setOnTargetSelectedListener(new GeckoActionProvider.OnTargetSelectedListener() {
                @Override
                public void onTargetSelected() {
                    mMenu.close();

                    // Refresh the menu item to show the high frequency apps.
                    mShowAsActionChangedListener.onShowAsActionChanged(GeckoMenuItem.this);
                }
            });
        }

        mShowAsActionChangedListener.onShowAsActionChanged(this);
        return this;
    }

    @Override
    public MenuItem setActionView(int resId) {
        return this;
    }

    @Override
    public MenuItem setActionView(View view) {
        return this;
    }

    @Override
    public MenuItem setAlphabeticShortcut(char alphaChar) {
        return this;
    }

    @Override
    public MenuItem setCheckable(boolean checkable) {
        if (mCheckable != checkable) {
            mCheckable = checkable;
            if (mShouldDispatchChanges) {
                mMenu.onItemChanged(this);
            } else {
                mDidChange = true;
            }
        }
        return this;
    }

    @Override
    public MenuItem setChecked(boolean checked) {
        if (mChecked != checked) {
            mChecked = checked;
            if (mShouldDispatchChanges) {
                mMenu.onItemChanged(this);
            } else {
                mDidChange = true;
            }
        }
        return this;
    }

    @Override
    public MenuItem setEnabled(boolean enabled) {
        if (mEnabled != enabled) {
            mEnabled = enabled;
            if (mShouldDispatchChanges) {
                mMenu.onItemChanged(this);
            } else {
                mDidChange = true;
            }
        }
        return this;
    }

    @Override
    public MenuItem setIcon(Drawable icon) {
        if (mIcon != icon) {
            mIcon = icon;
            if (mShouldDispatchChanges) {
                mMenu.onItemChanged(this);
            } else {
                mDidChange = true;
            }
        }
        return this;
    }

    @Override
    public MenuItem setIcon(int iconRes) {
        if (mIconRes != iconRes) {
            mIconRes = iconRes;
            if (mShouldDispatchChanges) {
                mMenu.onItemChanged(this);
            } else {
                mDidChange = true;
            }
        }
        return this;
    }

    @Override
    public MenuItem setIntent(Intent intent) {
        return this;
    }

    @Override
    public MenuItem setNumericShortcut(char numericChar) {
        return this;
    }

    @Override
    public MenuItem setOnActionExpandListener(MenuItem.OnActionExpandListener listener) {
        return this;
    }

    @Override
    public MenuItem setOnMenuItemClickListener(MenuItem.OnMenuItemClickListener menuItemClickListener) {
        mMenuItemClickListener = menuItemClickListener;
        return this;
    }

    @Override
    public MenuItem setShortcut(char numericChar, char alphaChar) {
        return this;
    }

    @Override
    public void setShowAsAction(int actionEnum) {
        setShowAsAction(actionEnum, 0);
    }

    public void setShowAsAction(int actionEnum, int style) {
        if (mShowAsActionChangedListener == null)
            return;

        if (mActionEnum == actionEnum)
            return;

        if (actionEnum > 0) {
            if (!mShowAsActionChangedListener.hasActionItemBar())
                return;

            if (!hasActionProvider()) {
                // Change the type to just an icon
                MenuItemActionBar actionView;
                if (style != 0) {
                    actionView = new MenuItemActionBar(mMenu.getContext(), null, style);
                } else {
                    if (actionEnum == SHOW_AS_ACTION_ALWAYS) {
                        actionView = new MenuItemActionBar(mMenu.getContext());
                    } else {
                        actionView = new MenuItemActionBar(mMenu.getContext(), null, R.attr.menuItemSecondaryActionBarStyle);
                    }
                }

                actionView.initialize(this);
                mActionView = actionView;
            }

            mActionEnum = actionEnum;
        }

        mShowAsActionChangedListener.onShowAsActionChanged(this);
    }

    @Override
    public MenuItem setShowAsActionFlags(int actionEnum) {
        return this;
    }

    public MenuItem setSubMenu(GeckoSubMenu subMenu) {
        mSubMenu = subMenu;
        return this;
    }

    @Override
    public MenuItem setTitle(CharSequence title) {
        if (!TextUtils.equals(mTitle, title)) {
            mTitle = title;
            if (mShouldDispatchChanges) {
                mMenu.onItemChanged(this);
            } else {
                mDidChange = true;
            }
        }
        return this;
    }

    @Override
    public MenuItem setTitle(int title) {
        CharSequence newTitle = mMenu.getResources().getString(title);
        return setTitle(newTitle);
    }

    @Override
    public MenuItem setTitleCondensed(CharSequence title) {
        mTitleCondensed = title;
        return this;
    }

    @Override
    public MenuItem setVisible(boolean visible) {
        // Action views are not normal menu items and visibility can get out
        // of sync unless we dispatch whenever required.
        if (isActionItem() || mVisible != visible) {
            mVisible = visible;
            if (mShouldDispatchChanges) {
                mMenu.onItemChanged(this);
            } else {
                mDidChange = true;
            }
        }
        return this;
    }

    public boolean invoke() {
        if (mMenuItemClickListener != null)
            return mMenuItemClickListener.onMenuItemClick(this);
        else
            return false;
    }

    public void setOnShowAsActionChangedListener(OnShowAsActionChangedListener listener) {
        mShowAsActionChangedListener = listener;
    }
}
