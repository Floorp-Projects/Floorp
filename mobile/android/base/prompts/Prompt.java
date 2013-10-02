/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.prompts;

import org.mozilla.gecko.util.GeckoEventResponder;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.widget.DateTimePicker;
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

    private View applyInputStyle(View view) {
        view.setPadding(mInputPaddingSize, 0, mInputPaddingSize, 0);
        return view;
    }

    public void show(JSONObject message) {
        processMessage(message);
    }

    public void show(String aTitle, String aText, PromptListItem[] aMenuList, boolean aMultipleSelection) {
        ThreadUtils.assertOnUiThread();

        GeckoAppShell.getLayerView().abortPanning();

        AlertDialog.Builder builder = new AlertDialog.Builder(mContext);
        if (!TextUtils.isEmpty(aTitle)) {
            builder.setTitle(aTitle);
        }

        if (!TextUtils.isEmpty(aText)) {
            builder.setMessage(aText);
        }

        int length = mInputs == null ? 0 : mInputs.length;
        if (aMenuList != null && aMenuList.length > 0) {
            int resourceId = android.R.layout.simple_list_item_1;
            if (mSelected != null && mSelected.length > 0) {
                if (aMultipleSelection) {
                    resourceId = R.layout.select_dialog_multichoice;
                } else {
                    resourceId = R.layout.select_dialog_singlechoice;
                }
            }
            PromptListAdapter adapter = new PromptListAdapter(mContext, resourceId, aMenuList);
            if (mSelected != null && mSelected.length > 0) {
                if (aMultipleSelection) {
                    adapter.listView = (ListView) mInflater.inflate(R.layout.select_dialog_list, null);
                    adapter.listView.setOnItemClickListener(this);
                    builder.setInverseBackgroundForced(true);
                    adapter.listView.setChoiceMode(ListView.CHOICE_MODE_MULTIPLE);
                    adapter.listView.setAdapter(adapter);
                    builder.setView(adapter.listView);
                } else {
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
            } else {
                builder.setAdapter(adapter, this);
                mSelected = null;
            }
        } else if (length == 1) {
            try {
                ScrollView view = new ScrollView(mContext);
                view.addView(mInputs[0].getView(mContext));
                builder.setView(applyInputStyle(view));
            } catch(UnsupportedOperationException ex) {
                // We cannot display these input widgets with this sdk version,
                // do not display any dialog and finish the prompt now.
                try {
                    finishDialog(new JSONObject("{\"button\": -1}"));
                } catch(JSONException e) { }
                return;
            }
        } else if (length > 1) {
            try {
                LinearLayout linearLayout = new LinearLayout(mContext);
                linearLayout.setOrientation(LinearLayout.VERTICAL);
                for (int i = 0; i < length; i++) {
                    View content = mInputs[i].getView(mContext);
                    linearLayout.addView(content);
                }
                ScrollView view = new ScrollView(mContext);
                view.addView(linearLayout);
                builder.setView(applyInputStyle(view));
            } catch(UnsupportedOperationException ex) {
                // We cannot display these input widgets with this sdk version,
                // do not display any dialog and finish the prompt now.
                try {
                    finishDialog(new JSONObject("{\"button\": -1}"));
                } catch(JSONException e) { }
                return;
            }
        }

        length = mButtons == null ? 0 : mButtons.length;
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

    @Override
    public void onClick(DialogInterface aDialog, int aWhich) {
        ThreadUtils.assertOnUiThread();
        JSONObject ret = new JSONObject();
        try {
            int button = -1;
            ListView list = mDialog.getListView();
            if (list != null || mSelected != null) {
                button = aWhich;
                if (mSelected != null) {
                    JSONArray selected = new JSONArray();
                    for (int i = 0; i < mSelected.length; i++) {
                        selected.put(mSelected[i]);
                    }
                    ret.put("button", selected);
                } else {
                    ret.put("button", button);
                }
            } else {
                switch(aWhich) {
                    case DialogInterface.BUTTON_POSITIVE : button = 0; break;
                    case DialogInterface.BUTTON_NEUTRAL  : button = 1; break;
                    case DialogInterface.BUTTON_NEGATIVE : button = 2; break;
                }
                ret.put("button", button);
            }
            if (mInputs != null) {
                for (int i = 0; i < mInputs.length; i++) {
                    ret.put(mInputs[i].getId(), mInputs[i].getValue());
                }
            }
        } catch(Exception ex) {
            Log.i(LOGTAG, "Error building return: " + ex);
        }

        if (mDialog != null) {
            mDialog.dismiss();
        }

        finishDialog(ret);
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        ThreadUtils.assertOnUiThread();
        mSelected[position] = !mSelected[position];
    }

    @Override
    public void onCancel(DialogInterface aDialog) {
        ThreadUtils.assertOnUiThread();
        JSONObject ret = new JSONObject();
        try {
            ret.put("button", -1);
        } catch(Exception ex) { }
        finishDialog(ret);
    }

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

    private void processMessage(JSONObject geckoObject) {
        String title = getSafeString(geckoObject, "title");
        String text = getSafeString(geckoObject, "text");
        mGuid = getSafeString(geckoObject, "guid");

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

    private static String getSafeString(JSONObject json, String key) {
        try {
            return json.getString(key);
        } catch (Exception e) {
            return "";
        }
    }

    private static JSONArray getSafeArray(JSONObject json, String key) {
        try {
            return json.getJSONArray(key);
        } catch (Exception e) {
            return new JSONArray();
        }
    }

    private static boolean getSafeBool(JSONObject json, String key) {
        try {
            return json.getBoolean(key);
        } catch (Exception e) {
            return false;
        }
    }

    private static int getSafeInt(JSONObject json, String key ) {
        try {
            return json.getInt(key);
        } catch (Exception e) {
            return 0;
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
            label = getSafeString(aObject, "label");
            isGroup = getSafeBool(aObject, "isGroup");
            inGroup = getSafeBool(aObject, "inGroup");
            disabled = getSafeBool(aObject, "disabled");
            id = getSafeInt(aObject, "id");
            isParent = getSafeBool(aObject, "isParent");
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
                d = new BitmapDrawable(Bitmap.createScaledBitmap(bitmap, mIconSize, mIconSize, true));
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
