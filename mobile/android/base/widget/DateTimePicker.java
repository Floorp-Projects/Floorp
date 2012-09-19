/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


package org.mozilla.gecko.widget;

import android.content.Context;
import android.graphics.Point;
import android.os.Build;
import android.text.format.DateFormat;
import android.text.format.DateUtils;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.SparseArray;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.Display;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.OrientationEventListener;
import android.view.WindowManager;
import android.widget.CalendarView;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.NumberPicker;
import android.widget.TextView;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Arrays;
import java.util.Calendar;
import java.util.Locale;
import java.util.TimeZone;

import org.mozilla.gecko.R;

public class DateTimePicker extends FrameLayout {

    private static final boolean DEBUG = true;
    private static final String LOGTAG = "GeckoDateTimePicker";
    private static final String DATE_FORMAT = "MM/dd/yyyy";
    private static final int DEFAULT_START_YEAR = 1;
    private static final int DEFAULT_END_YEAR = 9999;
    // Minimal screen width (in inches) for which we can show the calendar;
    private static final int SCREEN_SIZE_THRESHOLD = 5;
    private boolean mYearEnabled = true;
    private boolean mMonthEnabled = true;
    private boolean mWeekEnabled = false;
    private boolean mDayEnabled = true;
    private boolean mHourEnabled = true;
    private boolean mMinuteEnabled = true;
    private boolean mCalendarEnabled = false;
    // Size of the screen in inches;
    private int mScreenWidth;
    private int mScreenHeight;
    private OnValueChangeListener mOnChangeListener;
    private final LinearLayout mPickers;
    private final LinearLayout mDateSpinners;
    private final LinearLayout mTimeSpinners;
    private final LinearLayout mSpinners;
    private final NumberPicker mDaySpinner;
    private final NumberPicker mMonthSpinner;
    private final NumberPicker mWeekSpinner;
    private final NumberPicker mYearSpinner;
    private final NumberPicker mHourSpinner;
    private final NumberPicker mMinuteSpinner;
    private final CalendarView mCalendar;
    private final EditText mDaySpinnerInput;
    private final EditText mMonthSpinnerInput;
    private final EditText mWeekSpinnerInput;
    private final EditText mYearSpinnerInput;
    private final EditText mHourSpinnerInput;
    private final EditText mMinuteSpinnerInput;
    private Locale mCurrentLocale;
    private String[] mShortMonths;
    private int mNumberOfMonths;
    private Calendar mTempDate;
    private Calendar mMinDate;
    private Calendar mMaxDate;
    private Calendar mCurrentDate;
    private pickersState mState;

    public static enum pickersState { DATE, MONTH, WEEK, TIME, DATETIME };

