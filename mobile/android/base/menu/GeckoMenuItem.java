/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.menu;

import org.mozilla.gecko.widget.GeckoActionProvider;

import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.view.ActionProvider;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;

public class GeckoMenuItem implements MenuItem, View.OnClickListener {
    private static final String LOGTAG = "GeckoMenuItem";

    public static interface Layout {
        public void setId(int id);
        public void setIcon(Drawable icon);
        public void setIcon(int iconRes);
        public void setTitle(CharSequence title);
        public void setEnabled(boolean enabled);
        public void setCheckable(boolean checkable);
        public void setChecked(boolean checked);
        public void setOnClickListener(View.OnClickListener listener);
        public void setSubMenuIndicator(boolean hasSubMenu);
        public void setVisibility(int visible);
        public View getView();
    }

    public static interface OnShowAsActionChangedListener {
        public boolean hasActionItemBar();
        public void onShowAsActionChanged(GeckoMenuItem item, boolean isActionItem);
    }

    public static interface OnVisibilityChangedListener {
        public void onVisibilityChanged(GeckoMenuItem item, boolean isVisible);
    }

    private Context mContext;
    private int mId;
    private int mOrder;
    private Layout mLayout;
    private boolean mActionItem;
    private CharSequence mTitle;
    private CharSequence mTitleCondensed;
    private boolean mCheckable;
    private boolean mChecked;
    private boolean mVisible;
    private boolean mEnabled;
    private Drawable mIcon;
    private int mIconRes;
    private ActionProvider mActionProvider;
    private GeckoMenu mMenu;
    private GeckoSubMenu mSubMenu;
    private MenuItem.OnMenuItemClickListener mMenuItemClickListener;
    private OnVisibilityChangedListener mVisibilityChangedListener;
    private OnShowAsActionChangedListener mShowAsActionChangedListener;

    public GeckoMenuItem(Context context, int id) {
        mContext = context;
        mLayout = new MenuItemDefault(context, null);
        mLayout.setId(id);

        mId = id;
        mOrder = 0;
        mActionItem = false;
        mVisible = true;
        mEnabled = true;
        mCheckable = true;
        mChecked = false;
        mMenuItemClickListener = null;
    }

    public GeckoMenuItem(Context context, int id, int order) {
        this(context, id);
        mOrder = order;
    }

    @Override
    public boolean collapseActionView() {
        return false;
    }

    @Override
    public boolean expandActionView() {
        return false;
    }

    @Override
    public ActionProvider getActionProvider() {
        return mActionProvider;
    }

    @Override
    public View getActionView() {
        if (mActionProvider != null && mActionProvider instanceof GeckoActionProvider) {
            final View view = ((GeckoActionProvider) mActionProvider).getView(this);
            view.setOnClickListener(this);
            return view;
        }

        return null;
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
                return mContext.getResources().getDrawable(mIconRes);
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

    public View getLayout() {
        if (mActionProvider != null)
            return getActionView();

        return mLayout.getView();
    }

    public void setMenu(GeckoMenu menu) {
        mMenu = menu;
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
        if (mActionProvider != null)
            mActionProvider.onPrepareSubMenu(mSubMenu);

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
        mActionProvider = actionProvider;
        mSubMenu = new GeckoSubMenu(mContext, null);
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
        mCheckable = checkable;
        mLayout.setCheckable(checkable);
        return this;
    }

    @Override
    public MenuItem setChecked(boolean checked) {
        mChecked = checked;
        mLayout.setChecked(checked);
        return this;
    }

    @Override
    public MenuItem setEnabled(boolean enabled) {
        mEnabled = enabled;
        mLayout.setEnabled(enabled);
        return this;
    }

    @Override
    public MenuItem setIcon(Drawable icon) {
        mIcon = icon;
        mLayout.setIcon(icon);
        return this;
    }

    @Override
    public MenuItem setIcon(int iconRes) {
        mIconRes = iconRes;
        mLayout.setIcon(iconRes);
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
        if (mShowAsActionChangedListener == null)
            return;

        if (mActionItem == (actionEnum > 0))
            return;

        if (actionEnum > 0) {
            if (!mShowAsActionChangedListener.hasActionItemBar())
                return;

            // Change the type to just an icon
            mLayout = new MenuItemActionBar(mContext, null);
        } else {
            // Change the type to default
            mLayout = new MenuItemDefault(mContext, null);
        }

        mActionItem = (actionEnum > 0);         

        mLayout.setId(mId);
        mLayout.setOnClickListener(this);

        setTitle(mTitle);        
        setVisible(mVisible);
        setEnabled(mEnabled);
        setCheckable(mCheckable);
        setChecked(mChecked);

        if (mIcon == null)
            setIcon(mIconRes);
        else
            setIcon(mIcon);
        
        mShowAsActionChangedListener.onShowAsActionChanged(this, mActionItem);
    }

    @Override
    public MenuItem setShowAsActionFlags(int actionEnum) {
        return this;
    }

    public MenuItem setSubMenu(GeckoSubMenu subMenu) {
        mSubMenu = subMenu;
        mLayout.setSubMenuIndicator(subMenu != null);
        return this;
    }

    @Override
    public MenuItem setTitle(CharSequence title) {
        mTitle = title;
        mLayout.setTitle(mTitle);
        return this;
    }

    @Override
    public MenuItem setTitle(int title) {
        mTitle = mContext.getResources().getString(title);
        mLayout.setTitle(mTitle);
        return this;
    }

    @Override
    public MenuItem setTitleCondensed(CharSequence title) {
        mTitleCondensed = title;
        return this;
    }

    @Override
    public MenuItem setVisible(boolean visible) {
        mVisible = visible;
        mLayout.setVisibility(visible ? View.VISIBLE : View.GONE);

        if (mVisibilityChangedListener != null)
            mVisibilityChangedListener.onVisibilityChanged(this, visible);

        return this;
    }

    @Override
    public void onClick(View view) {
        // If there is a custom listener, pass it to parent menu, so that it can do default cleanups.
        if (mMenuItemClickListener != null) {
            if (mMenuItemClickListener instanceof GeckoMenu)
                mMenuItemClickListener.onMenuItemClick(this);
            else
                mMenu.onCustomMenuItemClick(this, mMenuItemClickListener);
        }
    }

    public void setOnShowAsActionChangedListener(OnShowAsActionChangedListener listener) {
        mShowAsActionChangedListener = listener;
    }

    public void setOnVisibilityChangedListener(OnVisibilityChangedListener listener) {
        mVisibilityChangedListener = listener;
    }
}
