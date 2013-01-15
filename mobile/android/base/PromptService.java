/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.util.GeckoEventResponder;
import org.mozilla.gecko.widget.DateTimePicker;

import org.json.JSONArray;
import org.json.JSONObject;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnClickListener;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.InputType;
import android.text.format.DateFormat;
import android.util.Log;
import android.view.inputmethod.InputMethodManager;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.CheckedTextView;
import android.widget.DatePicker;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.TimePicker;

import java.text.SimpleDateFormat;
import java.util.concurrent.SynchronousQueue;
import java.util.concurrent.TimeUnit;
import java.util.Calendar;
import java.util.GregorianCalendar;

public class PromptService implements OnClickListener, OnCancelListener, OnItemClickListener, GeckoEventResponder {
    private static final String LOGTAG = "GeckoPromptService";

    private static LayoutInflater sInflater;
    private static SynchronousQueue<String> sPromptQueue = new SynchronousQueue<String>();

    private String[] mButtons;
    private PromptInput[] mInputs;
    private boolean[] mSelected;
    private AlertDialog mDialog;

    private final int mGroupPaddingSize;
    private final int mLeftRightTextWithIconPadding;
    private final int mTopBottomTextWithIconPadding;
    private final int mIconTextPadding;
    private final int mIconSize;

    PromptService() {
        sInflater = LayoutInflater.from(GeckoApp.mAppContext);

        Resources res = GeckoApp.mAppContext.getResources();
        mGroupPaddingSize = (int) (res.getDimension(R.dimen.prompt_service_group_padding_size));
        mLeftRightTextWithIconPadding = (int) (res.getDimension(R.dimen.prompt_service_left_right_text_with_icon_padding));
        mTopBottomTextWithIconPadding = (int) (res.getDimension(R.dimen.prompt_service_top_bottom_text_with_icon_padding));
        mIconTextPadding = (int) (res.getDimension(R.dimen.prompt_service_icon_text_padding));
        mIconSize = (int) (res.getDimension(R.dimen.prompt_service_icon_size));

        GeckoAppShell.getEventDispatcher().registerEventListener("Prompt:Show", this);
    }

    void destroy() {
        GeckoAppShell.getEventDispatcher().unregisterEventListener("Prompt:Show", this);
    }

    private static String formatDateString(String dateFormat, Calendar calendar) {
        return new SimpleDateFormat(dateFormat).format(calendar.getTime());
    }

    private class PromptInput {
        private final JSONObject mJSONInput;

        private final String mLabel;
        private final String mType;
        private final String mHint;
        private final boolean mAutofocus;
        private final String mValue;

        private View mView;

        public PromptInput(JSONObject aJSONInput) {
            mJSONInput = aJSONInput;
            mLabel = getSafeString(aJSONInput, "label");
            mType = getSafeString(aJSONInput, "type");
            mHint = getSafeString(aJSONInput, "hint");
            mValue = getSafeString(aJSONInput, "value");
            mAutofocus = getSafeBool(aJSONInput, "autofocus");
        }