    public class OnValueChangeListener implements NumberPicker.OnValueChangeListener {
        public void onValueChange(NumberPicker picker, int oldVal, int newVal) {
            updateInputState();
            mTempDate.setTimeInMillis(mCurrentDate.getTimeInMillis());
            boolean newBehavior = (Build.VERSION.SDK_INT > 10);
            if (newBehavior) {
                if (DEBUG) Log.d(LOGTAG, "Sdk version > 10, using new behavior");
                //The native date picker widget on these sdks increment
                //the next field when one field reach the maximum
                if (picker == mDaySpinner && mDayEnabled) {
                    int maxDayOfMonth = mTempDate.getActualMaximum(Calendar.DAY_OF_MONTH);
                    int old = mTempDate.get(Calendar.DAY_OF_MONTH);
                    setTempDate(Calendar.DAY_OF_MONTH, old, newVal, 1, maxDayOfMonth);
                } else if (picker == mMonthSpinner && mMonthEnabled) {
                    int old = mTempDate.get(Calendar.MONTH);
                    setTempDate(Calendar.MONTH, old, newVal, 0, 11);
                } else if (picker == mWeekSpinner) {
                    int old = mTempDate.get(Calendar.WEEK_OF_YEAR);
                    int maxWeekOfYear = mTempDate.getActualMaximum(Calendar.WEEK_OF_YEAR);
                    setTempDate(Calendar.WEEK_OF_YEAR, old, newVal, 0, maxWeekOfYear);
                } else if (picker == mYearSpinner && mYearEnabled) {
                    int month=mTempDate.get(Calendar.MONTH);
                    mTempDate.set(Calendar.YEAR,newVal);
                    // Changing the year shouldn't change the month. (in case of non-leap year a Feb 29)
                    // change the day instead;
                    if (month != mTempDate.get(Calendar.MONTH)){
                        mTempDate.set(Calendar.MONTH, month);
                        mTempDate.set(Calendar.DAY_OF_MONTH,
                                mTempDate.getActualMaximum(Calendar.DAY_OF_MONTH));
                    }
                } else if (picker == mHourSpinner && mHourEnabled) {
                    setTempDate(Calendar.HOUR_OF_DAY, oldVal, newVal, 0, 23);
                } else if (picker == mMinuteSpinner && mMinuteEnabled) {
                    setTempDate(Calendar.MINUTE, oldVal, newVal, 0, 59);
                } else {
                    throw new IllegalArgumentException();
                }
            } else {
                if (DEBUG) Log.d(LOGTAG,"Sdk version < 10, using old behavior");
                if (picker == mDaySpinner && mDayEnabled){
                    mTempDate.set(Calendar.DAY_OF_MONTH, newVal);
                } else if (picker == mMonthSpinner && mMonthEnabled){
                    mTempDate.set(Calendar.MONTH, newVal);
                    if (mTempDate.get(Calendar.MONTH) == newVal+1){
                        mTempDate.set(Calendar.MONTH, newVal);
                        mTempDate.set(Calendar.DAY_OF_MONTH,
                                mTempDate.getActualMaximum(Calendar.DAY_OF_MONTH));
                    }
                } else if (picker == mWeekSpinner){
                    mTempDate.set(Calendar.WEEK_OF_YEAR, newVal);
                } else if (picker == mYearSpinner && mYearEnabled){
                    int month=mTempDate.get(Calendar.MONTH);
                    mTempDate.set(Calendar.YEAR, newVal);
                    if (month != mTempDate.get(Calendar.MONTH)){
                        mTempDate.set(Calendar.MONTH, month);
                        mTempDate.set(Calendar.DAY_OF_MONTH,
                                mTempDate.getActualMaximum(Calendar.DAY_OF_MONTH));
                    }
                } else if (picker == mHourSpinner && mHourEnabled){
                    mTempDate.set(Calendar.HOUR_OF_DAY, newVal);
                } else if (picker == mMinuteSpinner && mMinuteEnabled){
                    mTempDate.set(Calendar.MINUTE, newVal);
                } else {
                  throw new IllegalArgumentException();
                }
            }
            setDate(mTempDate);
            if (mDayEnabled) {
                mDaySpinner.setMaxValue(mCurrentDate.getActualMaximum(Calendar.DAY_OF_MONTH));
            }
            if(mWeekEnabled) {
               mWeekSpinner.setMaxValue(mCurrentDate.getActualMaximum(Calendar.WEEK_OF_YEAR));
            }
            updateCalendar();
            updateSpinners();
            notifyDateChanged();
        }

        private void setTempDate(int field, int oldVal, int newVal, int min, int max) {
            if (oldVal == max && newVal == min ) {
                mTempDate.add(field, 1);
            } else if (oldVal == min && newVal == max) {
                mTempDate.add(field, -1);
            } else {
                mTempDate.add(field, newVal - oldVal);
            }
        }
    }

    private static final NumberPicker.Formatter TWO_DIGIT_FORMATTER = new NumberPicker.Formatter() {
        final StringBuilder mBuilder = new StringBuilder();

        final java.util.Formatter mFmt = new java.util.Formatter(mBuilder, java.util.Locale.US);

        final Object[] mArgs = new Object[1];

        public String format(int value) {
            mArgs[0] = value;
            mBuilder.delete(0, mBuilder.length());
            mFmt.format("%02d", mArgs);
            return mFmt.toString();
        }
    };

    private void displayPickers() {
        setWeekShown(false);
        if (mState == pickersState.DATETIME) {
            return;
        }
        setHourShown(false);
        setMinuteShown(false);
        if (mState == pickersState.WEEK) {
            setDayShown(false);
            setMonthShown(false);
            setWeekShown(true);
        } else if (mState == pickersState.MONTH) {
            setDayShown(false);
        }
    }

