/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.pwa.PwaUtils;
import org.mozilla.gecko.util.DrawableUtil;
import org.mozilla.gecko.util.ResourceDrawableUtils;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.DrawableUtil;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ResourceDrawableUtils;
import org.mozilla.gecko.util.ShortcutUtils;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.widget.GeckoPopupMenu;
import org.mozilla.gecko.widget.themed.ThemedImageButton;
import org.mozilla.gecko.widget.themed.ThemedLinearLayout;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.support.v4.content.ContextCompat;
import android.support.v4.view.ViewCompat;
import android.util.AttributeSet;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;

import java.util.Iterator;
import java.util.List;
import java.util.UUID;
import java.util.ArrayList;

import static org.mozilla.gecko.toolbar.PageActionLayout.PageAction.UUID_PAGE_ACTION_PWA;

public class PageActionLayout extends ThemedLinearLayout implements BundleEventListener,
        View.OnClickListener,
        View.OnLongClickListener {
    private static final String MENU_BUTTON_KEY = "MENU_BUTTON_KEY";
    private static final int DEFAULT_PAGE_ACTIONS_SHOWN = 2;
    public static final String PREF_PWA_ONBOARDING = GeckoPreferences.NON_PREF_PREFIX + "pref_pwa_onboarding";


    private final Context mContext;
    private final LinearLayout mLayout;
    private final List<PageAction> mPageActionList;

    private GeckoPopupMenu mPageActionsMenu;

    // By default it's two, can be changed by calling setNumberShown(int)
    private int mMaxVisiblePageActions;
    private PwaConfirm mPwaConfirm;

    public PageActionLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mLayout = this;

        mPageActionList = new ArrayList<PageAction>();
        setNumberShown(DEFAULT_PAGE_ACTIONS_SHOWN);
        refreshPageActionIcons();
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();

        EventDispatcher.getInstance().registerUiThreadListener(this,
                "PageActions:Add",
                "PageActions:Remove");
    }

    @Override
    protected void onDetachedFromWindow() {
        EventDispatcher.getInstance().unregisterUiThreadListener(this,
                "PageActions:Add",
                "PageActions:Remove");

        super.onDetachedFromWindow();
    }

    @Override
    public void setPrivateMode(boolean isPrivate) {
        super.setPrivateMode(isPrivate);
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            if (child instanceof ThemedImageButton) {
                ((ThemedImageButton) child).setPrivateMode(true);
            }
        }
    }

    private void setNumberShown(int count) {
        ThreadUtils.assertOnUiThread();

        mMaxVisiblePageActions = count;

        for (int index = 0; index < count; index++) {
            if ((getChildCount() - 1) < index) {
                mLayout.addView(createImageButton());
            }
        }
    }

    @Override // BundleEventListener
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        ThreadUtils.assertOnUiThread();

        hidePreviousConfirmPrompt();

        if ("PageActions:Add".equals(event)) {
            final String id = message.getString("id");

            boolean alreadyAdded = isPwaAdded(id);
            if (alreadyAdded) {
                return;
            }

            maybeShowPwaOnboarding(id);

            final String title = message.getString("title");
            final String imageURL = message.getString("icon");
            final boolean important = message.getBoolean("important");
            final boolean useTint = message.getBoolean("useTint");

            addPageAction(id, title, imageURL, useTint, new OnPageActionClickListeners() {
                @Override
                public void onClick(final String id) {
                    if (UUID_PAGE_ACTION_PWA.equals(id)) {
                        mPwaConfirm = PwaConfirm.show(getContext());
                        return;
                    }
                    final GeckoBundle data = new GeckoBundle(1);
                    data.putString("id", id);
                    EventDispatcher.getInstance().dispatch("PageActions:Clicked", data);
                }

                @Override
                public boolean onLongClick(String id) {
                    final GeckoBundle data = new GeckoBundle(1);
                    data.putString("id", id);
                    EventDispatcher.getInstance().dispatch("PageActions:LongClicked", data);
                    return true;
                }
            }, important);

        } else if ("PageActions:Remove".equals(event)) {
            removePageAction(message.getString("id"));
        }
    }

    private void maybeShowPwaOnboarding(String id) {
        // only show pwa at normal mode
        final Tab selectedTab = Tabs.getInstance().getSelectedTab();
        if (!PwaUtils.shouldAddPwaShortcut(selectedTab)) {
            return;
        }
        if (UUID_PAGE_ACTION_PWA.equals(id)) {
            final SharedPreferences prefs = GeckoSharedPrefs.forApp(getContext());
            final boolean show = prefs.getBoolean(PREF_PWA_ONBOARDING, true);
            if (show && ShortcutUtils.isPinShortcutSupported()) {
                PwaOnboarding.show(getContext());
                prefs.edit().putBoolean(PREF_PWA_ONBOARDING, false).apply();
            }
        }
    }

    private boolean isPwaAdded(String id) {
        for (PageAction pageAction : mPageActionList) {
            if (pageAction.getID() != null && pageAction.getID().equals(id)) {
                return true;
            }
        }
        return false;
    }

    private void addPageAction(final String id, final String title, final String imageData, final boolean useTint,
                               final OnPageActionClickListeners onPageActionClickListeners, boolean important) {
        ThreadUtils.assertOnUiThread();

        final PageAction pageAction = new PageAction(id, title, null, onPageActionClickListeners, important);

        int insertAt = mPageActionList.size();
        while (insertAt > 0 && mPageActionList.get(insertAt - 1).isImportant()) {
            insertAt--;
        }
        mPageActionList.add(insertAt, pageAction);

        ResourceDrawableUtils.getDrawable(mContext, imageData, new ResourceDrawableUtils.BitmapLoader() {
            @Override
            public void onBitmapFound(final Drawable d) {
                if (mPageActionList.contains(pageAction)) {
                    final Drawable icon;
                    if (useTint) {
                        final ColorStateList colorStateList = ContextCompat.getColorStateList(
                                getContext(), R.color.page_action_fg);
                        icon = DrawableUtil.tintDrawableWithStateList(d, colorStateList);
                    } else {
                        icon = d;
                    }
                    pageAction.setDrawable(icon);
                    refreshPageActionIcons();
                }
            }
        });
    }

    private void removePageAction(String id) {
        ThreadUtils.assertOnUiThread();

        final Iterator<PageAction> iter = mPageActionList.iterator();
        while (iter.hasNext()) {
            final PageAction pageAction = iter.next();
            if (pageAction.getID().equals(id)) {
                iter.remove();
                refreshPageActionIcons();
                return;
            }
        }
    }

    private ThemedImageButton createImageButton() {
        ThreadUtils.assertOnUiThread();

        final ToolbarRoundButton imageButton = new ToolbarRoundButton(mContext, null, R.style.UrlBar_ImageButton);
        final int width = mContext.getResources().getDimensionPixelSize(R.dimen.page_action_button_width);
        imageButton.setLayoutParams(new LayoutParams(width, LayoutParams.MATCH_PARENT));
        imageButton.setScaleType(ImageView.ScaleType.CENTER_INSIDE);
        imageButton.setOnClickListener(this);
        imageButton.setOnLongClickListener(this);
        return imageButton;
    }

    @Override
    public void onClick(View v) {
        String buttonClickedId = (String) v.getTag();
        if (buttonClickedId != null) {
            if (buttonClickedId.equals(MENU_BUTTON_KEY)) {
                showMenu(v, mPageActionList.size() - mMaxVisiblePageActions + 1);
            } else {
                getPageActionWithId(buttonClickedId).onClick();
            }
        }
    }

    @Override
    public boolean onLongClick(View v) {
        String buttonClickedId = (String) v.getTag();
        if (buttonClickedId.equals(MENU_BUTTON_KEY)) {
            showMenu(v, mPageActionList.size() - mMaxVisiblePageActions + 1);
            return true;
        } else {
            return getPageActionWithId(buttonClickedId).onLongClick();
        }
    }

    private void setActionForView(final ImageButton view, final PageAction pageAction) {
        ThreadUtils.assertOnUiThread();

        if (pageAction == null) {
            view.setTag(null);
            view.setImageDrawable(null);
            view.setVisibility(View.GONE);
            view.setContentDescription(null);
            return;
        }

        view.setTag(pageAction.getID());
        view.setImageDrawable(pageAction.getDrawable());
        view.setVisibility(View.VISIBLE);
        view.setContentDescription(pageAction.getTitle());
    }

    private void refreshPageActionIcons() {
        ThreadUtils.assertOnUiThread();

        final Resources resources = mContext.getResources();
        for (int i = 0; i < this.getChildCount(); i++) {
            final ImageButton v = (ImageButton) this.getChildAt(i);
            final PageAction pageAction = getPageActionForViewAt(i);

            // If there are more page actions than buttons, set the menu icon.
            // Otherwise, set the page action's icon if there is a page action.
            if ((i == this.getChildCount() - 1) && (mPageActionList.size() > mMaxVisiblePageActions)) {
                v.setTag(MENU_BUTTON_KEY);
                v.setImageDrawable(resources.getDrawable(R.drawable.icon_pageaction));
                v.setVisibility((pageAction != null) ? View.VISIBLE : View.GONE);
                v.setContentDescription(resources.getString(R.string.page_action_dropmarker_description));
            } else {
                setActionForView(v, pageAction);
            }

            if (v instanceof ThemedImageButton) {
                ((ThemedImageButton) v).setPrivateMode(isPrivateMode());
            }
        }
    }

    private PageAction getPageActionForViewAt(int index) {
        ThreadUtils.assertOnUiThread();

        /**
         * We show the user the most recent pageaction added since this keeps the user aware of any new page actions being added
         * Also, the order of the pageAction is important i.e. if a page action is added, instead of shifting the pagactions to the
         * left to make space for the new one, it would be more visually appealing to have the pageaction appear in the blank space.
         *
         * buttonIndex is needed for this reason because every new View added to PageActionLayout gets added to the right of its neighbouring View.
         * Hence the button on the very leftmost has the index 0. We want our pageactions to start from the rightmost
         * and hence we maintain the insertion order of the child Views which is essentially the reverse of their index
         */

        final int buttonIndex = (this.getChildCount() - 1) - index;

        if (mPageActionList.size() > buttonIndex) {
            // Return the pageactions starting from the end of the list for the number of visible pageactions.
            final int buttonCount = Math.min(mPageActionList.size(), getChildCount());
            return mPageActionList.get((mPageActionList.size() - buttonCount) + buttonIndex);
        }
        return null;
    }

    private PageAction getPageActionWithId(String id) {
        ThreadUtils.assertOnUiThread();

        for (PageAction pageAction : mPageActionList) {
            if (pageAction.getID().equals(id)) {
                return pageAction;
            }
        }
        return null;
    }

    private void showMenu(View pageActionButton, int toShow) {
        ThreadUtils.assertOnUiThread();

        if (mPageActionsMenu == null) {
            mPageActionsMenu = new GeckoPopupMenu(pageActionButton.getContext(), pageActionButton);
            mPageActionsMenu.inflate(0);
            mPageActionsMenu.setOnMenuItemClickListener(new GeckoPopupMenu.OnMenuItemClickListener() {
                @Override
                public boolean onMenuItemClick(MenuItem item) {
                    int id = item.getItemId();
                    for (int i = 0; i < mPageActionList.size(); i++) {
                        PageAction pageAction = mPageActionList.get(i);
                        if (pageAction.key() == id) {
                            pageAction.onClick();
                            return true;
                        }
                    }
                    return false;
                }
            });
        }
        Menu menu = mPageActionsMenu.getMenu();
        menu.clear();

        for (int i = 0; i < mPageActionList.size() && i < toShow; i++) {
            PageAction pageAction = mPageActionList.get(i);
            MenuItem item = menu.add(Menu.NONE, pageAction.key(), Menu.NONE, pageAction.getTitle());
            item.setIcon(pageAction.getDrawable());
        }
        mPageActionsMenu.show();
    }

    private static interface OnPageActionClickListeners {
        public void onClick(String id);

        public boolean onLongClick(String id);
    }

    private void hidePreviousConfirmPrompt() {
        if (mPwaConfirm != null) {
            if (ViewCompat.isAttachedToWindow(mPwaConfirm) || mPwaConfirm.getParent() != null) {
                mPwaConfirm.disappear();
            }
            mPwaConfirm = null;
        }
    }

    public static class PageAction {
        public static final String UUID_PAGE_ACTION_PWA = "279c269d-6397-4f86-a6d2-452e26456d4a";

        private final OnPageActionClickListeners mOnPageActionClickListeners;
        private Drawable mDrawable;
        private final String mTitle;
        private final String mId;
        private final int key;
        private final boolean mImportant;

        public PageAction(String id,
                          String title,
                          Drawable image,
                          OnPageActionClickListeners onPageActionClickListeners,
                          boolean important) {
            mId = id;
            mTitle = title;
            mDrawable = image;
            mOnPageActionClickListeners = onPageActionClickListeners;
            mImportant = important;

            key = UUID.fromString(mId.subSequence(1, mId.length() - 2).toString()).hashCode();
        }

        public Drawable getDrawable() {
            return mDrawable;
        }

        public void setDrawable(Drawable d) {
            mDrawable = d;
        }

        public String getTitle() {
            return mTitle;
        }

        public String getID() {
            return mId;
        }

        public int key() {
            return key;
        }

        public boolean isImportant() {
            return mImportant;
        }

        public void onClick() {
            if (mOnPageActionClickListeners != null) {
                mOnPageActionClickListeners.onClick(mId);
            }
        }

        public boolean onLongClick() {
            if (mOnPageActionClickListeners != null) {
                return mOnPageActionClickListeners.onLongClick(mId);
            }
            return false;
        }


    }
}