        public View getView() throws UnsupportedOperationException {
            if (mType.equals("checkbox")) {
                CheckBox checkbox = new CheckBox(GeckoApp.mAppContext);
                checkbox.setLayoutParams(new LayoutParams(LayoutParams.FILL_PARENT, LayoutParams.WRAP_CONTENT));
                checkbox.setText(mLabel);
                checkbox.setChecked(getSafeBool(mJSONInput, "checked"));
                mView = (View)checkbox;
            } else if (mType.equals("date")) {
                try {
                    DateTimePicker input = new DateTimePicker(GeckoApp.mAppContext, "yyyy-MM-dd", mValue,
                                                              DateTimePicker.pickersState.DATE);
                    input.toggleCalendar(true);
                    mView = (View)input;
                } catch (UnsupportedOperationException ex) {
                    // We can't use our custom version of the DatePicker widget because the sdk is too old.
                    // But we can fallback on the native one.
                    DatePicker input = new DatePicker(GeckoApp.mAppContext);
                    try {
                        if (!mValue.equals("")) {
                            GregorianCalendar calendar = new GregorianCalendar();
                            calendar.setTime(new SimpleDateFormat("yyyy-MM-dd").parse(mValue));
                            input.updateDate(calendar.get(Calendar.YEAR),
                                             calendar.get(Calendar.MONTH),
                                             calendar.get(Calendar.DAY_OF_MONTH));
                        }
                    } catch (Exception e) {
                        Log.e(LOGTAG, "error parsing format string: " + e);
                    }
                    mView = (View)input;
                }
            } else if (mType.equals("week")) {
                DateTimePicker input = new DateTimePicker(GeckoApp.mAppContext, "yyyy-'W'ww", mValue,
                                                          DateTimePicker.pickersState.WEEK);
                mView = (View)input;
            } else if (mType.equals("time")) {
                TimePicker input = new TimePicker(GeckoApp.mAppContext);
                input.setIs24HourView(DateFormat.is24HourFormat(GeckoApp.mAppContext));

                GregorianCalendar calendar = new GregorianCalendar();
                if (!mValue.equals("")) {
                    try {
                        calendar.setTime(new SimpleDateFormat("kk:mm").parse(mValue));
                    } catch (Exception e) { }
                }
                input.setCurrentHour(calendar.get(GregorianCalendar.HOUR_OF_DAY));
                input.setCurrentMinute(calendar.get(GregorianCalendar.MINUTE));
                mView = (View)input;
            } else if (mType.equals("datetime-local") || mType.equals("datetime")) {
                DateTimePicker input = new DateTimePicker(GeckoApp.mAppContext, "yyyy-MM-dd kk:mm", mValue,
                                                          DateTimePicker.pickersState.DATETIME);
                input.toggleCalendar(true);
                mView = (View)input;
            } else if (mType.equals("month")) {
                DateTimePicker input = new DateTimePicker(GeckoApp.mAppContext, "yyyy-MM", mValue,
                                                          DateTimePicker.pickersState.MONTH);
                mView = (View)input;
            } else if (mType.equals("textbox") || mType.equals("password")) {
                EditText input = new EditText(GeckoApp.mAppContext);
                int inputtype = InputType.TYPE_CLASS_TEXT;
                if (mType.equals("password")) {
                    inputtype |= InputType.TYPE_TEXT_VARIATION_PASSWORD | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
                }
                input.setInputType(inputtype);
                input.setText(mValue);

                if (!mHint.equals("")) {
                    input.setHint(mHint);
                }

                if (mAutofocus) {
                    input.setOnFocusChangeListener(new View.OnFocusChangeListener() {
                        public void onFocusChange(View v, boolean hasFocus) {
                            if (hasFocus) {
                                ((InputMethodManager) GeckoApp.mAppContext.getSystemService(Context.INPUT_METHOD_SERVICE)).showSoftInput(v, 0);
                            }
                        }
                    });
                    input.requestFocus();
                }

                mView = (View)input;
            } else if (mType.equals("menulist")) {
                Spinner spinner = new Spinner(GeckoApp.mAppContext);
                try {
                    String[] listitems = getStringArray(mJSONInput, "values");
                    if (listitems.length > 0) {
                        ArrayAdapter<String> adapter = new ArrayAdapter<String>(GeckoApp.mAppContext, android.R.layout.simple_dropdown_item_1line, listitems);
                        spinner.setAdapter(adapter);
                    }
                } catch(Exception ex) { }
                mView = (View)spinner;
            }
            return mView;
        }

        public String getName() {
            return mType;
        }

        public String getValue() {
            if (mType.equals("checkbox")) {
                CheckBox checkbox = (CheckBox)mView;
                return checkbox.isChecked() ? "true" : "false";
            } else if (mType.equals("textbox") || mType.equals("password")) {
                EditText edit = (EditText)mView;
                return edit.getText().toString();
            } else if (mType.equals("menulist")) {
                Spinner spinner = (Spinner)mView;
                return Integer.toString(spinner.getSelectedItemPosition());
            } else if (mType.equals("time")) {
                TimePicker tp = (TimePicker)mView;
                GregorianCalendar calendar =
                    new GregorianCalendar(0,0,0,tp.getCurrentHour(),tp.getCurrentMinute());
                return formatDateString("kk:mm",calendar);
            } else if (android.os.Build.VERSION.SDK_INT < 11 && mType.equals("date")) {
                // We can't use the custom DateTimePicker with a sdk older than 11.
                // Fallback on the native DatePicker.
                DatePicker dp = (DatePicker)mView;
                GregorianCalendar calendar =
                    new GregorianCalendar(dp.getYear(),dp.getMonth(),dp.getDayOfMonth());
                return formatDateString("yyyy-MM-dd",calendar);
            } else {
                DateTimePicker dp = (DateTimePicker)mView;
                GregorianCalendar calendar = new GregorianCalendar();
                calendar.setTimeInMillis(dp.getTimeInMillis());
                if (mType.equals("date")) {
                    return formatDateString("yyyy-MM-dd",calendar);
                } else if (mType.equals("week")) {
                    return formatDateString("yyyy-'W'ww",calendar);
                } else if (mType.equals("datetime-local")) {
                    return formatDateString("yyyy-MM-dd kk:mm",calendar);
                } else if (mType.equals("datetime")) {
                    calendar.set(GregorianCalendar.ZONE_OFFSET,0);
                    calendar.setTimeInMillis(dp.getTimeInMillis());
                    return formatDateString("yyyy-MM-dd kk:mm",calendar);
                } else if (mType.equals("month")) {
                    return formatDateString("yyyy-MM",calendar);
                }
            }
            return "";
        }
    }

