/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.preference.Preference;
import android.text.SpannableString;
import android.util.Log;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import java.util.Iterator;

import org.json.JSONException;
import org.json.JSONObject;

import org.mozilla.gecko.favicons.decoders.FaviconDecoder;
import org.mozilla.gecko.favicons.decoders.LoadFaviconResult;
import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.R;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.widget.FaviconView;

/**
 * Represents an element in the list of search engines on the preferences menu.
 */
public class SearchEnginePreference extends Preference implements View.OnLongClickListener {
    private static final String LOGTAG = "SearchEnginePreference";

    // Indices in button array of the AlertDialog of the three buttons.
    public static final int INDEX_SET_DEFAULT_BUTTON = 0;
    public static final int INDEX_REMOVE_BUTTON = 1;

    // Cache label to avoid repeated use of the resource system.
    public final String LABEL_IS_DEFAULT;

    // Specifies if this engine is configured as the default search engine.
    private boolean mIsDefaultEngine;

    // Dialog element labels.
    private String[] mDialogItems;

    // The popup displayed when this element is tapped.
    private AlertDialog mDialog;

    private final SearchPreferenceCategory mParentCategory;

    // The icon to display in the prompt when clicked.
    private BitmapDrawable mPromptIcon;
    // The bitmap backing the drawable above - needed separately for the FaviconView.
    private Bitmap mIconBitmap;

    private FaviconView mFaviconView;

