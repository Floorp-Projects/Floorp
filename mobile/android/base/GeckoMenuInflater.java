/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.io.IOException;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import android.app.Activity;
import android.content.Context;
import android.content.res.TypedArray;
import android.content.res.XmlResourceParser;
import android.util.AttributeSet;
import android.util.Log;
import android.util.Xml;
import android.view.InflateException;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;

public class GeckoMenuInflater extends MenuInflater 
                               implements MenuItem.OnMenuItemClickListener {
    private static final String LOGTAG = "GeckoMenuInflater";

    private static final String TAG_ITEM = "item";
    private static final int NO_ID = 0;

    private Context mContext;

    // Private class to hold the parsed menu item. 
    private class ParsedItem {
        public int id;
        public int order;
        public CharSequence title;
        public int iconRes;
        public boolean checkable;
        public boolean checked;
        public boolean visible;
        public boolean enabled;
        public boolean showAsAction;
    }

    public GeckoMenuInflater(Context context) {
        super(context);
        mContext = context;
    }

    public void inflate(int menuRes, Menu menu) {

        // This is a very minimal parser for the custom menu.
        // This assumes that there is only one menu tag in the resource file.
        // This does not support sub-menus.

        XmlResourceParser parser = null;
        try {
            parser = mContext.getResources().getXml(menuRes);
            AttributeSet attrs = Xml.asAttributeSet(parser);

            ParsedItem item = null;
   
            String tag;
            int eventType = parser.getEventType();

            do {
                tag = parser.getName();
    
                switch (eventType) {
                    case XmlPullParser.START_TAG:
                        if (tag.equals(TAG_ITEM)) {
                            // Parse the menu item.
                            item = new ParsedItem();
                            parseItem(item, attrs);
                         }
                        break;
                        
                    case XmlPullParser.END_TAG:
                        if (parser.getName().equals(TAG_ITEM)) {
                            // Add the item.
                            MenuItem menuItem = menu.add(NO_ID, item.id, item.order, item.title);
                            setValues(item, menuItem);
                            menuItem.setOnMenuItemClickListener(this);
                        }
                        break;
                }

                eventType = parser.next();

            } while (eventType != XmlPullParser.END_DOCUMENT);

        } catch (XmlPullParserException e) {
            throw new InflateException("Error inflating menu XML", e);
        } catch (IOException e) {
            throw new InflateException("Error inflating menu XML", e);
        } finally {
            if (parser != null)
                parser.close();
        }
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        Activity activity = (Activity) mContext;
        boolean result = activity.onOptionsItemSelected(item);
        activity.closeOptionsMenu();
        return result;
    }
        
    public void parseItem(ParsedItem item, AttributeSet attrs) {
        TypedArray a = mContext.obtainStyledAttributes(attrs, R.styleable.MenuItem);

        item.id = a.getResourceId(R.styleable.MenuItem_id, NO_ID);
        item.order = a.getInt(R.styleable.MenuItem_orderInCategory, 0);
        item.title = a.getText(R.styleable.MenuItem_title);
        item.iconRes = a.getResourceId(R.styleable.MenuItem_icon, 0);
        item.checkable = a.getBoolean(R.styleable.MenuItem_checkable, false);
        item.checked = a.getBoolean(R.styleable.MenuItem_checked, false);
        item.visible = a.getBoolean(R.styleable.MenuItem_visible, true);
        item.enabled = a.getBoolean(R.styleable.MenuItem_enabled, true);
        item.showAsAction = a.getBoolean(R.styleable.MenuItem_showAsAction, false);

        a.recycle();
    }
        
    public void setValues(ParsedItem item, MenuItem menuItem) {
        menuItem.setChecked(item.checked)
                .setVisible(item.visible)
                .setEnabled(item.enabled)
                .setCheckable(item.checkable)
                .setIcon(item.iconRes)
                .setShowAsAction(item.showAsAction ? 1 : 0);
    }
}
