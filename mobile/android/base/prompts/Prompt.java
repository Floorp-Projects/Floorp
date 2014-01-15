/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.prompts;

import org.mozilla.gecko.util.GeckoEventResponder;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.widget.DateTimePicker;
import org.mozilla.gecko.prompts.ColorPickerInput;
import org.mozilla.gecko.R;
import org.mozilla.gecko.GeckoAppShell;

import org.json.JSONArray;
import org.json.JSONObject;
import org.json.JSONException;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnClickListener;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.Html;
import android.text.InputType;
import android.text.TextUtils;
import android.text.format.DateFormat;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.inputmethod.InputMethodManager;
import android.widget.AbsListView;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.CheckedTextView;
import android.widget.DatePicker;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.TimePicker;

import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.GregorianCalendar;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.TimeUnit;

public class Prompt implements OnClickListener, OnCancelListener, OnItemClickListener {
    private static final String LOGTAG = "GeckoPromptService";

    private String[] mButtons;
    private PromptInput[] mInputs;
    private boolean[] mSelected;
    private AlertDialog mDialog;

    private final LayoutInflater mInflater;
    private ConcurrentLinkedQueue<String> mPromptQueue;
    private final Context mContext;
    private PromptCallback mCallback;
    private String mGuid;

    private static boolean mInitialized = false;
    private static int mGroupPaddingSize;
    private static int mLeftRightTextWithIconPadding;
    private static int mTopBottomTextWithIconPadding;
    private static int mIconTextPadding;
    private static int mIconSize;
    private static int mInputPaddingSize;
    private static int mMinRowSize;

    public Prompt(Context context, ConcurrentLinkedQueue<String> queue) {
        this(context);
        mCallback = null;
        mPromptQueue = queue;
    }

    public Prompt(Context context, PromptCallback callback) {
        this(context);
        mCallback = callback;
        mPromptQueue = null;
    }

