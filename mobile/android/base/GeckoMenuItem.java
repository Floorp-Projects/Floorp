/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.view.ActionProvider;
import android.view.ContextMenu;
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
        public void setVisibility(int visible);
        public View getLayout();
    }

    public static interface OnShowAsActionChangedListener {
        public boolean hasActionItemBar();
        public void onShowAsActionChanged(GeckoMenuItem item, boolean isActionItem);
    }

    private Context mContext;
    private int mId;
    private int mOrder;
    private GeckoMenuItem.Layout mLayout;
    private boolean mActionItem;
    private CharSequence mTitle;
    private CharSequence mTitleCondensed;
    private boolean mCheckable;
    private boolean mChecked;
    private boolean mVisible;
    private boolean mEnabled;
    private Drawable mIcon;
    private int mIconRes;
    private MenuItem.OnMenuItemClickListener mMenuItemClickListener;
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

        mLayout.setOnClickListener(this);
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
        return null;
    }

    @Override
    public View getActionView() {
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
        return null;
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
        return mLayout.getLayout();
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
        return null;
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
        return false;
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

        if (mActionItem == (actionEnum == 1))
            return;

        if (actionEnum == 1) {
            if (!mShowAsActionChangedListener.hasActionItemBar())
                return;

            // Change the type to just an icon
            mLayout = new MenuItemActionBar(mContext, null);
        } else {
            // Change the type to default
            mLayout = new MenuItemDefault(mContext, null);
        }

        mActionItem = (actionEnum == 1);         

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
        return this;
    }

    @Override
    public void onClick(View view) {
        if (mMenuItemClickListener != null)
            mMenuItemClickListener.onMenuItemClick(this);
    }

    public void setOnShowAsActionChangedListener(OnShowAsActionChangedListener listener) {
        mShowAsActionChangedListener = listener;
    } 
}
