/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sriram Ramasubramanian <sriram@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko;

import java.util.ArrayList;
import java.util.Iterator;

import android.app.Activity;
import android.content.Intent;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.Typeface;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.RelativeLayout;
import android.widget.TextView;

public class TabsTray extends Activity implements GeckoApp.OnTabsChangedListener {

    private ListView mList;
    private TabsAdapter mTabsAdapter;
    private boolean mWaitingForClose;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        setContentView(R.layout.tabs_tray);

        mWaitingForClose = false;

        mList = (ListView) findViewById(R.id.list);

        LinearLayout addTab = (LinearLayout) findViewById(R.id.add_tab);
        addTab.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                GeckoApp.mAppContext.addTab();
                finishActivity();
            }
        });

        // Adding a native divider for the add-tab
        LinearLayout lastDivider = new LinearLayout(this);
        lastDivider.setOrientation(LinearLayout.HORIZONTAL);
        lastDivider.setLayoutParams(new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, mList.getDividerHeight()));
        lastDivider.setBackgroundDrawable(mList.getDivider());
        addTab.addView(lastDivider, 0);
        
        LinearLayout container = (LinearLayout) findViewById(R.id.container);
        container.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                finishActivity();
            }
        });

        GeckoApp.registerOnTabsChangedListener(this);
        onTabsChanged(null);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        GeckoApp.unregisterOnTabsChangedListener(this);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        // This function is called after the initial list is populated
        // Scrolling to the selected tab can happen here
        if (hasFocus) {
            int position = mTabsAdapter.getPositionForTab(Tabs.getInstance().getSelectedTab());
            if (position != -1) 
                mList.smoothScrollToPosition(position);
        }
    } 
   
    public void onTabsChanged(Tab tab) {
        if (Tabs.getInstance().getCount() == 1)
            finishActivity();

        if (mTabsAdapter == null) {
            mTabsAdapter = new TabsAdapter(this, Tabs.getInstance().getTabsInOrder());
            mList.setAdapter(mTabsAdapter);
            return;
        }
        
        int position = mTabsAdapter.getPositionForTab(tab);
        if (position == -1)
            return;

        if (Tabs.getInstance().getIndexOf(tab) == -1) {
            mWaitingForClose = false;
            mTabsAdapter = new TabsAdapter(this, Tabs.getInstance().getTabsInOrder());
            mList.setAdapter(mTabsAdapter);
        } else {
            View view = mList.getChildAt(position - mList.getFirstVisiblePosition());
            mTabsAdapter.assignValues(view, tab);
        }
    }

    void finishActivity() {
        finish();
        overridePendingTransition(0, R.anim.shrink_fade_out);
    }

    // Adapter to bind tabs into a list 
    private class TabsAdapter extends BaseAdapter {
        public TabsAdapter(Context context, ArrayList<Tab> tabs) {
            mContext = context;
            mInflater = LayoutInflater.from(mContext);
            mTabs = new ArrayList<Tab>();

            if (tabs == null)
                return;

            for (int i = 0; i < tabs.size(); i++) {
                mTabs.add(tabs.get(i));
            }
            
            mOnInfoClickListener = new View.OnClickListener() {
                public void onClick(View v) {
                    GeckoAppShell.sendEventToGecko(new GeckoEvent("Tab:Select", v.getTag().toString()));
                    finishActivity();
                }
            };

            mOnCloseClickListener = new Button.OnClickListener() {
                public void onClick(View v) {
                    if (mWaitingForClose)
                        return;
                
                    mWaitingForClose = true;
               
                    String tabId = v.getTag().toString();
                    Tabs tabs = Tabs.getInstance();
                    Tab tab = tabs.getTab(Integer.parseInt(tabId));

                    if (tab == null)
                        return;
                
                    if (tabs.isSelectedTab(tab)) {
                        int index = tabs.getIndexOf(tab);
                        if (index >= 1)
                            index--;
                        else
                            index = 1;
                        int id = tabs.getTabAt(index).getId();
                        GeckoAppShell.sendEventToGecko(new GeckoEvent("Tab:Select", String.valueOf(id)));
                        GeckoAppShell.sendEventToGecko(new GeckoEvent("Tab:Close", tabId));
                    } else {
                        GeckoAppShell.sendEventToGecko(new GeckoEvent("Tab:Close", tabId));
                        GeckoAppShell.sendEventToGecko(new GeckoEvent("Tab:Select", String.valueOf(tabs.getSelectedTabId())));
                    }
                }
            };
        }

        @Override    
        public int getCount() {
            return mTabs.size();
        }
    
        @Override    
        public Tab getItem(int position) {
            return mTabs.get(position);
        }

        @Override    
        public long getItemId(int position) {
            return position;
        }

        public int getPositionForTab(Tab tab) {
            if (mTabs == null || tab == null)
                return -1;

            return mTabs.indexOf(tab);
        }

        public void assignValues(View view, Tab tab) {
            if (view == null || tab == null)
                return;

            ImageView favicon = (ImageView) view.findViewById(R.id.favicon);

            Drawable faviconImage = tab.getFavicon();
            if (faviconImage != null)
                favicon.setImageDrawable(faviconImage);
            else
                favicon.setImageResource(R.drawable.favicon);

            TextView title = (TextView) view.findViewById(R.id.title);
            title.setText(tab.getDisplayTitle());

            if (Tabs.getInstance().isSelectedTab(tab))
                title.setTypeface(title.getTypeface(), Typeface.BOLD);
        }

        @Override    
        public View getView(int position, View convertView, ViewGroup parent) {
            convertView = mInflater.inflate(R.layout.tabs_row, null);

            Tab tab = mTabs.get(position);

            RelativeLayout info = (RelativeLayout) convertView.findViewById(R.id.info);
            info.setTag(String.valueOf(tab.getId()));
            info.setOnClickListener(mOnInfoClickListener);

            assignValues(convertView, tab);
            
            ImageButton close = (ImageButton) convertView.findViewById(R.id.close);
            if (mTabs.size() > 1) {
                close.setTag(String.valueOf(tab.getId()));
                close.setOnClickListener(mOnCloseClickListener);
            } else {
                close.setVisibility(View.GONE);
            }

            return convertView;
        }

        @Override
        public void notifyDataSetChanged() {
        }

        @Override
        public void notifyDataSetInvalidated() {
        }
    
        private Context mContext;
        private ArrayList<Tab> mTabs;
        private LayoutInflater mInflater;
        private View.OnClickListener mOnInfoClickListener;
        private Button.OnClickListener mOnCloseClickListener;
    }
}
