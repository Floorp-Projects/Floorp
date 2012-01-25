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
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnMultiChoiceClickListener;
import android.content.res.Resources;
import android.graphics.Color;
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
    private final static int PADDING_SIZE = 32; // in dip units
    private static int mPaddingSize = 0; // calculated from PADDING_SIZE. In pixel units

    PromptService() {
        mInflater = LayoutInflater.from(GeckoApp.mAppContext);
        Resources res = GeckoApp.mAppContext.getResources();
        mPaddingSize = (int)TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                                                      PADDING_SIZE,
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

        int length = mInputs.length;
        if (aMenuList.length > 0) {
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

        length = aButtons.length;
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

    public void finishDialog(String aReturn) {
        mInputs = null;
        mDialog = null;
        mSelected = null;
        try {
            GeckoAppShell.sPromptQueue.put(aReturn);
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

    private class PromptListItem {
        public String label = "";
        public boolean isGroup = false;
        public boolean inGroup = false;
        public boolean disabled = false;
        public int id = 0;
        PromptListItem(JSONObject aObject) {
            try { label = aObject.getString("label"); } catch(Exception ex) { }
            try { isGroup = aObject.getBoolean("isGroup"); } catch(Exception ex) { }
            try { inGroup = aObject.getBoolean("inGroup"); } catch(Exception ex) { }
            try { disabled = aObject.getBoolean("disabled"); } catch(Exception ex) { }
            try { id = aObject.getInt("id"); } catch(Exception ex) { }
        }
    }

    public class PromptListAdapter extends ArrayAdapter<PromptListItem> {
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

        public View getView(int position, View convertView, ViewGroup parent) {
            PromptListItem item = getItem(position);
            int resourceId = mResourceId;
            if (item.isGroup) {
                resourceId = R.layout.list_item_header;
            }
            View row = mInflater.inflate(resourceId, null);
            if (!item.isGroup){
                try {
                    CheckedTextView ct = (CheckedTextView)row.findViewById(android.R.id.text1);
                    if (ct != null){
                        ct.setEnabled(!item.disabled);
                        ct.setClickable(item.disabled);

                        // Apparently just using ct.setChecked(true) doesn't work, so this
                        // is stolen from the android source code as a way to set the checked
                        // state of these items
                        if (mSelected[position] && listView != null) {
                            listView.setItemChecked(position, true);
                        }

                        if (item.inGroup) {
                            ct.setPadding(mPaddingSize, 0, 0, 0);
                        }

                    }
                } catch (Exception ex) { }
            }
            TextView t1 = (TextView) row.findViewById(android.R.id.text1);
            if (t1 != null) {
                t1.setText(item.label);
            }

            return row;
        }
    }
}