    public DateTimePicker(Context context) {
        this(context, "", "", pickersState.DATE);
    }

    public DateTimePicker(Context context, String dateFormat, String dateTimeValue, pickersState state) {
        super(context);
        if (Build.VERSION.SDK_INT < 11) {
            throw new UnsupportedOperationException("Custom DateTimePicker is only available for SDK > 10");
        }
        setCurrentLocale(Locale.getDefault());
        mMinDate.set(DEFAULT_START_YEAR,1,1);
        mMaxDate.set(DEFAULT_END_YEAR,12,31);
        mState = state;
        LayoutInflater inflater = LayoutInflater.from(context);
        inflater.inflate(R.layout.datetime_picker, this, true);

        mOnChangeListener = new OnValueChangeListener();

        mDateSpinners = (LinearLayout)findViewById(R.id.date_spinners);
        mTimeSpinners = (LinearLayout)findViewById(R.id.time_spinners);
        mSpinners = (LinearLayout)findViewById(R.id.spinners);
        mPickers = (LinearLayout)findViewById(R.id.datetime_picker);

        // We will display differently according to the screen size width.
        WindowManager wm = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        Display display = wm.getDefaultDisplay();
        DisplayMetrics dm = new DisplayMetrics();
        display.getMetrics(dm);
        mScreenWidth = display.getWidth() / dm.densityDpi;
        mScreenHeight = display.getHeight() / dm.densityDpi;
        if (DEBUG) Log.d(LOGTAG, "screen width: " + mScreenWidth + " screen height: " + mScreenHeight);

        // If we're displaying a date, the screen is wide enought (and if we're using a sdk where the calendar view exists)
        // then display a calendar.
        if ((mState == pickersState.DATE || mState == pickersState.DATETIME) &&
            Build.VERSION.SDK_INT > 10 && mScreenWidth >= SCREEN_SIZE_THRESHOLD) {
            if (DEBUG) Log.d(LOGTAG,"SDK > 10 and screen wide enough, displaying calendar");
            mCalendar = new CalendarView(context);
            mCalendar.setVisibility(GONE);

            LayoutParams layoutParams = new LayoutParams(250,280);
            mCalendar.setLayoutParams(layoutParams);
            mCalendar.setFocusable(true);
            mCalendar.setFocusableInTouchMode(true);
            mCalendar.setMaxDate(mMaxDate.getTimeInMillis());
            mCalendar.setMinDate(mMinDate.getTimeInMillis());

            mCalendar.setOnDateChangeListener(new CalendarView.OnDateChangeListener() {
                public void onSelectedDayChange(
                    CalendarView view, int year, int month, int monthDay) {
                    mTempDate.set(year, month, monthDay);
                    setDate(mTempDate);
                    notifyDateChanged();
                }
            });

            mPickers.addView(mCalendar);
        } else {
          // If the screen is more wide than high, we are displaying daye and time spinners,
          // and if there is no calendar displayed,
          // we should display the fields in one row.
            if (mScreenWidth > mScreenHeight && mState == pickersState.DATETIME) {
                mSpinners.setOrientation(LinearLayout.HORIZONTAL);
            }
            mCalendar = null;
        }

        // Find the initial date from the constructor arguments.
        try {
            if (!dateTimeValue.equals("")) {
                mTempDate.setTime(new SimpleDateFormat(dateFormat).parse(dateTimeValue));
            } else {
                mTempDate.setTimeInMillis(System.currentTimeMillis());
            }
        } catch (Exception ex) {
            Log.e(LOGTAG, "Error parsing format string: " + ex);
            mTempDate.setTimeInMillis(System.currentTimeMillis());
        }

        // Initialize all spinners.
        mDaySpinner = setupSpinner(R.id.day, 1,
                                   mTempDate.get(Calendar.DAY_OF_MONTH));
        mDaySpinner.setFormatter(TWO_DIGIT_FORMATTER);
        mDaySpinnerInput = (EditText) mDaySpinner.getChildAt(1);

        mMonthSpinner = setupSpinner(R.id.month, 1,
                                     mTempDate.get(Calendar.MONTH));
        mMonthSpinner.setFormatter(TWO_DIGIT_FORMATTER);
        mMonthSpinner.setDisplayedValues(mShortMonths);
        mMonthSpinnerInput = (EditText) mMonthSpinner.getChildAt(1);

        mWeekSpinner = setupSpinner(R.id.week, 1,
                                    mTempDate.get(Calendar.WEEK_OF_YEAR));
        mWeekSpinner.setFormatter(TWO_DIGIT_FORMATTER);
        mWeekSpinnerInput = (EditText) mWeekSpinner.getChildAt(1);

        mYearSpinner = setupSpinner(R.id.year, DEFAULT_START_YEAR,
                                    DEFAULT_END_YEAR);
        mYearSpinnerInput = (EditText) mYearSpinner.getChildAt(1);

        mHourSpinner = setupSpinner(R.id.hour, 0, 23);
        mHourSpinner.setFormatter(TWO_DIGIT_FORMATTER);
        mHourSpinnerInput = (EditText) mHourSpinner.getChildAt(1);

        mMinuteSpinner = setupSpinner(R.id.minute, 0, 59);
        mMinuteSpinner.setFormatter(TWO_DIGIT_FORMATTER);
        mMinuteSpinnerInput = (EditText) mMinuteSpinner.getChildAt(1);

        // The order in which the spinners are displayed are locale-dependent
        reorderDateSpinners();
        // Set the date to the initial date. Since this date can come from the user,
        // it can fire an exception (out-of-bound date)
        try {
          updateDate(mTempDate);
        } catch (Exception ex) { }

        // Display only the pickers needed for the current state.
        displayPickers();
    }

