/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.prompts;

import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.GregorianCalendar;

import org.json.JSONObject;
import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.widget.AllCapsTextView;
import org.mozilla.gecko.widget.DateTimePicker;
import org.mozilla.gecko.widget.FloatingHintEditText;

import android.content.Context;
import android.content.res.Configuration;
import android.text.Html;
import android.text.InputType;
import android.text.TextUtils;
import android.text.format.DateFormat;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.inputmethod.InputMethodManager;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.DatePicker;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.TimePicker;

public class PromptInput {
    protected final String mLabel;
    protected final String mType;
    protected final String mId;
    protected final String mValue;
    protected OnChangeListener mListener;
    protected View mView;
    public static final String LOGTAG = "GeckoPromptInput";

    public interface OnChangeListener {
        public void onChange(PromptInput input);
    }

    public void setListener(OnChangeListener listener) {
        mListener = listener;
    }

    public static class EditInput extends PromptInput {
        protected final String mHint;
        protected final boolean mAutofocus;
        public static final String INPUT_TYPE = "textbox";

        public EditInput(JSONObject object) {
            super(object);
            mHint = object.optString("hint");
            mAutofocus = object.optBoolean("autofocus");
        }

        public View getView(final Context context) throws UnsupportedOperationException {
            EditText input = new FloatingHintEditText(context);
            input.setInputType(InputType.TYPE_CLASS_TEXT);
            input.setText(mValue);

            if (!TextUtils.isEmpty(mHint)) {
                input.setHint(mHint);
            }

            if (mAutofocus) {
                input.setOnFocusChangeListener(new View.OnFocusChangeListener() {
                    @Override
                    public void onFocusChange(View v, boolean hasFocus) {
                        if (hasFocus) {
                            ((InputMethodManager) context.getSystemService(Context.INPUT_METHOD_SERVICE)).showSoftInput(v, 0);
                        }
                    }
                });
                input.requestFocus();
            }

            mView = (View)input;
            return mView;
        }

        @Override
        public Object getValue() {
            EditText edit = (EditText)mView;
            return edit.getText();
        }
    }

    public static class NumberInput extends EditInput {
        public static final String INPUT_TYPE = "number";
        public NumberInput(JSONObject obj) {
            super(obj);
        }

        public View getView(final Context context) throws UnsupportedOperationException {
            EditText input = (EditText) super.getView(context);
            input.setRawInputType(Configuration.KEYBOARD_12KEY);
            input.setInputType(InputType.TYPE_CLASS_NUMBER |
                               InputType.TYPE_NUMBER_FLAG_SIGNED);
            return input;
        }
    }

    public static class PasswordInput extends EditInput {
        public static final String INPUT_TYPE = "password";
        public PasswordInput(JSONObject obj) {
            super(obj);
        }

        public View getView(Context context) throws UnsupportedOperationException {
            EditText input = (EditText) super.getView(context);
            input.setInputType(InputType.TYPE_CLASS_TEXT |
                               InputType.TYPE_TEXT_VARIATION_PASSWORD |
                               InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
            return input;
        }

        @Override
        public Object getValue() {
            EditText edit = (EditText)mView;
            return edit.getText();
        }
    }

    public static class CheckboxInput extends PromptInput {
        public static final String INPUT_TYPE = "checkbox";
        private boolean mChecked;

        public CheckboxInput(JSONObject obj) {
            super(obj);
            mChecked = obj.optBoolean("checked");
        }

        public View getView(Context context) throws UnsupportedOperationException {
            CheckBox checkbox = new CheckBox(context);
            checkbox.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
            checkbox.setText(mLabel);
            checkbox.setChecked(mChecked);
            mView = (View)checkbox;
            return mView;
        }

        @Override
        public Object getValue() {
            CheckBox checkbox = (CheckBox)mView;
            return checkbox.isChecked() ? Boolean.TRUE : Boolean.FALSE;
        }
    }

