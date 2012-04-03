/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Wes Johnston <wjohnston@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko;

import android.util.Log;
import java.lang.String;
import java.util.concurrent.SynchronousQueue;
import java.util.concurrent.TimeUnit;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnMultiChoiceClickListener;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.InputType;
import android.text.method.PasswordTransformationMethod;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup;
import android.view.LayoutInflater;
import android.view.ViewGroup.LayoutParams;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.TextView;
import android.widget.CheckBox;
import android.widget.CheckedTextView;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import org.json.JSONArray;
import org.json.JSONObject;
import android.text.InputType;

public class PromptService implements OnClickListener, OnCancelListener, OnItemClickListener {
    private static final String LOGTAG = "GeckoPromptService";

    private PromptInput[] mInputs;
    private AlertDialog mDialog = null;
    private static LayoutInflater mInflater;

    private final static int GROUP_PADDING_SIZE = 32; // in dip units
    private static int mGroupPaddingSize = 0; // calculated from GROUP_PADDING_SIZE. In pixel units

    private final static int LEFT_RIGHT_TEXT_WITH_ICON_PADDING = 10; // in dip units
    private static int mLeftRightTextWithIconPadding = 0; // calculated from LEFT_RIGHT_TEXT_WITH_ICON_PADDING.

    private final static int TOP_BOTTOM_TEXT_WITH_ICON_PADDING = 8; // in dip units
    private static int mTopBottomTextWithIconPadding = 0; // calculated from TOP_BOTTOM_TEXT_WITH_ICON_PADDING.

    private final static int ICON_TEXT_PADDING = 10; // in dip units
    private static int mIconTextPadding = 0; // calculated from ICON_TEXT_PADDING.

    private final static int ICON_SIZE = 72; // in dip units
    private static int mIconSize = 0; // calculated from ICON_SIZE.

