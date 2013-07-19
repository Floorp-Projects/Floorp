/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.widget.GeckoPopupMenu;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.util.Log;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;

import java.util.UUID;
import java.util.LinkedHashMap;

public class PageActionLayout extends LinearLayout implements GeckoEventListener,
                                                              View.OnClickListener,
                                                              View.OnLongClickListener {
    private final String LOGTAG = "GeckoPageActionLayout";
    private final String MENU_BUTTON_KEY = "MENU_BUTTON_KEY";
    private final int DEFAULT_PAGE_ACTIONS_SHOWN = 2;

    private LinkedHashMap<String, PageAction> mPageActionList;
    private GeckoPopupMenu mPageActionsMenu;
    private Context mContext;
    private LinearLayout mLayout;

    // By default it's two, can be changed by calling setNumberShown(int)
    private int mMaxVisiblePageActions;

    public PageActionLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mLayout = this;

        mPageActionList = new LinkedHashMap<String, PageAction>();
        setNumberShown(DEFAULT_PAGE_ACTIONS_SHOWN);
        refreshPageActionIcons();

        registerEventListener("PageActions:Add");
        registerEventListener("PageActions:Remove");
    }

    public void setNumberShown(int count) {
        mMaxVisiblePageActions = count;

        for(int index = 0; index < count; index++) {
            if ((this.getChildCount() - 1) < index) {
                mLayout.addView(createImageButton());
            }
        }
    }

    public void onDestroy() {
        unregisterEventListener("PageActions:Add");
        unregisterEventListener("PageActions:Remove");
    }

    protected void registerEventListener(String event) {
        GeckoAppShell.getEventDispatcher().registerEventListener(event, this);
    }

    protected void unregisterEventListener(String event) {
        GeckoAppShell.getEventDispatcher().unregisterEventListener(event, this);
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        try {
            if (event.equals("PageActions:Add")) {
                final String id = message.getString("id");
                final String title = message.getString("title");
                final String imageURL = message.optString("icon");

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
                });
            } else if (event.equals("PageActions:Remove")) {
                final String id = message.getString("id");

                removePageAction(id);
            }
        } catch(JSONException ex) {
            Log.e(LOGTAG, "Error deocding", ex);
        }
    }

    public void addPageAction(final String id, final String title, final String imageData, final OnPageActionClickListeners mOnPageActionClickListeners) {
        final PageAction pageAction = new PageAction(id, title, null, mOnPageActionClickListeners);
        mPageActionList.put(id, pageAction);

        BitmapUtils.getDrawable(mContext, imageData, new BitmapUtils.BitmapLoader() {
            @Override
            public void onBitmapFound(final Drawable d) {
                if (mPageActionList.containsKey(id)) {
                    pageAction.setDrawable(d);
                    refreshPageActionIcons();
                }
            }
        });
    }

    public void removePageAction(String id) {
        mPageActionList.remove(id);
        refreshPageActionIcons();
    }

    private ImageButton createImageButton() {
        ImageButton imageButton = new ImageButton(mContext, null, R.style.AddressBar_ImageButton_Icon);
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
                mPageActionList.get(buttonClickedId).onClick();
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
            return mPageActionList.get(buttonClickedId).onLongClick();
        }
    }

    private void refreshPageActionIcons() {
        final Resources resources = mContext.getResources();
        for(int index = 0; index < this.getChildCount(); index++) {
            final ImageButton v = (ImageButton)this.getChildAt(index);
            final PageAction pageAction = getPageActionForViewAt(index);

            if (index == (this.getChildCount() - 1)) {
                String id = (pageAction != null) ? pageAction.getID() : null;
                v.setTag((mPageActionList.size() > mMaxVisiblePageActions) ? MENU_BUTTON_KEY : id);
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run () {
                        // If there are more pageactions then buttons, set the menu icon. Otherwise set the page action's icon if there is a page action.
                        Drawable d = (pageAction != null) ? pageAction.getDrawable() : null;
                        v.setImageDrawable((mPageActionList.size() > mMaxVisiblePageActions) ? resources.getDrawable(R.drawable.icon_pageaction) : d);
                        v.setVisibility((pageAction != null) ? View.VISIBLE : View.GONE);
                    }
                });
            } else {
                v.setTag((pageAction != null) ? pageAction.getID() : null);
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run () {
                        v.setImageDrawable((pageAction != null) ? pageAction.getDrawable() : null);
                        v.setVisibility((pageAction != null) ? View.VISIBLE : View.GONE);
                    }
                });
            }
        }
    }

    private PageAction getPageActionForViewAt(int index) {
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
            return getPageActionAt((mPageActionList.size() - totalVisibleButtons) + buttonIndex);
        }
        return null;
    }

    private PageAction getPageActionAt(int index) {
        int count = 0;
        for(PageAction pageAction : mPageActionList.values()) {
            if (count == index) {
                return pageAction;
            }
            count++;
        }
        return null;
    }

    private void showMenu(View mPageActionButton, int toShow) {
        if (mPageActionsMenu == null) {
            mPageActionsMenu = new GeckoPopupMenu(mPageActionButton.getContext(), mPageActionButton);
            mPageActionsMenu.inflate(0);
            mPageActionsMenu.setOnMenuItemClickListener(new GeckoPopupMenu.OnMenuItemClickListener() {
                @Override
                public boolean onMenuItemClick(MenuItem item) {
                    for(PageAction pageAction : mPageActionList.values()) {
                        if (pageAction.key() == item.getItemId()) {
                            pageAction.onClick();
                        }
                    }
                    return true;
                }
            });
        }
        Menu menu = mPageActionsMenu.getMenu();
        menu.clear();

        int count = 0;
        for(PageAction pageAction : mPageActionList.values()) {
            if (count < toShow) {
                MenuItem item = menu.add(Menu.NONE, pageAction.key(), Menu.NONE, pageAction.getTitle());
                item.setIcon(pageAction.getDrawable());
            }
            count++;
        }
        mPageActionsMenu.show();
    }

    public static interface OnPageActionClickListeners {
        public void onClick(String id);
        public boolean onLongClick(String id);
    }

    private static class PageAction {
        private OnPageActionClickListeners mOnPageActionClickListeners;
        private Drawable mDrawable;
        private String mTitle;
        private String mId;
        private int key;

        public PageAction(String id, String title, Drawable image, OnPageActionClickListeners mOnPageActionClickListeners) {
            this.mId = id;
            this.mTitle = title;
            this.mDrawable = image;
            this.mOnPageActionClickListeners = mOnPageActionClickListeners;

            this.key = UUID.fromString(mId.subSequence(1, mId.length() - 2).toString()).hashCode();
        }

        public Drawable getDrawable() {
            return mDrawable;
        }

        public void setDrawable(Drawable d) {
            this.mDrawable = d;
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