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

    private static final String GECKO_IDENTIFIER = "Gecko.";
    private static final int GECKO_IDENTIFIER_LENGTH = GECKO_IDENTIFIER.length();

    private GeckoViewsFactory() { }

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

            if (TextUtils.isEmpty(viewName))
                return null;
        
            Log.i(LOGTAG, "Creating custom Gecko view: " + viewName);

            if (TextUtils.equals(viewName, "AboutHomePromoBox"))
                return new AboutHomePromoBox(context, attrs);
            else if (TextUtils.equals(viewName, "AboutHomeContent"))
                return new AboutHomeContent(context, attrs);
            else if (TextUtils.equals(viewName, "AboutHomeContent$TopSitesGridView"))
                return new AboutHomeContent.TopSitesGridView(context, attrs);
            else if (TextUtils.equals(viewName, "AboutHomeSection"))
                return new AboutHomeSection(context, attrs);
            else if (TextUtils.equals(viewName, "AwesomeBarTabs"))
                return new AwesomeBarTabs(context, attrs);
            else if (TextUtils.equals(viewName, "AwesomeBarTabs.Background"))
                return new AwesomeBarTabs.Background(context, attrs);
            else if (TextUtils.equals(viewName, "BackButton"))
                return new BackButton(context, attrs);
            else if (TextUtils.equals(viewName, "BrowserToolbarBackground"))
                return new BrowserToolbarBackground(context, attrs);
            else if (TextUtils.equals(viewName, "BrowserToolbar$RightEdge"))
                return new BrowserToolbar.RightEdge(context, attrs);
            else if (TextUtils.equals(viewName, "FormAssistPopup"))
                return new FormAssistPopup(context, attrs);
            else if (TextUtils.equals(viewName, "ForwardButton"))
                return new ForwardButton(context, attrs);
            else if (TextUtils.equals(viewName, "GeckoApp$MainLayout"))
                return new GeckoApp.MainLayout(context, attrs);
            else if (TextUtils.equals(viewName, "LinkTextView"))
                return new LinkTextView(context, attrs);
            else if (TextUtils.equals(viewName, "FindInPageBar"))
                return new FindInPageBar(context, attrs);
            else if (TextUtils.equals(viewName, "MenuButton"))
                return new MenuButton(context, attrs);
            else if (TextUtils.equals(viewName, "RemoteTabs"))
                return new RemoteTabs(context, attrs);
            else if (TextUtils.equals(viewName, "TabsButton"))
                return new TabsButton(context, attrs);
            else if (TextUtils.equals(viewName, "TabsPanel"))
                return new TabsPanel(context, attrs);
            else if (TextUtils.equals(viewName, "TabsTray"))
                return new TabsTray(context, attrs);
            else if (TextUtils.equals(viewName, "TextSelectionHandle"))
                return new TextSelectionHandle(context, attrs);
            else if (TextUtils.equals(viewName, "gfx.LayerView"))
                return new LayerView(context, attrs);
            else if (TextUtils.equals(viewName, "AllCapsTextView"))
                return new AllCapsTextView(context, attrs);
            else if (TextUtils.equals(viewName, "Button"))
                return new GeckoButton(context, attrs);
            else if (TextUtils.equals(viewName, "EditText"))
                return new GeckoEditText(context, attrs);
            else if (TextUtils.equals(viewName, "FrameLayout"))
                return new GeckoFrameLayout(context, attrs);
            else if (TextUtils.equals(viewName, "ImageButton"))
                return new GeckoImageButton(context, attrs);
            else if (TextUtils.equals(viewName, "ImageView"))
                return new GeckoImageView(context, attrs);
            else if (TextUtils.equals(viewName, "LinearLayout"))
                return new GeckoLinearLayout(context, attrs);
            else if (TextUtils.equals(viewName, "RelativeLayout"))
                return new GeckoRelativeLayout(context, attrs);
            else if (TextUtils.equals(viewName, "TextSwitcher"))
                return new GeckoTextSwitcher(context, attrs);
            else if (TextUtils.equals(viewName, "TextView"))
                return new GeckoTextView(context, attrs);
            else
                Log.d(LOGTAG, "Warning: unknown custom view: " + viewName);
        }

        return null;
    }
}