    PromptService() {
        mInflater = LayoutInflater.from(GeckoApp.mAppContext);
        Resources res = GeckoApp.mAppContext.getResources();
        mGroupPaddingSize = (int)TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                                                           GROUP_PADDING_SIZE,
                                                           res.getDisplayMetrics());
        mLeftRightTextWithIconPadding = (int)TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                                                                       LEFT_RIGHT_TEXT_WITH_ICON_PADDING,
                                                                       res.getDisplayMetrics());
        mTopBottomTextWithIconPadding = (int)TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                                                                       TOP_BOTTOM_TEXT_WITH_ICON_PADDING,
                                                                       res.getDisplayMetrics());
        mIconTextPadding = (int)TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                                                          ICON_TEXT_PADDING,
                                                          res.getDisplayMetrics());
        mIconSize = (int)TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                                                   ICON_SIZE,
                                                   res.getDisplayMetrics());
    }

    private class PromptButton {
        public String label = "";
        PromptButton(JSONObject aJSONButton) {
            try {
                label = aJSONButton.getString("label");
            } catch(Exception ex) { }
        }
    }

    private class PromptInput {
        private String label = "";
        private String type  = "";
        private String hint  = "";
        private JSONObject mJSONInput = null;
        private View view = null;

        public PromptInput(JSONObject aJSONInput) {
            mJSONInput = aJSONInput;
            try {
                label = aJSONInput.getString("label");
            } catch(Exception ex) { }
            try {
                type  = aJSONInput.getString("type");
            } catch(Exception ex) { }
            try {
                hint  = aJSONInput.getString("hint");
            } catch(Exception ex) { }
        }

        public View getView() {
            if (type.equals("checkbox")) {
                CheckBox checkbox = new CheckBox(GeckoApp.mAppContext);
                checkbox.setLayoutParams(new LayoutParams(LayoutParams.FILL_PARENT, LayoutParams.WRAP_CONTENT));
                checkbox.setText(label);
                try {
                    Boolean value = mJSONInput.getBoolean("checked");
                    checkbox.setChecked(value);
                } catch(Exception ex) { }
                view = (View)checkbox;
            } else if (type.equals("textbox") || this.type.equals("password")) {
                EditText input = new EditText(GeckoApp.mAppContext);
                int inputtype = InputType.TYPE_CLASS_TEXT;
                if (type.equals("password")) {
                    inputtype |= InputType.TYPE_TEXT_VARIATION_PASSWORD | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
                }
                input.setInputType(inputtype);

                try {
                    String value = mJSONInput.getString("value");
                    input.setText(value);
                } catch(Exception ex) { }

                if (!hint.equals("")) {
                    input.setHint(hint);
                }
                view = (View)input;
            } else if (type.equals("menulist")) {
                Spinner spinner = new Spinner(GeckoApp.mAppContext);
                try {
                    String[] listitems = getStringArray(mJSONInput, "values");
                    if (listitems.length > 0) {
                        ArrayAdapter<String> adapter = new ArrayAdapter<String>(GeckoApp.mAppContext, android.R.layout.simple_dropdown_item_1line, listitems);
                        spinner.setAdapter(adapter);
                    }
                } catch(Exception ex) { }
                view = (View)spinner;
            }
            return view;
        }

        public String getName() {
            return type;
        }
    
        public String getValue() {
            if (this.type.equals("checkbox")) {
                CheckBox checkbox = (CheckBox)view;
                return checkbox.isChecked() ? "true" : "false";
            } else if (type.equals("textbox") || type.equals("password")) {
                EditText edit = (EditText)view;
                return edit.getText().toString();
            } else if (type.equals("menulist")) {
                Spinner spinner = (Spinner)view;
                return Integer.toString(spinner.getSelectedItemPosition());
            }
            return "";
        }
    }

    public void Show(String aTitle, String aText, PromptButton[] aButtons, PromptListItem[] aMenuList, boolean aMultipleSelection) {
        AlertDialog.Builder builder = new AlertDialog.Builder(GeckoApp.mAppContext);
        if (!aTitle.equals("")) {
            builder.setTitle(aTitle);
        }

        if (!aText.equals("")) {
            builder.setMessage(aText);
        }

        int length = mInputs == null ? 0 : mInputs.length;
        if (aMenuList != null && aMenuList.length > 0) {
            int resourceId = android.R.layout.select_dialog_item;
            if (mSelected != null && mSelected.length > 0) {
                if (aMultipleSelection) {
                    resourceId = android.R.layout.select_dialog_multichoice;
                } else {
                    resourceId = android.R.layout.select_dialog_singlechoice;
                }
            }
            PromptListAdapter adapter = new PromptListAdapter(GeckoApp.mAppContext, resourceId, aMenuList);
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
            builder.setView(mInputs[0].getView());
        } else if (length > 1) {
            LinearLayout linearLayout = new LinearLayout(GeckoApp.mAppContext);
            linearLayout.setOrientation(LinearLayout.VERTICAL);
            for (int i = 0; i < length; i++) {
                View content = mInputs[i].getView();
                linearLayout.addView(content);
            }
            builder.setView((View)linearLayout);
        }

        length = aButtons == null ? 0 : aButtons.length;
        if (length > 0) {
            builder.setPositiveButton(aButtons[0].label, this);
        }
        if (length > 1) {
            builder.setNeutralButton(aButtons[1].label, this);
        }
        if (length > 2) {
            builder.setNegativeButton(aButtons[2].label, this);
        }

        mDialog = builder.create();
        mDialog.setOnCancelListener(this);
        mDialog.show();
    }

    public void onClick(DialogInterface aDialog, int aWhich) {
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

    private boolean[] mSelected = null;
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        mSelected[position] = !mSelected[position];
    }

    public void onCancel(DialogInterface aDialog) {
        JSONObject ret = new JSONObject();
        try {
            ret.put("button", -1);
        } catch(Exception ex) { }
        finishDialog(ret.toString());
    }

    static SynchronousQueue<String> mPromptQueue = new SynchronousQueue<String>();

    static public String waitForReturn() throws InterruptedException {
        String value;

        while (null == (value = mPromptQueue.poll(1, TimeUnit.MILLISECONDS))) {
            GeckoAppShell.processNextNativeEvent();
        }

        return value;
    }

    public void finishDialog(String aReturn) {
        mInputs = null;
        mDialog = null;
        mSelected = null;
        try {
            mPromptQueue.put(aReturn);
        } catch(Exception ex) { }
    }

    public void processMessage(JSONObject geckoObject) {
        String title = "";
        try {
            title = geckoObject.getString("title");
        } catch(Exception ex) { }
        String text = "";
        try {
            text = geckoObject.getString("text");
        } catch(Exception ex) { }

        JSONArray buttons = new JSONArray();
        try {
            buttons = geckoObject.getJSONArray("buttons");
        } catch(Exception ex) { }
        int length = buttons.length();
        PromptButton[] promptbuttons = new PromptButton[length];
        for (int i = 0; i < length; i++) {
            try {
                promptbuttons[i] = new PromptButton(buttons.getJSONObject(i));
            } catch(Exception ex) { }
        }

        JSONArray inputs = new JSONArray();
        try {
            inputs = geckoObject.getJSONArray("inputs");
        } catch(Exception ex) { }
        length = inputs.length();
        mInputs = new PromptInput[length];
        for (int i = 0; i < length; i++) {
            try {
                mInputs[i] = new PromptInput(inputs.getJSONObject(i));
            } catch(Exception ex) { }
        }

        PromptListItem[] menuitems = getListItemArray(geckoObject, "listitems");
        mSelected = getBooleanArray(geckoObject, "selected");
        boolean multiple = false;
        try {
            multiple = geckoObject.getBoolean("multiple");
        } catch(Exception ex) { }
        this.Show(title, text, promptbuttons, menuitems, multiple);
    }

    private String[] getStringArray(JSONObject aObject, String aName) {
        JSONArray items = new JSONArray();
        try {
            items = aObject.getJSONArray(aName);
        } catch(Exception ex) { }
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
        JSONArray items = new JSONArray();
        try {
            items = aObject.getJSONArray(aName);
        } catch(Exception ex) { }
        int length = items.length();
        PromptListItem[] list = new PromptListItem[length];
        for (int i = 0; i < length; i++) {
            try {
                list[i] = new PromptListItem(items.getJSONObject(i));
            } catch(Exception ex) { }
        }
        return list;
    }

    static public class PromptListItem {
        public String label = "";
        public boolean isGroup = false;
        public boolean inGroup = false;
        public boolean disabled = false;
        public int id = 0;

        // This member can't be accessible from JS, see bug 733749.
        public Drawable icon = null;

        PromptListItem(JSONObject aObject) {
            try { label = aObject.getString("label"); } catch(Exception ex) { }
            try { isGroup = aObject.getBoolean("isGroup"); } catch(Exception ex) { }
            try { inGroup = aObject.getBoolean("inGroup"); } catch(Exception ex) { }
            try { disabled = aObject.getBoolean("disabled"); } catch(Exception ex) { }
            try { id = aObject.getInt("id"); } catch(Exception ex) { }
        }

        public PromptListItem(String aLabel) {
            label = aLabel;
        }
    }

    public class PromptListAdapter extends ArrayAdapter<PromptListItem> {
        private static final int VIEW_TYPE_ITEM = 0;
        private static final int VIEW_TYPE_GROUP = 1;
        private static final int VIEW_TYPE_COUNT = 2;

        public ListView listView = null;
    	private PromptListItem[] mList;
    	private int mResourceId = -1;
    	PromptListAdapter(Context context, int textViewResourceId, PromptListItem[] objects) {
            super(context, textViewResourceId, objects);
            mList = objects;
            mResourceId = textViewResourceId;
    	}

        public int getCount() {
            return mList.length;
        }

        public PromptListItem getItem(int position) {
            return mList[position];
        }

        public long getItemId(int position) {
            return mList[position].id;
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
            if (item.icon == null)
                return;

            Resources res = GeckoApp.mAppContext.getResources();

            // Set padding inside the item.
            t.setPadding(item.inGroup ? mLeftRightTextWithIconPadding + mGroupPaddingSize :
                                        mLeftRightTextWithIconPadding,
                         mTopBottomTextWithIconPadding,
                         mLeftRightTextWithIconPadding,
                         mTopBottomTextWithIconPadding);

            // Set the padding between the icon and the text.
            t.setCompoundDrawablePadding(mIconTextPadding);

            // We want the icon to be of a specific size. Some do not
            // follow this rule so we have to resize them.
            Bitmap bitmap = ((BitmapDrawable) item.icon).getBitmap();
            Drawable d = new BitmapDrawable(Bitmap.createScaledBitmap(bitmap, mIconSize, mIconSize, true));

            t.setCompoundDrawablesWithIntrinsicBounds(d, null, null, null);
        }

        private void maybeUpdateCheckedState(int position, PromptListItem item, ViewHolder viewHolder) {
            if (item.isGroup)
                return;

            CheckedTextView ct;
            try {
                ct = (CheckedTextView) viewHolder.textView;
            } catch (Exception e) {
                return;
            }

            ct.setEnabled(!item.disabled);
            ct.setClickable(item.disabled);

            // Apparently just using ct.setChecked(true) doesn't work, so this
            // is stolen from the android source code as a way to set the checked
            // state of these items
            if (listView != null)
                listView.setItemChecked(position, mSelected[position]);

            ct.setPadding((item.inGroup ? mGroupPaddingSize : viewHolder.paddingLeft),
                          viewHolder.paddingTop,
                          viewHolder.paddingRight,
                          viewHolder.paddingBottom);
        }

        public View getView(int position, View convertView, ViewGroup parent) {
            PromptListItem item = getItem(position);
            ViewHolder viewHolder = null;

            if (convertView == null) {
                int resourceId = mResourceId;
                if (item.isGroup) {
                    resourceId = R.layout.list_item_header;
                }

                convertView = mInflater.inflate(resourceId, null);

                viewHolder = new ViewHolder();
                viewHolder.textView = (TextView) convertView.findViewById(android.R.id.text1);

                viewHolder.paddingLeft = viewHolder.textView.getPaddingLeft();
                viewHolder.paddingRight = viewHolder.textView.getPaddingRight();
                viewHolder.paddingTop = viewHolder.textView.getPaddingTop();
                viewHolder.paddingBottom = viewHolder.textView.getPaddingBottom();

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
            public TextView textView;
            public int paddingLeft;
            public int paddingRight;
            public int paddingTop;
            public int paddingBottom;
        }
    }
}