    public NumberPicker setupSpinner(int id, int min, int max) {
        NumberPicker mSpinner = (NumberPicker) findViewById(id);
        mSpinner.setMinValue(min);
        mSpinner.setMaxValue(max);
        mSpinner.setOnValueChangedListener(mOnChangeListener);
        mSpinner.setOnLongPressUpdateInterval(100);
        return mSpinner;
    }

    public long getTimeInMillis(){
        return mCurrentDate.getTimeInMillis();
    }

    private void reorderDateSpinners() {
        mDateSpinners.removeAllViews();
        char[] order = DateFormat.getDateFormatOrder(getContext());
        final int spinnerCount = order.length;
        for (int i = 0; i < spinnerCount; i++) {
            switch (order[i]) {
                case DateFormat.DATE:
                    mDateSpinners.addView(mDaySpinner);
                    break;
                case DateFormat.MONTH:
                    mDateSpinners.addView(mMonthSpinner);
                    break;
                case DateFormat.YEAR:
                    mDateSpinners.addView(mYearSpinner);
                    break;
                default:
                    throw new IllegalArgumentException();
            }
        }
        mDateSpinners.addView(mWeekSpinner);
    }

    private void setDate(Calendar calendar){
        mCurrentDate = mTempDate;
        if (mCurrentDate.before(mMinDate)) {
            mCurrentDate.setTimeInMillis(mMinDate.getTimeInMillis());
        } else if (mCurrentDate.after(mMaxDate)) {
            mCurrentDate.setTimeInMillis(mMaxDate.getTimeInMillis());
        }
    }

    private void updateInputState() {
        InputMethodManager inputMethodManager = (InputMethodManager)
          getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (mYearEnabled && inputMethodManager.isActive(mYearSpinnerInput)) {
            mYearSpinnerInput.clearFocus();
            inputMethodManager.hideSoftInputFromWindow(getWindowToken(), 0);
        } else if (mMonthEnabled && inputMethodManager.isActive(mMonthSpinnerInput)) {
            mMonthSpinnerInput.clearFocus();
            inputMethodManager.hideSoftInputFromWindow(getWindowToken(), 0);
        } else if (mDayEnabled && inputMethodManager.isActive(mDaySpinnerInput)) {
            mDaySpinnerInput.clearFocus();
            inputMethodManager.hideSoftInputFromWindow(getWindowToken(), 0);
        } else if (mHourEnabled && inputMethodManager.isActive(mHourSpinnerInput)) {
            mHourSpinnerInput.clearFocus();
            inputMethodManager.hideSoftInputFromWindow(getWindowToken(), 0);
        } else if (mMinuteEnabled && inputMethodManager.isActive(mMinuteSpinnerInput)) {
            mMinuteSpinnerInput.clearFocus();
            inputMethodManager.hideSoftInputFromWindow(getWindowToken(), 0);
        }
    }

