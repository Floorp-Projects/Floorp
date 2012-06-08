/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

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

            if (TextUtils.equals(viewName, "AboutHomeSection"))
                return new AboutHomeSection(context, attrs);
            else if (TextUtils.equals(viewName, "AwesomeBarTabs"))
                return new AwesomeBarTabs(context, attrs);
            else if (TextUtils.equals(viewName, "FormAssistPopup"))
                return new FormAssistPopup(context, attrs);
            else if (TextUtils.equals(viewName, "LinkTextView"))
                return new LinkTextView(context, attrs);
            else if (TextUtils.equals(viewName, "FindInPageBar"))
                return new FindInPageBar(context, attrs);
            else if (TextUtils.equals(viewName, "TabsPanel"))
                return new TabsPanel(context, attrs);
        }

        return null;
    }
}
