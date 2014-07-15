/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.R;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.widget.GeckoPopupMenu;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.util.AttributeSet;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;

import java.util.UUID;
import java.util.ArrayList;

public class PageActionLayout extends LinearLayout implements NativeEventListener,
                                                              View.OnClickListener,
                                                              View.OnLongClickListener {
    private static final String MENU_BUTTON_KEY = "MENU_BUTTON_KEY";
    private static final int DEFAULT_PAGE_ACTIONS_SHOWN = 2;

    private ArrayList<PageAction> mPageActionList;
    private GeckoPopupMenu mPageActionsMenu;
    private Context mContext;
    private LinearLayout mLayout;

    // By default it's two, can be changed by calling setNumberShown(int)
    private int mMaxVisiblePageActions;

    public PageActionLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mLayout = this;

        mPageActionList = new ArrayList<PageAction>();
        setNumberShown(DEFAULT_PAGE_ACTIONS_SHOWN);
        refreshPageActionIcons();

        EventDispatcher.getInstance().registerGeckoThreadListener(this,
            "PageActions:Add",
            "PageActions:Remove");
    }

    private void setNumberShown(int count) {
        ThreadUtils.assertOnUiThread();

        mMaxVisiblePageActions = count;

        for(int index = 0; index < count; index++) {
            if ((this.getChildCount() - 1) < index) {
                mLayout.addView(createImageButton());
            }
        }
    }

    public void onDestroy() {
        EventDispatcher.getInstance().unregisterGeckoThreadListener(this,
            "PageActions:Add",
            "PageActions:Remove");
    }

    @Override
    public void handleMessage(final String event, final NativeJSObject message, final EventCallback callback) {
        // NativeJSObject cannot be used off of the Gecko thread, so convert it to a Bundle.
        final Bundle bundle = message.toBundle();

        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                handleUiMessage(event, bundle);
            }
        });
    }

    private void handleUiMessage(final String event, final Bundle message) {
        ThreadUtils.assertOnUiThread();

        if (event.equals("PageActions:Add")) {
            final String id = message.getString("id");
            final String title = message.getString("title");
            final String imageURL = message.getString("icon", null);
            final boolean important = message.getBoolean("important");

            addPageAction(id, title, imageURL, new OnPageActionClickListeners() {
                @Override
                public void onClick(String id) {
                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("PageActions:Clicked", id));
                }

                @Override
                public boolean onLongClick(String id) {
                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("PageActions:LongClicked", id));
                    return true;
                }
            }, important);
        } else if (event.equals("PageActions:Remove")) {
            final String id = message.getString("id");

            removePageAction(id);
        }
    }

    private void addPageAction(final String id, final String title, final String imageData, final OnPageActionClickListeners mOnPageActionClickListeners, boolean mImportant) {
        ThreadUtils.assertOnUiThread();

        final PageAction pageAction = new PageAction(id, title, null, mOnPageActionClickListeners, mImportant);

        int insertAt = mPageActionList.size();
        while(insertAt > 0 && mPageActionList.get(insertAt-1).isImportant()) {
          insertAt--;
        }
        mPageActionList.add(insertAt, pageAction);

        BitmapUtils.getDrawable(mContext, imageData, new BitmapUtils.BitmapLoader() {
            @Override
            public void onBitmapFound(final Drawable d) {
                if (mPageActionList.contains(pageAction)) {
                    pageAction.setDrawable(d);
                    refreshPageActionIcons();
                }
            }
        });
    }

    private void removePageAction(String id) {
        ThreadUtils.assertOnUiThread();

        for(int i = 0; i < mPageActionList.size(); i++) {
            if (mPageActionList.get(i).getID().equals(id)) {
                mPageActionList.remove(i);
                refreshPageActionIcons();
                return;
            }
        }
    }

    private ImageButton createImageButton() {
        ThreadUtils.assertOnUiThread();

        ImageButton imageButton = new ImageButton(mContext, null, R.style.UrlBar_ImageButton_Icon);
        imageButton.setLayoutParams(new LayoutParams(mContext.getResources().getDimensionPixelSize(R.dimen.page_action_button_width), LayoutParams.MATCH_PARENT));
        imageButton.setScaleType(ImageView.ScaleType.CENTER_INSIDE);
        imageButton.setOnClickListener(this);
        imageButton.setOnLongClickListener(this);
        return imageButton;
    }

    @Override
    public void onClick(View v) {
        String buttonClickedId = (String)v.getTag();
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
        String buttonClickedId = (String)v.getTag();
        if (buttonClickedId.equals(MENU_BUTTON_KEY)) {
            showMenu(v, mPageActionList.size() - mMaxVisiblePageActions + 1);
            return true;
        } else {
            return getPageActionWithId(buttonClickedId).onLongClick();
        }
    }

    private void setActionForView(final ImageButton view, final PageAction pageAction) {
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
        for(int index = 0; index < this.getChildCount(); index++) {
            final ImageButton v = (ImageButton)this.getChildAt(index);
            final PageAction pageAction = getPageActionForViewAt(index);

            // If there are more pageactions then buttons, set the menu icon. Otherwise set the page action's icon if there is a page action.
            if (index == (this.getChildCount() - 1) && mPageActionList.size() > mMaxVisiblePageActions) {
                v.setTag(MENU_BUTTON_KEY);
                v.setImageDrawable(resources.getDrawable(R.drawable.icon_pageaction));
                v.setVisibility((pageAction != null) ? View.VISIBLE : View.GONE);
                v.setContentDescription(resources.getString(R.string.page_action_dropmarker_description));
            } else {
                setActionForView(v, pageAction);
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

        int buttonIndex = (this.getChildCount() - 1) - index;
        int totalVisibleButtons = ((mPageActionList.size() < this.getChildCount()) ? mPageActionList.size() : this.getChildCount());

        if (mPageActionList.size() > buttonIndex) {
            // Return the pageactions starting from the end of the list for the number of visible pageactions.
            return mPageActionList.get((mPageActionList.size() - totalVisibleButtons) + buttonIndex);
        }
        return null;
    }

    private PageAction getPageActionWithId(String id) {
        ThreadUtils.assertOnUiThread();

        for(int i = 0; i < mPageActionList.size(); i++) {
            PageAction pageAction = mPageActionList.get(i);
            if (pageAction.getID().equals(id)) {
                return pageAction;
            }
        }
        return null;
    }

    private void showMenu(View mPageActionButton, int toShow) {
        ThreadUtils.assertOnUiThread();

        if (mPageActionsMenu == null) {
            mPageActionsMenu = new GeckoPopupMenu(mPageActionButton.getContext(), mPageActionButton);
            mPageActionsMenu.inflate(0);
            mPageActionsMenu.setOnMenuItemClickListener(new GeckoPopupMenu.OnMenuItemClickListener() {
                @Override
                public boolean onMenuItemClick(MenuItem item) {
                    int id = item.getItemId();
                    for(int i = 0; i < mPageActionList.size(); i++) {
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

        for(int i = 0; i < mPageActionList.size(); i++) {
            if (i < toShow) {
                PageAction pageAction = mPageActionList.get(i);
                MenuItem item = menu.add(Menu.NONE, pageAction.key(), Menu.NONE, pageAction.getTitle());
                item.setIcon(pageAction.getDrawable());
            }
        }
        mPageActionsMenu.show();
    }

    private static interface OnPageActionClickListeners {
        public void onClick(String id);
        public boolean onLongClick(String id);
    }

    private static class PageAction {
        private OnPageActionClickListeners mOnPageActionClickListeners;
        private Drawable mDrawable;
        private String mTitle;
        private String mId;
        private int key;
        private boolean mImportant;

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
