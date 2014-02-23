/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import org.mozilla.gecko.R;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.Resources;
import android.preference.Preference;
import android.view.View;
import android.widget.TextView;

/**
 * Represents an element in a <code>CustomListCategory</code> preference menu.
 * This preference con display a dialog when clicked, and also supports
 * being set as a default item within the preference list category.
 */

public abstract class CustomListPreference extends Preference implements View.OnLongClickListener {
    protected String LOGTAG = "CustomListPreference";

    // Indices of the buttons of the Dialog.
    public static final int INDEX_SET_DEFAULT_BUTTON = 0;

    // Dialog item labels.
    protected final String[] mDialogItems;

    // Dialog displayed when this element is tapped.
    protected AlertDialog mDialog;

    // Cache label to avoid repeated use of the resource system.
    public final String LABEL_IS_DEFAULT;
    public final String LABEL_SET_AS_DEFAULT;

    protected boolean mIsDefault;

    // Enclosing parent category that contains this preference.
    protected final CustomListCategory mParentCategory;

    /**
     * Create a preference object to represent a list preference that is attached to
     * a category.
     *
     * @param context The activity context we operate under.
     * @param parentCategory The PreferenceCategory this object exists within.
     */
    public CustomListPreference(Context context, CustomListCategory parentCategory) {
        super(context);

        mParentCategory = parentCategory;
        setLayoutResource(getPreferenceLayoutResource());

        setOnPreferenceClickListener(new OnPreferenceClickListener() {
            @Override
            public boolean onPreferenceClick(Preference preference) {
                CustomListPreference sPref = (CustomListPreference) preference;
                sPref.showDialog();
                return true;
            }
        });

        Resources res = getContext().getResources();

        // Fetch these strings now, instead of every time we ever want to relabel a button.
        LABEL_IS_DEFAULT = res.getString(R.string.pref_default);
        LABEL_SET_AS_DEFAULT = res.getString(R.string.pref_dialog_set_default);

        mDialogItems = getDialogStrings();
    }

    /**
     * Returns the Android resource id for the layout.
     */
    protected abstract int getPreferenceLayoutResource();

    /**
     * Set whether this object's UI should display this as the default item.
     * Note: This must be called from the UI thread because it touches the view hierarchy.
     *
     * To ensure proper ordering, this method should only be called after this Preference
     * is added to the PreferenceCategory.
     *
     * @param isDefault Flag indicating if this represents the default list item.
     */
    public void setIsDefault(boolean isDefault) {
        mIsDefault = isDefault;
        if (isDefault) {
            setOrder(0);
            setSummary(LABEL_IS_DEFAULT);
        } else {
            setOrder(1);
            setSummary("");
        }
    }

    /**
     * Returns the strings to be displayed in the dialog.
     */
    abstract protected String[] getDialogStrings();

    /**
     * Display a dialog for this preference, when the preference is clicked.
     */
    public void showDialog() {
        final AlertDialog.Builder builder = new AlertDialog.Builder(getContext());
        builder.setTitle(getTitle().toString());
        builder.setItems(mDialogItems, new DialogInterface.OnClickListener() {
            // Forward relevant events to the container class for handling.
            @Override
            public void onClick(DialogInterface dialog, int indexClicked) {
                hideDialog();
                onDialogIndexClicked(indexClicked);
            }
        });

        configureDialogBuilder(builder);

        // We have to construct the dialog itself on the UI thread.
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

    /**
     * (Optional) Configure the AlertDialog builder.
     */
    protected void configureDialogBuilder(AlertDialog.Builder builder) {
        return;
    }

    abstract protected void onDialogIndexClicked(int index);

    /**
     * Disables buttons in the shown AlertDialog as required. The button elements are not created
     * until after show is called, so this method has to be called from the onShowListener above.
     * @see this.showDialog
     */
    protected void configureShownDialog() {
        // If this is already the default list item, disable the button for setting this as the default.
        final TextView defaultButton = (TextView) mDialog.getListView().getChildAt(INDEX_SET_DEFAULT_BUTTON);
        if (mIsDefault) {
            defaultButton.setEnabled(false);

            // Failure to unregister this listener leads to tapping the button dismissing the dialog
            // without doing anything.
            defaultButton.setOnClickListener(null);
        }
    }

    /**
     * Hide the dialog we previously created, if any.
     */
    public void hideDialog() {
        if (mDialog != null && mDialog.isShowing()) {
            mDialog.dismiss();
        }
    }

    @Override
    public boolean onLongClick(View view) {
        // Show the preference dialog on long-press.
        showDialog();
        return true;
    }
}