    /**
     * Create a preference object to represent a search engine that is attached to category
     * containingCategory.
     * @param context The activity context we operate under.
     * @param parentCategory The PreferenceCategory this object exists within.
     * @see this.setSearchEngine
     */
    public SearchEnginePreference(Context context, SearchPreferenceCategory parentCategory) {
        super(context);
        mParentCategory = parentCategory;

        Resources res = getContext().getResources();

        // Set the layout resource for this preference - includes a FaviconView.
        setLayoutResource(R.layout.preference_search_engine);

        setOnPreferenceClickListener(new OnPreferenceClickListener() {
            @Override
            public boolean onPreferenceClick(Preference preference) {
                SearchEnginePreference sPref = (SearchEnginePreference) preference;
                sPref.showDialog();

                return true;
            }
        });

        // Fetch this resource now, instead of every time we ever want to relabel a button.
        LABEL_IS_DEFAULT = res.getString(R.string.pref_search_default);

        // Set up default dialog items.
        mDialogItems = new String[] { res.getString(R.string.pref_search_set_default),
                                      res.getString(R.string.pref_search_remove) };
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
    public boolean onLongClick(View view) {
        // Show the preference dialog on long-press.
        showDialog();
        return true;
    }

    /**
     * Configure this Preference object from the Gecko search engine JSON object.
     * @param geckoEngineJSON The Gecko-formatted JSON object representing the search engine.
     * @throws JSONException If the JSONObject is invalid.
     */
    public void setSearchEngineFromJSON(JSONObject geckoEngineJSON) throws JSONException {
        final String engineName = geckoEngineJSON.getString("name");
        final SpannableString titleSpannable = new SpannableString(engineName);

        setTitle(titleSpannable);

        final String iconURI = geckoEngineJSON.getString("iconURI");
        // Keep a reference to the bitmap - we'll need it later in onBindView.
        try {

            // Bug 961600: we shouldn't be doing all of this work. The favicon cache
            // should be used, and will give us the right size icon.

            LoadFaviconResult result = FaviconDecoder.decodeDataURI(iconURI);
            if (result == null) {
                // Nothing we can do.
                Log.w(LOGTAG, "Unable to decode icon URI.");
                return;
            }

            Iterator<Bitmap> bitmaps = result.getBitmaps();
            if (!bitmaps.hasNext()) {
                Log.w(LOGTAG, "No bitmaps in decoded icon.");
                return;
            }

            mIconBitmap = bitmaps.next();

            if (!bitmaps.hasNext()) {
                // We're done! There was only one, so this is as big as it gets.
                return;
            }

            // Find a bitmap of a more suitable size.
            final int desiredWidth;
            if (mFaviconView != null) {
                desiredWidth = mFaviconView.getWidth();
            } else {
                // sLargestFaviconSize is initialized when Favicons is attached to a
                // context, which occurs during GeckoApp.onCreate. That might not
                // ever happen (leaving it at 0), so we fall back.
                if (Favicons.sLargestFaviconSize == 0) {
                    desiredWidth = 128;
                } else {
                    desiredWidth = Favicons.sLargestFaviconSize;
                }
            }

            int currentWidth = mIconBitmap.getWidth();
            while ((currentWidth < desiredWidth) &&
                   bitmaps.hasNext()) {
                Bitmap b = bitmaps.next();
                if (b.getWidth() > currentWidth) {
                    currentWidth = b.getWidth();
                    mIconBitmap = b;
                }
            }

        } catch (IllegalArgumentException e) {
            Log.e(LOGTAG, "IllegalArgumentException creating Bitmap. Most likely a zero-length bitmap.", e);
        } catch (NullPointerException e) {
            Log.e(LOGTAG, "NullPointerException creating Bitmap. Most likely a zero-length bitmap.", e);
        }
    }

    /**
     * Set if this object's UI should show that this is the default engine. To ensure proper ordering,
     * this method should only be called after this Preference is added to the PreferenceCategory.
     * @param isDefault Flag indicating if this represents the default engine.
     */
    public void setIsDefaultEngine(boolean isDefault) {
        mIsDefaultEngine = isDefault;
        if (isDefault) {
            setOrder(0);
            setSummary(LABEL_IS_DEFAULT);
        } else {
            setOrder(1);
            setSummary("");
        }
    }

    /**
     * Display the AlertDialog providing options to reconfigure this search engine. Sets an event
     * listener to disable buttons in the dialog as appropriate after they have been constructed by
     * Android.
     * @see this.configureShownDialog
     * @see this.hideDialog
     */
    public void showDialog() {
        // If we are the only engine left, then we are the default engine, and none of the options
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

        final AlertDialog.Builder builder = new AlertDialog.Builder(getContext());
        builder.setTitle(getTitle().toString());
        builder.setItems(mDialogItems, new DialogInterface.OnClickListener() {
            // Forward the various events that we care about to the container class for handling.
            @Override
            public void onClick(DialogInterface dialog, int indexClicked) {
                hideDialog();
                switch (indexClicked) {
                    case INDEX_SET_DEFAULT_BUTTON:
                        mParentCategory.setDefault(SearchEnginePreference.this);
                        break;
                    case INDEX_REMOVE_BUTTON:
                        mParentCategory.uninstall(SearchEnginePreference.this);
                        break;
                    default:
                        Log.w(LOGTAG, "Selected index out of range.");
                        break;
                }
            }
        });

        // Copy the icon from this object to the prompt we produce. We lazily create the drawable,
        // as the user may not ever actually tap this object.
        if (mPromptIcon == null && mIconBitmap != null) {
            mPromptIcon = new BitmapDrawable(getContext().getResources(), mFaviconView.getBitmap());
        }
        builder.setIcon(mPromptIcon);

        // Icons are hidden until Bug 926711 is fixed.
        //builder.setIcon(mPromptIcon);

        // We have to construct the dialog itself on the UI thread.
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                mDialog = builder.create();
                mDialog.setOnShowListener(new DialogInterface.OnShowListener() {
                    // Called when the dialog is shown (so we're finally able to manipulate button enabledness).
                    @Override
                    public void onShow(DialogInterface dialog) {
                        configureShownDialog();
                    }
                });
                mDialog.show();
            }
        });
    }

    /**
     * Hide the dialog we previously created, if any.
     */
    public void hideDialog() {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                // Null check so we can chain engine-mutating methods up in SearchPreferenceCategory
                // without consequence.
                if (mDialog != null && mDialog.isShowing()) {
                    mDialog.dismiss();
                }
            }
        });
    }

    /**
     * Disables buttons in the shown AlertDialog as required. The button elements are not created
     * until after we call show, so this method has to be called from the onShowListener above.
     * @see this.showDialog
     */
    private void configureShownDialog() {
        // If we are the default engine, disable the "Set as default" button.
        final TextView defaultButton = (TextView) mDialog.getListView().getChildAt(INDEX_SET_DEFAULT_BUTTON);
        // Disable "Set as default" button if we are already the default.
        if (mIsDefaultEngine) {
            defaultButton.setEnabled(false);
            // Failure to unregister this listener leads to tapping the button dismissing the dialog
            // without doing anything.
            defaultButton.setOnClickListener(null);
        }
    }
}