    // GeckoEventListener implementation
    public void handleMessage(String event, final JSONObject message) {
        // The dialog must be created on the UI thread.
        GeckoAppShell.getMainHandler().post(new Runnable() {
            public void run() {
                processMessage(message);
            }
        });
    }

    // GeckoEventResponder implementation
    public String getResponse() {
        // we only handle one kind of message in handleMessage, and this is the
        // response we provide for that message
        String promptServiceResult = "";
        try {
            promptServiceResult = waitForReturn();
        } catch (InterruptedException e) { }
        return promptServiceResult;
    }

    public void show(String aTitle, String aText, PromptListItem[] aMenuList, boolean aMultipleSelection) {
        GeckoApp.assertOnUiThread();

        final LayerView layerView = GeckoApp.mAppContext.getLayerView();
        // treat actions that show a dialog as if preventDefault by content to prevent panning
        layerView.abortPanning();

        final AlertDialog.Builder builder = new AlertDialog.Builder(GeckoApp.mAppContext);
        if (!aTitle.equals("")) {
            builder.setTitle(aTitle);
        }

        if (!aText.equals("")) {
            builder.setMessage(aText);
        }

        int length = mInputs == null ? 0 : mInputs.length;
        if (aMenuList != null && aMenuList.length > 0) {
            int resourceId = android.R.layout.simple_list_item_1;
            if (mSelected != null && mSelected.length > 0) {
                if (aMultipleSelection) {
                    resourceId = R.layout.select_dialog_multichoice;
                } else {
                    resourceId = android.R.layout.select_dialog_singlechoice;
                }
            }
            PromptListAdapter adapter = new PromptListAdapter(GeckoApp.mAppContext, resourceId, aMenuList);
            if (mSelected != null && mSelected.length > 0) {
                if (aMultipleSelection) {
                    adapter.listView = (ListView) sInflater.inflate(R.layout.select_dialog_list, null);
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
                builder.setView(mInputs[0].getView());
            } catch(UnsupportedOperationException ex) {
                // We cannot display these input widgets with this sdk version,
                // do not display any dialog and finish the prompt now.
                finishDialog("{\"button\": -1}");
                return;
            }
        } else if (length > 1) {
            LinearLayout linearLayout = new LinearLayout(GeckoApp.mAppContext);
            linearLayout.setOrientation(LinearLayout.VERTICAL);
            try {
                for (int i = 0; i < length; i++) {
                    View content = mInputs[i].getView();
                    linearLayout.addView(content);
                }
            } catch(UnsupportedOperationException ex) {
                // We cannot display these input widgets with this sdk version,
                // do not display any dialog and finish the prompt now.
                finishDialog("{\"button\": -1}");
                return;
            }
            builder.setView((View)linearLayout);
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
        mDialog.setOnCancelListener(PromptService.this);
        mDialog.show();
    }

    public void onClick(DialogInterface aDialog, int aWhich) {
        GeckoApp.assertOnUiThread();
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
                    ret.put(mInputs[i].getName(), mInputs[i].getValue());
                }
            }
        } catch(Exception ex) {
            Log.i(LOGTAG, "Error building return: " + ex);
        }

        if (mDialog != null) {
            mDialog.dismiss();
        }