    private void updateSpinners() {
        if (mDayEnabled) {
            if (mCurrentDate.equals(mMinDate)) {
                mDaySpinner.setMinValue(mCurrentDate.get(Calendar.DAY_OF_MONTH));
                mDaySpinner.setMaxValue(mCurrentDate.getActualMaximum(Calendar.DAY_OF_MONTH));
            } else if (mCurrentDate.equals(mMaxDate)) {
                mDaySpinner.setMinValue(mCurrentDate.getActualMinimum(Calendar.DAY_OF_MONTH));
                mDaySpinner.setMaxValue(mCurrentDate.get(Calendar.DAY_OF_MONTH));
            } else {
                mDaySpinner.setMinValue(1);
                mDaySpinner.setMaxValue(mCurrentDate.getActualMaximum(Calendar.DAY_OF_MONTH));
            }
            mDaySpinner.setValue(mCurrentDate.get(Calendar.DAY_OF_MONTH));
        }

        if (mWeekEnabled) {
            mWeekSpinner.setMinValue(1);
            mWeekSpinner.setMaxValue(mCurrentDate.getActualMaximum(Calendar.WEEK_OF_YEAR));
            mWeekSpinner.setValue(mCurrentDate.get(Calendar.WEEK_OF_YEAR));
        }

        if (mMonthEnabled) {
            mMonthSpinner.setDisplayedValues(null);
            if (mCurrentDate.equals(mMinDate)) {
                mMonthSpinner.setMinValue(mCurrentDate.get(Calendar.MONTH));
                mMonthSpinner.setMaxValue(mCurrentDate.getActualMaximum(Calendar.MONTH));
            } else if (mCurrentDate.equals(mMaxDate)) {
                mMonthSpinner.setMinValue(mCurrentDate.getActualMinimum(Calendar.MONTH));
                mMonthSpinner.setMaxValue(mCurrentDate.get(Calendar.MONTH));
            } else {
                mMonthSpinner.setMinValue(0);
                mMonthSpinner.setMaxValue(11);
            }

            String[] displayedValues = Arrays.copyOfRange(mShortMonths,
                    mMonthSpinner.getMinValue(), mMonthSpinner.getMaxValue() + 1);
            mMonthSpinner.setDisplayedValues(displayedValues);
            mMonthSpinner.setValue(mCurrentDate.get(Calendar.MONTH));
        }

        if (mYearEnabled) {
            mYearSpinner.setMinValue(mMinDate.get(Calendar.YEAR));
            mYearSpinner.setMaxValue(mMaxDate.get(Calendar.YEAR));
            mYearSpinner.setValue(mCurrentDate.get(Calendar.YEAR));
        }

        if (mHourEnabled) {
            mHourSpinner.setValue(mCurrentDate.get(Calendar.HOUR_OF_DAY));
        }
        if (mMinuteEnabled) {
            mMinuteSpinner.setValue(mCurrentDate.get(Calendar.MINUTE));
        }
    }

    private void updateCalendar() {
        if (mCalendarEnabled){
            mCalendar.setDate(mCurrentDate.getTimeInMillis(), false, false);
        }
    }

