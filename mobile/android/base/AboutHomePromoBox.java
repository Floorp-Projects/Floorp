/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.sync.setup.activities.SetupSyncActivity;

import org.json.JSONObject;

import android.content.Context;
import android.content.Intent;
import android.os.SystemClock;
import android.text.SpannableString;
import android.text.style.StyleSpan;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

/**
 * A promotional box for the about:home page. The layout contains an ImageView to the left of a
 * TextView whose resources may be overidden to display custom values for a new type of promo box.
 * To do this, add a new Type value and update show() to call setResources() for your values -
 * including a set[Box Type]Resources() helper method is recommended.
 */
public class AboutHomePromoBox extends TextView implements View.OnClickListener {
    private static final String LOGTAG = "AboutHomePromoBox";

    public enum Type { NONE, SYNC, APPS };

    private Type mType;

    private final Context mContext;

    // Use setResources() to set these variables for each PromoBox type.
    private int mTextResource;
    private int mBoldTextResource;
    private int mImageResource;

    public AboutHomePromoBox(Context context, AttributeSet attrs) {
        super(context, attrs);

        mContext = context;
        setOnClickListener(this);
    }

    @Override
    public void onClick(View v) {
        switch (mType) {
            case SYNC:
                final Context context = v.getContext();
                final Intent intent = new Intent(context, SetupSyncActivity.class);
                context.startActivity(intent);
                break;

            case APPS:
                Tabs.getInstance().loadUrl("https://marketplace.mozilla.org", Tabs.LOADURL_NEW_TAB);
                break;

            default:
                Log.e(LOGTAG, "Invalid type was set when promo box was clicked.");
                break;
        }
    }

    /**
     * Shows the specified promo box. If a promo box is already active, it will be overidden with a
     * promo box of the specified type.
     */
    public void show(Type type) {
        mType = type;
        switch (type) {
            case SYNC:
                setSyncResources();
                break;

            case APPS:
                setAppsResources();
                break;

            default:
                Log.e(LOGTAG, "show() - Invalid AboutHomePromoBox.Type specified.");
                hide();
                return;
        }
        updateViewResources();
        setVisibility(View.VISIBLE);
    }

    public void hide() {
        setVisibility(View.GONE);
        mType = null;
    }

    private void setResources(int textResource, int boldTextResource, int imageResource) {
        mTextResource = textResource;
        mBoldTextResource = boldTextResource;
        mImageResource = imageResource;
    }

    private void updateViewResources() {
        updateTextViewResources();
        setCompoundDrawablesWithIntrinsicBounds(mImageResource, 0, 0, 0);
    }

    private void updateTextViewResources() {
        final String promoText = mContext.getResources().getString(mTextResource);
        final String boldName = mContext.getResources().getString(mBoldTextResource);
        final int styleIndex = promoText.indexOf(boldName);
        if (styleIndex < 0)
            setText(promoText);
        else {
            final SpannableString spannableText = new SpannableString(promoText);
            spannableText.setSpan(new StyleSpan(android.graphics.Typeface.BOLD), styleIndex,
                    styleIndex + boldName.length(), 0);
            setText(spannableText, TextView.BufferType.SPANNABLE);
        }
    }

    // Type.SYNC: Setup Firefox sync.
    private void setSyncResources() {
        setResources(R.string.abouthome_about_sync, R.string.abouthome_sync_bold_name,
                R.drawable.abouthome_promo_logo_sync);
    }

    // Types.APPS: Visit the Marketplace.
    private void setAppsResources() {
        setResources(R.string.abouthome_about_apps, R.string.abouthome_apps_bold_name,
                R.drawable.abouthome_promo_logo_apps);
    }
}
