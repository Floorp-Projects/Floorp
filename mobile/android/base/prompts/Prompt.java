/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.prompts;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.R;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnClickListener;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.CheckedTextView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.ScrollView;
import android.widget.TextView;

import java.util.ArrayList;

public class Prompt implements OnClickListener, OnCancelListener, OnItemClickListener,
                               PromptInput.OnChangeListener {
    private static final String LOGTAG = "GeckoPromptService";

    private String[] mButtons;
    private PromptInput[] mInputs;
    private AlertDialog mDialog;

    private final LayoutInflater mInflater;
    private final Context mContext;
    private PromptCallback mCallback;
    private String mGuid;
    private PromptListAdapter mAdapter;

    private static boolean mInitialized;
    private static int mInputPaddingSize;

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

    public void show(JSONObject message) {
        processMessage(message);
    }


    public void show(String title, String text, PromptListItem[] listItems, int choiceMode) {
        ThreadUtils.assertOnUiThread();

        GeckoAppShell.getLayerView().abortPanning();

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
            // If we failed to add any requested input elements, don't show the dialog
            return;
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
        mDialog.show();
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
    private void addListResult(final JSONObject result, int which) {
        if (mAdapter == null) {
            return;
        }

        try {
            JSONArray selected = new JSONArray();

            // If the button has already been filled in
            ArrayList<Integer> selectedItems = mAdapter.getSelected();
            for (Integer item : selectedItems) {
                selected.put(item);
            }

            // If we haven't assigned a button yet, or we assigned it to -1, assign the which
            // parameter to both selected and the button.
            if (!result.has("button") || result.optInt("button") == -1) {
                if (!selectedItems.contains(which)) {
                    selected.put(which);
                }

                result.put("button", which);
            }

            result.put("list", selected);
        } catch(JSONException ex) { }
    }

    /* Adds to a result value from the inputs that can be shown in dialogs.
     * Each input will set its own value in the result.
     */
    private void addInputValues(final JSONObject result) {
        try {
            if (mInputs != null) {
                for (int i = 0; i < mInputs.length; i++) {
                    result.put(mInputs[i].getId(), mInputs[i].getValue());
                }
            }
        } catch(JSONException ex) { }
    }

    /* Adds the selected button to a result. This should only be called if there
     * are no lists shown on the dialog, since they also write their results to the button
     * attribute.
     */
    private void addButtonResult(final JSONObject result, int which) {
        int button = -1;
        switch(which) {
            case DialogInterface.BUTTON_POSITIVE : button = 0; break;
            case DialogInterface.BUTTON_NEUTRAL  : button = 1; break;
            case DialogInterface.BUTTON_NEGATIVE : button = 2; break;
        }
        try {
            result.put("button", button);
        } catch(JSONException ex) { }
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
        switch(choiceMode) {
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
     *         if the requested input is compatible with this Android verison
     */
    private boolean addInputs(AlertDialog.Builder builder) {
        int length = mInputs == null ? 0 : mInputs.length;
        if (length == 0) {
            return true;
        }

        try {
            View root = null;
            boolean scrollable = false; // If any of the innuts are scrollable, we won't wrap this in a ScrollView

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
        } catch(Exception ex) {
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
     *  or if the dialog can't be created because of invalid JSON.
     */
    private void cancelDialog() {
        JSONObject ret = new JSONObject();
        try {
            ret.put("button", -1);
        } catch(Exception ex) { }
        addInputValues(ret);
        notifyClosing(ret);
    }

    /* Called any time we're closing the dialog to cleanup and notify listeners that the dialog
     * is closing.
     */
    private void closeDialog(int which) {
        JSONObject ret = new JSONObject();
        mDialog.dismiss();

        addButtonResult(ret, which);
        addListResult(ret, which);
        addInputValues(ret);

        notifyClosing(ret);
    }

    /* Called any time we're closing the dialog to cleanup and notify listeners that the dialog
     * is closing.
     */
    private void notifyClosing(JSONObject aReturn) {
        try {
            aReturn.put("guid", mGuid);
        } catch(JSONException ex) { }

        // poke the Gecko thread in case it's waiting for new events
        GeckoAppShell.sendEventToGecko(GeckoEvent.createNoOpEvent());

        if (mCallback != null) {
            mCallback.onPromptFinished(aReturn.toString());
        }
    }

    /* Handles parsing the initial JSON sent to show dialogs
     */
    private void processMessage(JSONObject geckoObject) {
        String title = geckoObject.optString("title");
        String text = geckoObject.optString("text");
        mGuid = geckoObject.optString("guid");

        mButtons = getStringArray(geckoObject, "buttons");

        JSONArray inputs = getSafeArray(geckoObject, "inputs");
        mInputs = new PromptInput[inputs.length()];
        for (int i = 0; i < mInputs.length; i++) {
            try {
                mInputs[i] = PromptInput.getInput(inputs.getJSONObject(i));
                mInputs[i].setListener(this);
            } catch(Exception ex) { }
        }

        PromptListItem[] menuitems = PromptListItem.getArray(geckoObject.optJSONArray("listitems"));
        String selected = geckoObject.optString("choiceMode");

        int choiceMode = ListView.CHOICE_MODE_NONE;
        if ("single".equals(selected)) {
            choiceMode = ListView.CHOICE_MODE_SINGLE;
        } else if ("multiple".equals(selected)) {
            choiceMode = ListView.CHOICE_MODE_MULTIPLE;
        }

        show(title, text, menuitems, choiceMode);
    }

    // Called when the prompt inputs on the dialog change
    @Override
    public void onChange(PromptInput input) {
        // If there are no buttons on this dialog, assuming that "changing" an input
        // means something was selected and we can close. This provides a way to tap
        // on a list item and close the dialog automatically.
        closeIfNoButtons(-1);
    }

    private static JSONArray getSafeArray(JSONObject json, String key) {
        try {
            return json.getJSONArray(key);
        } catch (Exception e) {
            return new JSONArray();
        }
    }

    public static String[] getStringArray(JSONObject aObject, String aName) {
        JSONArray items = getSafeArray(aObject, aName);
        int length = items.length();
        String[] list = new String[length];
        for (int i = 0; i < length; i++) {
            try {
                list[i] = items.getString(i);
            } catch(Exception ex) { }
        }
        return list;
    }

    private static boolean[] getBooleanArray(JSONObject aObject, String aName) {
        JSONArray items = new JSONArray();
        try {
            items = aObject.getJSONArray(aName);
        } catch(Exception ex) { return null; }
        int length = items.length();
        boolean[] list = new boolean[length];
        for (int i = 0; i < length; i++) {
            try {
                list[i] = items.getBoolean(i);
            } catch(Exception ex) { }
        }
        return list;
    }

    public interface PromptCallback {
        public void onPromptFinished(String jsonResult);
    }
}
