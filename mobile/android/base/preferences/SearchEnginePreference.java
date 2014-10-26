/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.R;
import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.favicons.decoders.FaviconDecoder;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.widget.FaviconView;

import android.app.AlertDialog;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.text.SpannableString;
import android.util.Log;
import android.view.View;
import android.widget.Toast;

/**
 * Represents an element in the list of search engines on the preferences menu.
 */
public class SearchEnginePreference extends CustomListPreference {
    protected String LOGTAG = "SearchEnginePreference";

    protected static final int INDEX_REMOVE_BUTTON = 1;

    // The icon to display in the prompt when clicked.
    private BitmapDrawable mPromptIcon;

    // The bitmap backing the drawable above - needed separately for the FaviconView.
    private Bitmap mIconBitmap;

    private FaviconView mFaviconView;

    // Search engine identifier specified by the gecko search service. This will be "other"
    // for engines that are not shipped with the app.
    private String mIdentifier;

    public SearchEnginePreference(Context context, SearchPreferenceCategory parentCategory) {
        super(context, parentCategory);
    }

    /**
     * Called by Android when we're bound to the custom view. Allows us to set the custom properties
     * of our custom view elements as we desire (We can now use findViewById on them).
     *
     * @param view The view instance for this Preference object.
     */
    @Override
    protected void onBindView(View view) {
        super.onBindView(view);

        // Set the icon in the FaviconView.
        mFaviconView = ((FaviconView) view.findViewById(R.id.search_engine_icon));
        mFaviconView.updateAndScaleImage(mIconBitmap, getTitle().toString());
    }

    @Override
    protected int getPreferenceLayoutResource() {
        return R.layout.preference_search_engine;
    }

    /**
     * Returns the strings to be displayed in the dialog.
     */
    @Override
    protected String[] createDialogItems() {
        return new String[] { LABEL_SET_AS_DEFAULT,
                              LABEL_REMOVE };
    }

    @Override
    public void showDialog() {
        // If this is the last engine, then we are the default, and none of the options
        // on this menu can do anything.
        if (mParentCategory.getPreferenceCount() == 1) {
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    Toast.makeText(getContext(), R.string.pref_search_last_toast, Toast.LENGTH_SHORT).show();
                }
            });
            return;
        }

        super.showDialog();
    }

    @Override
    protected void configureDialogBuilder(AlertDialog.Builder builder) {
        // Copy the icon from this object to the prompt we produce. We lazily create the drawable,
        // as the user may not ever actually tap this object.
        if (mPromptIcon == null && mIconBitmap != null) {
            mPromptIcon = new BitmapDrawable(getContext().getResources(), mFaviconView.getBitmap());
        }

        builder.setIcon(mPromptIcon);
    }

    @Override
    protected void onDialogIndexClicked(int index) {
        switch (index) {
            case INDEX_SET_DEFAULT_BUTTON:
                mParentCategory.setDefault(this);
                break;

            case INDEX_REMOVE_BUTTON:
                mParentCategory.uninstall(this);
                break;

            default:
                Log.w(LOGTAG, "Selected index out of range.");
                break;
        }
    }

    /**
     * @return Identifier of built-in search engine, or "other" if engine is not built-in.
     */
    public String getIdentifier() {
        return mIdentifier;
    }

    /**
     * Configure this Preference object from the Gecko search engine JSON object.
     * @param geckoEngineJSON The Gecko-formatted JSON object representing the search engine.
     * @throws JSONException If the JSONObject is invalid.
     */
    public void setSearchEngineFromJSON(JSONObject geckoEngineJSON) throws JSONException {
        mIdentifier = geckoEngineJSON.getString("identifier");

        // A null JS value gets converted into a string.
        if (mIdentifier.equals("null")) {
            mIdentifier = "other";
        }

        final String engineName = geckoEngineJSON.getString("name");
        final SpannableString titleSpannable = new SpannableString(engineName);

        setTitle(titleSpannable);

        final String iconURI = geckoEngineJSON.getString("iconURI");
        // Keep a reference to the bitmap - we'll need it later in onBindView.
        try {
            final int desiredWidth;
            if (mFaviconView != null) {
                desiredWidth = mFaviconView.getWidth();
            } else {
                // largestFaviconSize is initialized when Favicons is attached to a
                // context, which occurs during GeckoApp.onCreate. That might not
                // ever happen (leaving it at 0), so we fall back.
                if (Favicons.largestFaviconSize == 0) {
                    desiredWidth = 128;
                } else {
                    desiredWidth = Favicons.largestFaviconSize;
                }
            }

            // TODO: use the cache. Bug 961600.
            mIconBitmap = FaviconDecoder.getMostSuitableBitmapFromDataURI(iconURI, desiredWidth);

        } catch (IllegalArgumentException e) {
            Log.e(LOGTAG, "IllegalArgumentException creating Bitmap. Most likely a zero-length bitmap.", e);
        } catch (NullPointerException e) {
            Log.e(LOGTAG, "NullPointerException creating Bitmap. Most likely a zero-length bitmap.", e);
        }
    }
}
