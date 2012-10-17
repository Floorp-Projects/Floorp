/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.LayerView;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;

public final class GeckoViewsFactory implements LayoutInflater.Factory {
    private static final String LOGTAG = "GeckoViewsFactory";

    private static final String GECKO_VIEW_IDENTIFIER = "org.mozilla.gecko.";
    private static final int GECKO_VIEW_IDENTIFIER_LENGTH = GECKO_VIEW_IDENTIFIER.length();

    private GeckoViewsFactory() { }

    // Making this a singleton class.
    private static final GeckoViewsFactory INSTANCE = new GeckoViewsFactory();

    public static GeckoViewsFactory getInstance() {
        return INSTANCE;
    }

    @Override
    public View onCreateView(String name, Context context, AttributeSet attrs) {
        if (!TextUtils.isEmpty(name) && name.startsWith(GECKO_VIEW_IDENTIFIER)) {
            String viewName = name.substring(GECKO_VIEW_IDENTIFIER_LENGTH);

            if (TextUtils.isEmpty(viewName))
                return null;
        
            Log.i(LOGTAG, "Creating custom Gecko view: " + viewName);

            if (TextUtils.equals(viewName, "AboutHomePromoBox"))
                return new AboutHomePromoBox(context, attrs);
            else if (TextUtils.equals(viewName, "AboutHomeContent"))
                return new AboutHomeContent(context, attrs);
            else if (TextUtils.equals(viewName, "AboutHomeContent$TopSitesGridView"))
                return new AboutHomeContent.TopSitesGridView(context, attrs);
            else if (TextUtils.equals(viewName, "AboutHomeContent$TopSitesThumbnail"))
                return new AboutHomeContent.TopSitesThumbnail(context, attrs);
            else if (TextUtils.equals(viewName, "AboutHomeSection"))
                return new AboutHomeSection(context, attrs);
            else if (TextUtils.equals(viewName, "AwesomeBarTabs"))
                return new AwesomeBarTabs(context, attrs);
            else if (TextUtils.equals(viewName, "BrowserToolbarBackground"))
                return new BrowserToolbarBackground(context, attrs);
            else if (TextUtils.equals(viewName, "FormAssistPopup"))
                return new FormAssistPopup(context, attrs);
            else if (TextUtils.equals(viewName, "GeckoApp$MainLayout"))
                return new GeckoApp.MainLayout(context, attrs);
            else if (TextUtils.equals(viewName, "LinkTextView"))
                return new LinkTextView(context, attrs);
            else if (TextUtils.equals(viewName, "FindInPageBar"))
                return new FindInPageBar(context, attrs);
            else if (TextUtils.equals(viewName, "MenuButton"))
                return new MenuButton(context, attrs);
            else if (TextUtils.equals(viewName, "TabsButton"))
                return new TabsButton(context, attrs);
            else if (TextUtils.equals(viewName, "TabsPanel"))
                return new TabsPanel(context, attrs);
            else if (TextUtils.equals(viewName, "TabsPanelButton"))
                return new TabsPanelButton(context, attrs);
            else if (TextUtils.equals(viewName, "TextSelectionHandle"))
                return new TextSelectionHandle(context, attrs);
            else if (TextUtils.equals(viewName, "gfx.LayerView"))
                return new LayerView(context, attrs);
            else
                Log.d(LOGTAG, "Warning: unknown custom view: " + viewName);
        }

        return null;
    }
}