    private Prompt(Context context) {
        mContext = context;
        mInflater = LayoutInflater.from(mContext);

        if (!mInitialized) {
            Resources res = mContext.getResources();
            mGroupPaddingSize = (int) (res.getDimension(R.dimen.prompt_service_group_padding_size));
            mLeftRightTextWithIconPadding = (int) (res.getDimension(R.dimen.prompt_service_left_right_text_with_icon_padding));
            mTopBottomTextWithIconPadding = (int) (res.getDimension(R.dimen.prompt_service_top_bottom_text_with_icon_padding));
            mIconTextPadding = (int) (res.getDimension(R.dimen.prompt_service_icon_text_padding));
            mIconSize = (int) (res.getDimension(R.dimen.prompt_service_icon_size));
            mInputPaddingSize = (int) (res.getDimension(R.dimen.prompt_service_inputs_padding));
            mMinRowSize = (int) (res.getDimension(R.dimen.prompt_service_min_list_item_height));
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

    public void show(String title, String text, PromptListItem[] listItems, boolean multipleSelection) {
        ThreadUtils.assertOnUiThread();

        GeckoAppShell.getLayerView().abortPanning();

        AlertDialog.Builder builder = new AlertDialog.Builder(mContext);
        if (!TextUtils.isEmpty(title)) {
            builder.setTitle(title);
        }

        if (!TextUtils.isEmpty(text)) {
            builder.setMessage(text);
        }

        // Because lists are currently added through the normal Android AlertBuilder interface, they're
        // incompatible with also adding additional input elements to a dialog.
        if (listItems != null && listItems.length > 0) {
            addlistItems(builder, listItems, multipleSelection);
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
        try {
            if (mSelected != null) {
                JSONArray selected = new JSONArray();
                for (int i = 0; i < mSelected.length; i++) {
                    if (mSelected[i]) {
                        selected.put(i);
                    }
                }
                result.put("list", selected);
            } else {
                // Mirror the selected array from multi choice for consistency.
                JSONArray selected = new JSONArray();
                selected.put(which);
                result.put("list", selected);
                // Make the button be the index of the select item.
                result.put("button", which);
            }
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
        JSONObject ret = new JSONObject();
        try {
            ListView list = mDialog.getListView();
            addButtonResult(ret, which);
            addInputValues(ret);

            if (list != null || mSelected != null) {
                addListResult(ret, which);
            }
        } catch(Exception ex) {
            Log.i(LOGTAG, "Error building return: " + ex);
        }

        if (dialog != null) {
            dialog.dismiss();
        }

        finishDialog(ret);
    }

    /* Adds a set of list items to the prompt. This can be used for either context menu type dialogs, checked lists,
     * or multiple selection lists. If mSelected is set in the prompt before addlistItems is called, the items will be
     * shown with "checkmarks" on their left side.
     *
     * @param builder
     *        The alert builder currently building this dialog.
     * @param listItems
     *        The items to add.
     * @param multipleSelection
     *        If true, and mSelected is defined to be a non-zero-length list, the list will show checkmarks on the
     *        left and allow multiple selection. 
    */
    private void addlistItems(AlertDialog.Builder builder, PromptListItem[] listItems, boolean multipleSelection) {
        if (mSelected != null && mSelected.length > 0) {
            if (multipleSelection) {
                addMultiSelectList(builder, listItems);
            } else {
                addSingleSelectList(builder, listItems);
            }
        } else {
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
        PromptListAdapter adapter = new PromptListAdapter(mContext, R.layout.select_dialog_multichoice, listItems);
        adapter.listView = (ListView) mInflater.inflate(R.layout.select_dialog_list, null);
        adapter.listView.setOnItemClickListener(this);
        builder.setInverseBackgroundForced(true);
        adapter.listView.setChoiceMode(ListView.CHOICE_MODE_MULTIPLE);
        adapter.listView.setAdapter(adapter);
        builder.setView(adapter.listView);
    }

    /* Shows a single-select list with radio boxes on the side.
     *
     * @param builder
     *        the alert builder currently building this dialog.
     * @param listItems
     *        The items to add.
     */
    private void addSingleSelectList(AlertDialog.Builder builder, PromptListItem[] listItems) {
        PromptListAdapter adapter = new PromptListAdapter(mContext, R.layout.select_dialog_singlechoice, listItems);
        // For single select, we only maintain a single index of the selected row
        int selectedIndex = -1;
        for (int i = 0; i < mSelected.length; i++) {
            if (mSelected[i]) {
                selectedIndex = i;
                break;
            }
        }
        mSelected = null;

        builder.setSingleChoiceItems(adapter, selectedIndex, this);
    }

    /* Shows a single-select list.
     *
     * @param builder
     *        the alert builder currently building this dialog.
     * @param listItems
     *        The items to add.
     */
    private void addMenuList(AlertDialog.Builder builder, PromptListItem[] listItems) {
        PromptListAdapter adapter = new PromptListAdapter(mContext, android.R.layout.simple_list_item_1, listItems);
        builder.setAdapter(adapter, this);
        mSelected = null;
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
                root = mInputs[0].getView(mContext);
                applyInputStyle(root, mInputs[0]);
                scrollable |= mInputs[0].getScrollable();
            } else if (length > 1) {
                LinearLayout linearLayout = new LinearLayout(mContext);
                linearLayout.setOrientation(LinearLayout.VERTICAL);
                for (int i = 0; i < length; i++) {
                    View content = mInputs[i].getView(mContext);
                    applyInputStyle(content, mInputs[i]);
                    linearLayout.addView(content);
                    scrollable |= mInputs[i].getScrollable();
                }
                root = linearLayout;
            }

            if (scrollable) {
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
        mSelected[position] = !mSelected[position];
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
        finishDialog(ret);
    }

    /* Called any time we're closing the dialog to cleanup and notify listeners that the dialog
     * is closing.
     */
    public void finishDialog(JSONObject aReturn) {
        mInputs = null;
        mButtons = null;
        mDialog = null;
        mSelected = null;
        try {
            aReturn.put("guid", mGuid);
        } catch(JSONException ex) { }

        if (mPromptQueue != null) {
            mPromptQueue.offer(aReturn.toString());
        }

        // poke the Gecko thread in case it's waiting for new events
        GeckoAppShell.sendEventToGecko(GeckoEvent.createNoOpEvent());

        if (mCallback != null) {
            mCallback.onPromptFinished(aReturn.toString());
        }
        mGuid = null;
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
            } catch(Exception ex) { }
        }

        PromptListItem[] menuitems = getListItemArray(geckoObject, "listitems");
        mSelected = getBooleanArray(geckoObject, "selected");
        boolean multiple = geckoObject.optBoolean("multiple");
        show(title, text, menuitems, multiple);
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

    private PromptListItem[] getListItemArray(JSONObject aObject, String aName) {
        JSONArray items = getSafeArray(aObject, aName);
        int length = items.length();
        PromptListItem[] list = new PromptListItem[length];
        for (int i = 0; i < length; i++) {
            try {
                list[i] = new PromptListItem(items.getJSONObject(i));
            } catch(Exception ex) { }
        }
        return list;
    }

    public static class PromptListItem {
        public final String label;
        public final boolean isGroup;
        public final boolean inGroup;
        public final boolean disabled;
        public final int id;
        public final boolean isParent;

        // This member can't be accessible from JS, see bug 733749.
        public Drawable icon;

        PromptListItem(JSONObject aObject) {
            label = aObject.optString("label");
            isGroup = aObject.optBoolean("isGroup");
            inGroup = aObject.optBoolean("inGroup");
            disabled = aObject.optBoolean("disabled");
            id = aObject.optInt("id");
            isParent = aObject.optBoolean("isParent");
        }

        public PromptListItem(String aLabel) {
            label = aLabel;
            isGroup = false;
            inGroup = false;
            disabled = false;
            id = 0;
            isParent = false;
        }
    }

    public interface PromptCallback {
        public void onPromptFinished(String jsonResult);
    }

    public class PromptListAdapter extends ArrayAdapter<PromptListItem> {
        private static final int VIEW_TYPE_ITEM = 0;
        private static final int VIEW_TYPE_GROUP = 1;
        private static final int VIEW_TYPE_COUNT = 2;

        public ListView listView;
        private int mResourceId = -1;
        private Drawable mBlankDrawable = null;
        private Drawable mMoreDrawable = null;

        PromptListAdapter(Context context, int textViewResourceId, PromptListItem[] objects) {
            super(context, textViewResourceId, objects);
            mResourceId = textViewResourceId;
        }

        @Override
        public int getItemViewType(int position) {
            PromptListItem item = getItem(position);
            return (item.isGroup ? VIEW_TYPE_GROUP : VIEW_TYPE_ITEM);
        }

        @Override
        public int getViewTypeCount() {
            return VIEW_TYPE_COUNT;
        }

        private Drawable getMoreDrawable(Resources res) {
            if (mMoreDrawable == null) {
                mMoreDrawable = res.getDrawable(android.R.drawable.ic_menu_more);
            }
            return mMoreDrawable;
        }

        private Drawable getBlankDrawable(Resources res) {
            if (mBlankDrawable == null) {
                mBlankDrawable = res.getDrawable(R.drawable.blank);
            }
            return mBlankDrawable;
        }

        private void maybeUpdateIcon(PromptListItem item, TextView t) {
            if (item.icon == null && !item.inGroup && !item.isParent) {
                t.setCompoundDrawablesWithIntrinsicBounds(null, null, null, null);
                return;
            }

            Drawable d = null;
            Resources res = mContext.getResources();
            // Set the padding between the icon and the text.
            t.setCompoundDrawablePadding(mIconTextPadding);
            if (item.icon != null) {
                // We want the icon to be of a specific size. Some do not
                // follow this rule so we have to resize them.
                Bitmap bitmap = ((BitmapDrawable) item.icon).getBitmap();
                d = new BitmapDrawable(res, Bitmap.createScaledBitmap(bitmap, mIconSize, mIconSize, true));
            } else if (item.inGroup) {
                // We don't currently support "indenting" items with icons
                d = getBlankDrawable(res);
            }

            Drawable moreDrawable = null;
            if (item.isParent) {
                moreDrawable = getMoreDrawable(res);
            }

            if (d != null || moreDrawable != null) {
                t.setCompoundDrawablesWithIntrinsicBounds(d, null, moreDrawable, null);
            }
        }

        private void maybeUpdateCheckedState(int position, PromptListItem item, ViewHolder viewHolder) {
            viewHolder.textView.setEnabled(!item.disabled && !item.isGroup);
            viewHolder.textView.setClickable(item.isGroup || item.disabled);

            if (mSelected == null) {
                return;
            }

            CheckedTextView ct;
            try {
                ct = (CheckedTextView) viewHolder.textView;
                // Apparently just using ct.setChecked(true) doesn't work, so this
                // is stolen from the android source code as a way to set the checked
                // state of these items
                if (listView != null) {
                    listView.setItemChecked(position, mSelected[position]);
                }
            } catch (Exception e) {
                return;
            }

        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            PromptListItem item = getItem(position);
            ViewHolder viewHolder = null;

            if (convertView == null) {
                int resourceId = mResourceId;
                if (item.isGroup) {
                    resourceId = R.layout.list_item_header;
                }

                convertView = mInflater.inflate(resourceId, null);
                convertView.setMinimumHeight(mMinRowSize);

                TextView tv = (TextView) convertView.findViewById(android.R.id.text1);
                viewHolder = new ViewHolder(tv, tv.getPaddingLeft(), tv.getPaddingRight(),
                                            tv.getPaddingTop(), tv.getPaddingBottom());

                convertView.setTag(viewHolder);
            } else {
                viewHolder = (ViewHolder) convertView.getTag();
            }

            viewHolder.textView.setText(item.label);
            maybeUpdateCheckedState(position, item, viewHolder);
            maybeUpdateIcon(item, viewHolder.textView);

            return convertView;
        }

        private class ViewHolder {
            public final TextView textView;
            public final int paddingLeft;
            public final int paddingRight;
            public final int paddingTop;
            public final int paddingBottom;

            ViewHolder(TextView aTextView, int aLeft, int aRight, int aTop, int aBottom) {
                textView = aTextView;
                paddingLeft = aLeft;
                paddingRight = aRight;
                paddingTop = aTop;
                paddingBottom = aBottom;
            }
        }
    }
}