        finishDialog(ret.toString());
    }

    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        GeckoApp.assertOnUiThread();
        mSelected[position] = !mSelected[position];
    }

    public void onCancel(DialogInterface aDialog) {
        GeckoApp.assertOnUiThread();
        JSONObject ret = new JSONObject();
        try {
            ret.put("button", -1);
        } catch(Exception ex) { }
        finishDialog(ret.toString());
    }

    public static String waitForReturn() throws InterruptedException {
        String value;

        while (null == (value = sPromptQueue.poll(1, TimeUnit.MILLISECONDS))) {
            GeckoAppShell.processNextNativeEvent();
        }

        return value;
    }

    public void finishDialog(String aReturn) {
        mInputs = null;
        mButtons = null;
        mDialog = null;
        mSelected = null;
        try {
            sPromptQueue.put(aReturn);
        } catch(Exception ex) {
            Log.d(LOGTAG, "sPromptQueue not ready yet");
        }
    }

    private void processMessage(JSONObject geckoObject) {
        String title = getSafeString(geckoObject, "title");
        String text = getSafeString(geckoObject, "text");

        mButtons = getStringArray(geckoObject, "buttons");

        JSONArray inputs = getSafeArray(geckoObject, "inputs");
        mInputs = new PromptInput[inputs.length()];
        for (int i = 0; i < mInputs.length; i++) {
            try {
                mInputs[i] = new PromptInput(inputs.getJSONObject(i));
            } catch(Exception ex) { }
        }

        PromptListItem[] menuitems = getListItemArray(geckoObject, "listitems");
        mSelected = getBooleanArray(geckoObject, "selected");
        boolean multiple = getSafeBool(geckoObject, "multiple");
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

    private String[] getStringArray(JSONObject aObject, String aName) {
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

    private boolean[] getBooleanArray(JSONObject aObject, String aName) {
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

    public class PromptListAdapter extends ArrayAdapter<PromptListItem> {
        private static final int VIEW_TYPE_ITEM = 0;
        private static final int VIEW_TYPE_GROUP = 1;
        private static final int VIEW_TYPE_COUNT = 2;

        public ListView listView;
        private int mResourceId = -1;

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

        private void maybeUpdateIcon(PromptListItem item, TextView t) {
            if (item.icon == null && !item.isParent) {
                t.setCompoundDrawablesWithIntrinsicBounds(null, null, null, null);
                return;
            }

            Drawable d = null;
            Resources res = GeckoApp.mAppContext.getResources();
            // Set the padding between the icon and the text.
            t.setCompoundDrawablePadding(mIconTextPadding);
            if (item.icon != null) {
                // Set padding inside the item.
                t.setPadding(item.inGroup ? mLeftRightTextWithIconPadding + mGroupPaddingSize :
                                            mLeftRightTextWithIconPadding,
                             mTopBottomTextWithIconPadding,
                             mLeftRightTextWithIconPadding,
                             mTopBottomTextWithIconPadding);
                // We want the icon to be of a specific size. Some do not
                // follow this rule so we have to resize them.
                Bitmap bitmap = ((BitmapDrawable) item.icon).getBitmap();
                d = new BitmapDrawable(Bitmap.createScaledBitmap(bitmap, mIconSize, mIconSize, true));
            }

            Drawable moreDrawable = null;
            if (item.isParent) {
                moreDrawable = res.getDrawable(android.R.drawable.ic_menu_more);
            }

            if (d != null || moreDrawable != null) {
                t.setCompoundDrawablesWithIntrinsicBounds(d, null, moreDrawable, null);
            }
        }

        private void maybeUpdateCheckedState(int position, PromptListItem item, ViewHolder viewHolder) {
            viewHolder.textView.setPadding((item.inGroup ? mGroupPaddingSize : viewHolder.paddingLeft),
                                            viewHolder.paddingTop,
                                            viewHolder.paddingRight,
                                            viewHolder.paddingBottom);

            viewHolder.textView.setEnabled(!item.disabled && !item.isGroup);
            viewHolder.textView.setClickable(item.isGroup || item.disabled);

            if (mSelected == null)
                return;

            CheckedTextView ct;
            try {
                ct = (CheckedTextView) viewHolder.textView;
                // Apparently just using ct.setChecked(true) doesn't work, so this
                // is stolen from the android source code as a way to set the checked
                // state of these items
                if (listView != null)
                    listView.setItemChecked(position, mSelected[position]);
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

                convertView = sInflater.inflate(resourceId, null);

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
