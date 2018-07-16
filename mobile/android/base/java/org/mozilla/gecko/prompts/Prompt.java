/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.prompts;

import org.mozilla.gecko.R;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnClickListener;
import android.content.res.Resources;
import android.text.TextUtils;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.ScrollView;

import java.util.ArrayList;

public class Prompt implements OnClickListener, OnCancelListener, OnItemClickListener,
                               PromptInput.OnChangeListener, Tabs.OnTabsChangedListener {
    private static final String LOGTAG = "GeckoPromptService";

    private String[] mButtons;
    private PromptInput[] mInputs;
    private AlertDialog mDialog;
    private int mDoubleTapButtonType;

    private final LayoutInflater mInflater;
    private final Context mContext;
    private PromptCallback mCallback;
    private String mGuid;
    private PromptListAdapter mAdapter;

    private static boolean mInitialized;
    private static int mInputPaddingSize;

    private int mTabId = Tabs.INVALID_TAB_ID;
    private Object mPreviousInputValue = null;

    public Prompt(Context context, PromptCallback callback) {
        this(context);
        mCallback = callback;
    }

    private Prompt(Context context) {
        mContext = context;
        mInflater = LayoutInflater.from(mContext);

        if (!mInitialized) {
            Resources res = mContext.getResources();
            mInputPaddingSize = (int) (res.getDimension(R.dimen.prompt_service_inputs_padding));
            mInitialized = true;
        }
    }

    private View applyInputStyle(View view, PromptInput input) {
        // Don't add padding to color picker views
        if (input.canApplyInputStyle()) {
            view.setPadding(mInputPaddingSize, 0, mInputPaddingSize, 0);
        }
        return view;
    }

    public void show(GeckoBundle message) {
        String title = message.getString("title", "");
        String text = message.getString("text", "");
        mGuid = message.getString("guid", "");

        mButtons = message.getStringArray("buttons");
        final int buttonCount = mButtons == null ? 0 : mButtons.length;
        mDoubleTapButtonType = convertIndexToButtonType(message.getInt("doubleTapButton", -1), buttonCount);
        mPreviousInputValue = null;

        GeckoBundle[] inputs = message.getBundleArray("inputs");
        mInputs = new PromptInput[inputs != null ? inputs.length : 0];
        for (int i = 0; i < mInputs.length; i++) {
            mInputs[i] = PromptInput.getInput(inputs[i]);
            mInputs[i].setListener(this);
        }

        PromptListItem[] menuitems = PromptListItem.getArray(message.getBundleArray("listitems"));
        String selected = message.getString("choiceMode", "");

        int choiceMode = ListView.CHOICE_MODE_NONE;
        if ("single".equals(selected)) {
            choiceMode = ListView.CHOICE_MODE_SINGLE;
        } else if ("multiple".equals(selected)) {
            choiceMode = ListView.CHOICE_MODE_MULTIPLE;
        }

        mTabId = message.getInt("tabId", Tabs.INVALID_TAB_ID);

        show(title, text, menuitems, choiceMode);
    }

    private int convertIndexToButtonType(final int buttonIndex, final int buttonCount) {
        if (buttonIndex < 0 || buttonIndex >= buttonCount) {
            // All valid DialogInterface button values are < 0,
            // so we return 0 as an invalid value.
            return 0;
        }

        switch (buttonIndex) {
            case 0:
                return DialogInterface.BUTTON_POSITIVE;
            case 1:
                return DialogInterface.BUTTON_NEUTRAL;
            case 2:
                return DialogInterface.BUTTON_NEGATIVE;
            default:
                return 0;
        }
    }

    public void show(String title, String text, PromptListItem[] listItems, int choiceMode) {
        ThreadUtils.assertOnUiThread();

        try {
            create(title, text, listItems, choiceMode);
        } catch (IllegalStateException ex) {
            Log.i(LOGTAG, "Error building dialog", ex);
            return;
        }

        if (mTabId != Tabs.INVALID_TAB_ID) {
            Tabs.registerOnTabsChangedListener(this);

            final Tab tab = Tabs.getInstance().getTab(mTabId);
            if (Tabs.getInstance().getSelectedTab() == tab) {
                mDialog.show();
            }
        } else {
            mDialog.show();
        }
    }

    @Override
    public void onTabChanged(final Tab tab, final Tabs.TabEvents msg, final String data) {
        if (tab != Tabs.getInstance().getTab(mTabId)) {
            return;
        }

        switch (msg) {
            case SELECTED:
                Log.i(LOGTAG, "Selected");
                mDialog.show();
                break;
            case UNSELECTED:
                Log.i(LOGTAG, "Unselected");
                mDialog.hide();
                break;
            case LOCATION_CHANGE:
                Log.i(LOGTAG, "Location change");
                mDialog.cancel();
                break;
        }
    }

    private void create(String title, String text, PromptListItem[] listItems, int choiceMode)
            throws IllegalStateException {

        AlertDialog.Builder builder = new AlertDialog.Builder(mContext);
        if (!TextUtils.isEmpty(title)) {
            // Long strings can delay showing the dialog, so we cap the number of characters shown to 256.
            builder.setTitle(title.substring(0, Math.min(title.length(), 256)));
        }

        if (!TextUtils.isEmpty(text)) {
            builder.setMessage(text);
        }

        // Because lists are currently added through the normal Android AlertBuilder interface, they're
        // incompatible with also adding additional input elements to a dialog.
        if (listItems != null && listItems.length > 0) {
            addListItems(builder, listItems, choiceMode);
        } else if (!addInputs(builder)) {
            throw new IllegalStateException("Could not add inputs to dialog");
        }

        int length = mButtons == null ? 0 : mButtons.length;
        if (length > 0) {
            builder.setPositiveButton(mButtons[0], this);
            if (length > 1) {
                builder.setNeutralButton(mButtons[1], this);
                if (length > 2) {
                    builder.setNegativeButton(mButtons[2], this);
                }
            }
        }

        mDialog = builder.create();
        mDialog.setOnCancelListener(Prompt.this);
    }

    public void setButtons(String[] buttons) {
        mButtons = buttons;
    }

    public void setInputs(PromptInput[] inputs) {
        mInputs = inputs;
    }

    /* Adds to a result value from the lists that can be shown in dialogs.
     *  Will set the selected value(s) to the button attribute of the
     *  object that's passed in. If this is a multi-select dialog, sets a
     *  selected attribute to an array of booleans.
     */
    private void addListResult(final GeckoBundle result, int which) {
        if (mAdapter == null) {
            return;
        }

        // If the button has already been filled in
        final ArrayList<Integer> selected = mAdapter.getSelected();

        // If we haven't assigned a button yet, or we assigned it to -1, assign the which
        // parameter to both selected and the button.
        if (result.getInt("button", -1) == -1) {
            if (!selected.contains(which)) {
                selected.add(which);
            }

            result.putInt("button", which);
        }

        result.putIntArray("list", selected);
    }

    /* Adds to a result value from the inputs that can be shown in dialogs.
     * Each input will set its own value in the result.
     */
    private void addInputValues(final GeckoBundle result) {
        if (mInputs == null) {
            return;
        }

        for (final PromptInput input : mInputs) {
            if (input == null) {
                continue;
            }
            input.putInBundle(result);
        }
    }

    /* Adds the selected button to a result. This should only be called if there
     * are no lists shown on the dialog, since they also write their results to the button
     * attribute.
     */
    private void addButtonResult(final GeckoBundle result, int which) {
        int button = -1;
        switch (which) {
            case DialogInterface.BUTTON_POSITIVE : button = 0; break;
            case DialogInterface.BUTTON_NEUTRAL  : button = 1; break;
            case DialogInterface.BUTTON_NEGATIVE : button = 2; break;
        }
        result.putInt("button", button);
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        ThreadUtils.assertOnUiThread();
        closeDialog(which);
    }

    /* Adds a set of list items to the prompt. This can be used for either context menu type dialogs, checked lists,
     * or multiple selection lists.
     *
     * @param builder
     *        The alert builder currently building this dialog.
     * @param listItems
     *        The items to add.
     * @param choiceMode
     *        One of the ListView.CHOICE_MODE constants to designate whether this list shows checkmarks, radios buttons, or nothing.
    */
    private void addListItems(AlertDialog.Builder builder, PromptListItem[] listItems, int choiceMode) {
        switch (choiceMode) {
            case ListView.CHOICE_MODE_MULTIPLE_MODAL:
            case ListView.CHOICE_MODE_MULTIPLE:
                addMultiSelectList(builder, listItems);
                break;
            case ListView.CHOICE_MODE_SINGLE:
                addSingleSelectList(builder, listItems);
                break;
            case ListView.CHOICE_MODE_NONE:
            default:
                addMenuList(builder, listItems);
        }
    }

    /* Shows a multi-select list with checkmarks on the side. Android doesn't support using an adapter for
     * multi-choice lists by default so instead we insert our own custom list so that we can do fancy things
     * to the rows like disabling/indenting them.
     *
     * @param builder
     *        The alert builder currently building this dialog.
     * @param listItems
     *        The items to add.
     */
    private void addMultiSelectList(AlertDialog.Builder builder, PromptListItem[] listItems) {
        ListView listView = (ListView) mInflater.inflate(R.layout.select_dialog_list, null);
        listView.setOnItemClickListener(this);
        listView.setChoiceMode(ListView.CHOICE_MODE_MULTIPLE);

        mAdapter = new PromptListAdapter(mContext, R.layout.select_dialog_multichoice, listItems);
        listView.setAdapter(mAdapter);
        builder.setView(listView);
    }

    /* Shows a single-select list with radio boxes on the side.
     *
     * @param builder
     *        the alert builder currently building this dialog.
     * @param listItems
     *        The items to add.
     */
    private void addSingleSelectList(AlertDialog.Builder builder, PromptListItem[] listItems) {
        mAdapter = new PromptListAdapter(mContext, R.layout.select_dialog_singlechoice, listItems);
        builder.setSingleChoiceItems(mAdapter, mAdapter.getSelectedIndex(), new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                // The adapter isn't aware of single vs. multi choice lists, so manually
                // clear any other selected items first.
                ArrayList<Integer> selected = mAdapter.getSelected();
                for (Integer sel : selected) {
                    mAdapter.toggleSelected(sel);
                }

                // Now select this item.
                mAdapter.toggleSelected(which);
                closeIfNoButtons(which);
            }
        });
    }

    /* Shows a single-select list.
     *
     * @param builder
     *        the alert builder currently building this dialog.
     * @param listItems
     *        The items to add.
     */
    private void addMenuList(AlertDialog.Builder builder, PromptListItem[] listItems) {
        mAdapter = new PromptListAdapter(mContext, android.R.layout.simple_list_item_1, listItems);
        builder.setAdapter(mAdapter, this);
    }

    /* Wraps an input in a linearlayout. We do this so that we can set padding that appears outside the background
     * drawable for the view.
     */
    private View wrapInput(final PromptInput input) {
        final LinearLayout linearLayout = new LinearLayout(mContext);
        linearLayout.setOrientation(LinearLayout.VERTICAL);
        applyInputStyle(linearLayout, input);

        linearLayout.addView(input.getView(mContext));

        return linearLayout;
    }

    /* Add the requested input elements to the dialog.
     *
     * @param builder
     *        the alert builder currently building this dialog.
     * @return
     *         return true if the inputs were added successfully. This may fail
     *         if the requested input is compatible with this Android version.
     */
    private boolean addInputs(AlertDialog.Builder builder) {
        int length = mInputs == null ? 0 : mInputs.length;
        if (length == 0) {
            return true;
        }

        try {
            View root = null;
            boolean scrollable = false; // If any of the inputs are scrollable, we won't wrap this in a ScrollView

            if (length == 1) {
                root = wrapInput(mInputs[0]);
                scrollable |= mInputs[0].getScrollable();
            } else if (length > 1) {
                LinearLayout linearLayout = new LinearLayout(mContext);
                linearLayout.setOrientation(LinearLayout.VERTICAL);
                for (int i = 0; i < length; i++) {
                    View content = wrapInput(mInputs[i]);
                    linearLayout.addView(content);
                    scrollable |= mInputs[i].getScrollable();
                }
                root = linearLayout;
            }

            if (scrollable) {
                // If we're showing some sort of scrollable list, force an inverse background.
                builder.setInverseBackgroundForced(true);
                builder.setView(root);
            } else {
                ScrollView view = new ScrollView(mContext);
                view.addView(root);
                builder.setView(view);
            }
        } catch (Exception ex) {
            Log.e(LOGTAG, "Error showing prompt inputs", ex);
            // We cannot display these input widgets with this sdk version,
            // do not display any dialog and finish the prompt now.
            cancelDialog();
            return false;
        }

        return true;
    }

    /* AdapterView.OnItemClickListener
     * Called when a list item is clicked
     */
    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        ThreadUtils.assertOnUiThread();
        mAdapter.toggleSelected(position);

        // If there are no buttons on this dialog, then we take selecting an item as a sign to close
        // the dialog. Note that means it will be hard to select multiple things in this list, but
        // given there is no way to confirm+close the dialog, it seems reasonable.
        closeIfNoButtons(position);
    }

    private boolean closeIfNoButtons(int selected) {
        ThreadUtils.assertOnUiThread();
        if (mButtons == null || mButtons.length == 0) {
            closeDialog(selected);
            return true;
        }
        return false;
    }

    /* @DialogInterface.OnCancelListener
     * Called when the user hits back to cancel a dialog. The dialog will close itself when this
     * ends. Setup the correct return values here.
     *
     * @param aDialog
     *          A dialog interface for the dialog that's being closed.
     */
    @Override
    public void onCancel(DialogInterface aDialog) {
        ThreadUtils.assertOnUiThread();
        cancelDialog();
    }

    /* Called in situations where we want to cancel the dialog . This can happen if the user hits back,
     * or if the dialog can't be created because of invalid input.
     */
    private void cancelDialog() {
        final GeckoBundle ret = new GeckoBundle();
        ret.putInt("button", -1);
        addInputValues(ret);

        notifyClosing(ret);
    }

    /* Called any time we're closing the dialog to cleanup and notify listeners that the dialog
     * is closing.
     */
    private void closeDialog(int which) {
        final GeckoBundle ret = new GeckoBundle();
        mDialog.dismiss();

        addButtonResult(ret, which);
        addListResult(ret, which);
        addInputValues(ret);

        notifyClosing(ret);
    }

    /* Called any time we're closing the dialog to cleanup and notify listeners that the dialog
     * is closing.
     */
    private void notifyClosing(final GeckoBundle ret) {
        ret.putString("guid", mGuid);

        if (mTabId != Tabs.INVALID_TAB_ID) {
            Tabs.unregisterOnTabsChangedListener(this);
        }

        if (mCallback != null) {
            mCallback.onPromptFinished(ret);
        }
    }

    // Called when the prompt inputs on the dialog change
    @Override
    public void onChange(PromptInput input) {
        // If there are no buttons on this dialog, assuming that "changing" an input
        // means something was selected and we can close. This provides a way to tap
        // on a list item and close the dialog automatically.
        if (!closeIfNoButtons(-1)) {
            // Alternatively, if a default button has been specified for double tapping,
            // we want to close the dialog if the same input value has been transmitted
            // twice in a row.
            closeIfDoubleTapEnabled(input.getValue());
        }
    }

    private boolean closeIfDoubleTapEnabled(Object inputValue) {
        if (mDoubleTapButtonType != 0 && inputValue == mPreviousInputValue) {
            closeDialog(mDoubleTapButtonType);
            return true;
        }
        mPreviousInputValue = inputValue;
        return false;
    }

    public interface PromptCallback {
        /**
         * Called when the Prompt has been completed (i.e. when the user has selected an item or action in the Prompt).
         * This callback is run on the UI thread.
         */
        public void onPromptFinished(GeckoBundle result);
    }
}