    private void notifyDateChanged() {
        sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_SELECTED);
    }

    public void toggleCalendar(boolean shown) {
        if ((mState != pickersState.DATE && mState != pickersState.DATETIME) ||
            Build.VERSION.SDK_INT < 11 || mScreenWidth < SCREEN_SIZE_THRESHOLD) {
            if (DEBUG) Log.d(LOGTAG,"Cannot display calendar on this device, in this state" +
                ": screen width :"+mScreenWidth);
            return;
        }
        if (shown){
            mCalendarEnabled = true;
            mCalendar.setVisibility(VISIBLE);
            setYearShown(false);
            setWeekShown(false);
            setMonthShown(false);
            setDayShown(false);
        } else {
            mCalendar.setVisibility(GONE);
            setYearShown(true);
            setMonthShown(true);
            setDayShown(true);
            mSpinners.setOrientation(LinearLayout.HORIZONTAL);
            mCalendarEnabled = false;
        }
    }

    private void setYearShown(boolean shown) {
        if (shown) {
            toggleCalendar(false);
            mYearSpinner.setVisibility(VISIBLE);
            mYearEnabled = true;
        } else {
            mYearSpinner.setVisibility(GONE);
            mYearEnabled = false;
        }
    }

    private void setWeekShown(boolean shown) {
        if (shown) {
            toggleCalendar(false);
            mWeekSpinner.setVisibility(VISIBLE);
            mWeekEnabled = true;
        } else {
            mWeekSpinner.setVisibility(GONE);
            mWeekEnabled = false;
        }
    }

    private void setMonthShown(boolean shown) {
        if (shown) {
            toggleCalendar(false);
            mMonthSpinner.setVisibility(VISIBLE);
            mMonthEnabled = true;
        } else {
            mMonthSpinner.setVisibility(GONE);
            mMonthEnabled = false;
        }
    }

    private void setDayShown(boolean shown) {
        if (shown) {
            toggleCalendar(false);
            mDaySpinner.setVisibility(VISIBLE);
            mDayEnabled = true;
        } else {
            mDaySpinner.setVisibility(GONE);
            mDayEnabled = false;
        }
    }

    private void setHourShown(boolean shown) {
        if (shown) {
            mHourSpinner.setVisibility(VISIBLE);
            mHourEnabled= true;
        } else {
            mHourSpinner.setVisibility(GONE);
            mTimeSpinners.setVisibility(GONE);
            mHourEnabled = false;
        }
    }

    private void setMinuteShown(boolean shown) {
        if (shown) {
            mMinuteSpinner.setVisibility(VISIBLE);
            mTimeSpinners.findViewById(R.id.mincolon).setVisibility(VISIBLE);
            mMinuteEnabled= true;
        } else {
            mMinuteSpinner.setVisibility(GONE);
            mTimeSpinners.findViewById(R.id.mincolon).setVisibility(GONE);
            mMinuteEnabled = false;
        }
    }

    private void setCurrentLocale(Locale locale) {
        if (locale.equals(mCurrentLocale)) {
            return;
        }

        mCurrentLocale = locale;

        mTempDate = getCalendarForLocale(mTempDate, locale);
        mMinDate = getCalendarForLocale(mMinDate, locale);
        mMaxDate = getCalendarForLocale(mMaxDate, locale);
        mCurrentDate = getCalendarForLocale(mCurrentDate, locale);

        mNumberOfMonths = mTempDate.getActualMaximum(Calendar.MONTH) + 1;
        mShortMonths = new String[mNumberOfMonths];
        for (int i = 0; i < mNumberOfMonths; i++) {
            mShortMonths[i] = DateUtils.getMonthString(Calendar.JANUARY + i,
                    DateUtils.LENGTH_MEDIUM);
        }
    }

    private Calendar getCalendarForLocale(Calendar oldCalendar, Locale locale) {
        if (oldCalendar == null) {
            return Calendar.getInstance(locale);
        } else {
            final long currentTimeMillis = oldCalendar.getTimeInMillis();
            Calendar newCalendar = Calendar.getInstance(locale);
            newCalendar.setTimeInMillis(currentTimeMillis);
            return newCalendar;
        }
    }

    public void updateDate(Calendar calendar) {
        if (mCurrentDate.equals(calendar)) {
            return;
        }
        mCurrentDate.setTimeInMillis(calendar.getTimeInMillis());
        if (mCurrentDate.before(mMinDate)) {
            mCurrentDate.setTimeInMillis(mMinDate.getTimeInMillis());
        } else if (mCurrentDate.after(mMaxDate)) {
            mCurrentDate.setTimeInMillis(mMaxDate.getTimeInMillis());
        }
        updateSpinners();
        notifyDateChanged();
    }

}