    public static class DateTimeInput extends PromptInput {
        public static final String[] INPUT_TYPES = new String[] {
            "date",
            "week",
            "time",
            "datetime-local",
            "datetime",
            "month"
        };

        public DateTimeInput(JSONObject obj) {
            super(obj);
        }

        public View getView(Context context) throws UnsupportedOperationException {
            if (mType.equals("date")) {
                try {
                    DateTimePicker input = new DateTimePicker(context, "yyyy-MM-dd", mValue,
                                                              DateTimePicker.PickersState.DATE);
                    input.toggleCalendar(true);
                    mView = (View)input;
                } catch (UnsupportedOperationException ex) {
                    // We can't use our custom version of the DatePicker widget because the sdk is too old.
                    // But we can fallback on the native one.
                    DatePicker input = new DatePicker(context);
                    try {
                        if (!TextUtils.isEmpty(mValue)) {
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
                DateTimePicker input = new DateTimePicker(context, "yyyy-'W'ww", mValue,
                                                          DateTimePicker.PickersState.WEEK);
                mView = (View)input;
            } else if (mType.equals("time")) {
                TimePicker input = new TimePicker(context);
                input.setIs24HourView(DateFormat.is24HourFormat(context));

                GregorianCalendar calendar = new GregorianCalendar();
                if (!TextUtils.isEmpty(mValue)) {
                    try {
                        calendar.setTime(new SimpleDateFormat("HH:mm").parse(mValue));
                    } catch (Exception e) { }
                }
                input.setCurrentHour(calendar.get(GregorianCalendar.HOUR_OF_DAY));
                input.setCurrentMinute(calendar.get(GregorianCalendar.MINUTE));
                mView = (View)input;
            } else if (mType.equals("datetime-local") || mType.equals("datetime")) {
                DateTimePicker input = new DateTimePicker(context, "yyyy-MM-dd HH:mm", mValue,
                                                          DateTimePicker.PickersState.DATETIME);
                input.toggleCalendar(true);
                mView = (View)input;
            } else if (mType.equals("month")) {
                DateTimePicker input = new DateTimePicker(context, "yyyy-MM", mValue,
                                                          DateTimePicker.PickersState.MONTH);
                mView = (View)input;
            }
            return mView;
        }

        private static String formatDateString(String dateFormat, Calendar calendar) {
            return new SimpleDateFormat(dateFormat).format(calendar.getTime());
        }

        @Override
        public Object getValue() {
            if (Versions.preHC && mType.equals("date")) {
                // We can't use the custom DateTimePicker with a sdk older than 11.
                // Fallback on the native DatePicker.
                DatePicker dp = (DatePicker)mView;
                GregorianCalendar calendar =
                    new GregorianCalendar(dp.getYear(),dp.getMonth(),dp.getDayOfMonth());
                return formatDateString("yyyy-MM-dd",calendar);
            } else if (mType.equals("time")) {
                TimePicker tp = (TimePicker)mView;
                GregorianCalendar calendar =
                    new GregorianCalendar(0,0,0,tp.getCurrentHour(),tp.getCurrentMinute());
                return formatDateString("HH:mm",calendar);
            } else {
                DateTimePicker dp = (DateTimePicker)mView;
                GregorianCalendar calendar = new GregorianCalendar();
                calendar.setTimeInMillis(dp.getTimeInMillis());
                if (mType.equals("date")) {
                    return formatDateString("yyyy-MM-dd",calendar);
                } else if (mType.equals("week")) {
                    return formatDateString("yyyy-'W'ww",calendar);
                } else if (mType.equals("datetime-local")) {
                    return formatDateString("yyyy-MM-dd HH:mm",calendar);
                } else if (mType.equals("datetime")) {
                    calendar.set(GregorianCalendar.ZONE_OFFSET,0);
                    calendar.setTimeInMillis(dp.getTimeInMillis());
                    return formatDateString("yyyy-MM-dd HH:mm",calendar);
                } else if (mType.equals("month")) {
                    return formatDateString("yyyy-MM",calendar);
                }
            }
            return super.getValue();
        }
    }

    public static class MenulistInput extends PromptInput {
        public static final String INPUT_TYPE = "menulist";
        private static String[] mListitems;
        private static int mSelected;

        public Spinner spinner;
        public AllCapsTextView textView;

        public MenulistInput(JSONObject obj) {
            super(obj);
            mListitems = Prompt.getStringArray(obj, "values");
            mSelected = obj.optInt("selected");
        }

        public View getView(final Context context) throws UnsupportedOperationException {
            if (Versions.preHC) {
                spinner = new Spinner(context);
            } else {
                spinner = new Spinner(context, Spinner.MODE_DIALOG);
            }
            try {
                if (mListitems.length > 0) {
                    ArrayAdapter<String> adapter = new ArrayAdapter<String>(context, android.R.layout.simple_spinner_item, mListitems);
                    adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);

                    spinner.setAdapter(adapter);
                    spinner.setSelection(mSelected);
                }
            } catch (Exception ex) {
            }

            if (!TextUtils.isEmpty(mLabel)) {
                LinearLayout container = new LinearLayout(context);
                container.setOrientation(LinearLayout.VERTICAL);

                textView = new AllCapsTextView(context, null);
                textView.setText(mLabel);
                container.addView(textView);

                container.addView(spinner);
                return container;
            }

            return spinner;
        }

        @Override
        public Object getValue() {
            return new Integer(spinner.getSelectedItemPosition());
        }
    }

    public static class LabelInput extends PromptInput {
        public static final String INPUT_TYPE = "label";
        public LabelInput(JSONObject obj) {
            super(obj);
        }

        public View getView(Context context) throws UnsupportedOperationException {
            // not really an input, but a way to add labels and such to the dialog
            TextView view = new TextView(context);
            view.setText(Html.fromHtml(mLabel));
            mView = view;
            return mView;
        }
    }

    public PromptInput(JSONObject obj) {
        mLabel = obj.optString("label");
        mType = obj.optString("type");
        String id = obj.optString("id");
        mId = TextUtils.isEmpty(id) ? mType : id;
        mValue = obj.optString("value");
    }

    public static PromptInput getInput(JSONObject obj) {
        String type = obj.optString("type");
        if (EditInput.INPUT_TYPE.equals(type)) {
            return new EditInput(obj);
        } else if (NumberInput.INPUT_TYPE.equals(type)) {
            return new NumberInput(obj);
        } else if (PasswordInput.INPUT_TYPE.equals(type)) {
            return new PasswordInput(obj);
        } else if (CheckboxInput.INPUT_TYPE.equals(type)) {
            return new CheckboxInput(obj);
        } else if (MenulistInput.INPUT_TYPE.equals(type)) {
            return new MenulistInput(obj);
        } else if (LabelInput.INPUT_TYPE.equals(type)) {
            return new LabelInput(obj);
        } else if (IconGridInput.INPUT_TYPE.equals(type)) {
            return new IconGridInput(obj);
        } else if (ColorPickerInput.INPUT_TYPE.equals(type)) {
            return new ColorPickerInput(obj);
        } else if (TabInput.INPUT_TYPE.equals(type)) {
            return new TabInput(obj);
        } else {
            for (String dtType : DateTimeInput.INPUT_TYPES) {
                if (dtType.equals(type)) {
                    return new DateTimeInput(obj);
                }
            }
        }
        return null;
    }

    public View getView(Context context) throws UnsupportedOperationException {
        return null;
    }

    public String getId() {
        return mId;
    }

    public Object getValue() {
        return null;
    }

    public boolean getScrollable() {
        return false;
    }

    public boolean canApplyInputStyle() {
        return true;
    }

    protected void notifyListeners(String val) {
        if (mListener != null) {
            mListener.onChange(this);
        }
    }
}
