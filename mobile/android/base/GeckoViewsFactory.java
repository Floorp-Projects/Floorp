/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.widget.AboutHomeView;
import org.mozilla.gecko.widget.AddonsSection;
import org.mozilla.gecko.widget.IconTabWidget;
import org.mozilla.gecko.widget.LastTabsSection;
import org.mozilla.gecko.widget.LinkTextView;
import org.mozilla.gecko.widget.PromoBox;
import org.mozilla.gecko.widget.RemoteTabsSection;
import org.mozilla.gecko.widget.TabRow;
import org.mozilla.gecko.widget.ThumbnailView;
import org.mozilla.gecko.widget.TopSitesView;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;

import java.lang.reflect.Constructor;
import java.util.HashMap;
import java.util.Map;

public final class GeckoViewsFactory implements LayoutInflater.Factory {
    private static final String LOGTAG = "GeckoViewsFactory";

    private static final String GECKO_VIEW_IDENTIFIER = "org.mozilla.gecko.";
    private static final int GECKO_VIEW_IDENTIFIER_LENGTH = GECKO_VIEW_IDENTIFIER.length();

    private static final String GECKO_IDENTIFIER = "Gecko.";
    private static final int GECKO_IDENTIFIER_LENGTH = GECKO_IDENTIFIER.length();

    private final Map<String, Constructor<? extends View>> mFactoryMap;

    private GeckoViewsFactory() {
        // initialize the hashmap to a capacity that is a prime number greater than
        // (size * 4/3). The size is the number of items we expect to put in it, and
        // 4/3 is the inverse of the default load factor.
        mFactoryMap = new HashMap<String, Constructor<? extends View>>(53);
        Class<Context> arg1Class = Context.class;
        Class<AttributeSet> arg2Class = AttributeSet.class;
        try {
            mFactoryMap.put("AboutHomeView", AboutHomeView.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("AddonsSection", AddonsSection.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("LastTabsSection", LastTabsSection.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("PromoBox", PromoBox.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("RemoteTabsSection", RemoteTabsSection.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("TopSitesView", TopSitesView.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("AwesomeBarTabs", AwesomeBarTabs.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("AwesomeBarTabs$BackgroundLayout", AwesomeBarTabs.BackgroundLayout.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("BackButton", BackButton.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("BrowserToolbarBackground", BrowserToolbarBackground.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("BrowserToolbar$RightEdge", BrowserToolbar.RightEdge.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("CheckableLinearLayout", CheckableLinearLayout.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("FormAssistPopup", FormAssistPopup.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("ForwardButton", ForwardButton.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("GeckoApp$MainLayout", GeckoApp.MainLayout.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("LinkTextView", LinkTextView.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("MenuItemDefault", MenuItemDefault.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("FindInPageBar", FindInPageBar.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("IconTabWidget", IconTabWidget.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("RemoteTabs", RemoteTabs.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("ShapedButton", ShapedButton.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("TabRow", TabRow.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("TabsPanel", TabsPanel.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("TabsPanel$TabsListContainer", TabsPanel.TabsListContainer.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("TabsPanel$TabsPanelToolbar", TabsPanel.TabsPanelToolbar.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("TabsTray", TabsTray.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("ThumbnailView", ThumbnailView.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("TextSelectionHandle", TextSelectionHandle.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("gfx.LayerView", LayerView.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("AllCapsTextView", AllCapsTextView.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("Button", GeckoButton.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("EditText", GeckoEditText.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("FrameLayout", GeckoFrameLayout.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("ImageButton", GeckoImageButton.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("ImageView", GeckoImageView.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("LinearLayout", GeckoLinearLayout.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("RelativeLayout", GeckoRelativeLayout.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("TextSwitcher", GeckoTextSwitcher.class.getConstructor(arg1Class, arg2Class));
            mFactoryMap.put("TextView", GeckoTextView.class.getConstructor(arg1Class, arg2Class));
        } catch (NoSuchMethodException nsme) {
            Log.e(LOGTAG, "Unable to initialize views factory", nsme);
        }
    }

    // Making this a singleton class.
    private static final GeckoViewsFactory INSTANCE = new GeckoViewsFactory();

    public static GeckoViewsFactory getInstance() {
        return INSTANCE;
    }

    @Override
    public View onCreateView(String name, Context context, AttributeSet attrs) {
        if (!TextUtils.isEmpty(name)) {
            String viewName = null;

            if (name.startsWith(GECKO_VIEW_IDENTIFIER))
                viewName = name.substring(GECKO_VIEW_IDENTIFIER_LENGTH);
            else if (name.startsWith(GECKO_IDENTIFIER))
                viewName = name.substring(GECKO_IDENTIFIER_LENGTH);
            else
                return null;

            Constructor<? extends View> constructor = mFactoryMap.get(viewName);
            if (constructor != null) {
                try {
                    return constructor.newInstance(context, attrs);
                } catch (Exception e) {
                    Log.e(LOGTAG, "Unable to instantiate view " + name, e);
                    return null;
                }
            }

            Log.d(LOGTAG, "Warning: unknown custom view: " + name);
        }

        return null;
    }
}
