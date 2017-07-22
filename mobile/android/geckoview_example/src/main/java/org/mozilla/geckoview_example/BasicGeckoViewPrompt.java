/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview_example;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.net.Uri;
import android.os.Build;
import android.text.InputType;
import android.text.format.DateFormat;
import android.util.Log;
import android.view.InflateException;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.CheckedTextView;
import android.widget.CompoundButton;
import android.widget.DatePicker;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.TimePicker;

import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.Locale;

import org.mozilla.gecko.GeckoView;
import org.mozilla.gecko.util.GeckoBundle;

final class BasicGeckoViewPrompt implements GeckoView.PromptDelegate {
    protected static final String LOGTAG = "BasicGeckoViewPrompt";

    public int filePickerRequestCode = 1;
    private int mFileType;
    private FileCallback mFileCallback;

    private static Activity getActivity(final GeckoView view) {
        if (view != null) {
            final Context context = view.getContext();
            if (context instanceof Activity) {
                return (Activity) context;
            }
        }
        return null;
    }

    private AlertDialog.Builder addCheckbox(final AlertDialog.Builder builder,
                                            ViewGroup parent,
                                            final AlertCallback callback) {
        if (!callback.hasCheckbox()) {
            return builder;
        }
        final CheckBox checkbox = new CheckBox(builder.getContext());
        if (callback.getCheckboxMessage() != null) {
            checkbox.setText(callback.getCheckboxMessage());
        }
        checkbox.setChecked(callback.getCheckboxValue());
        checkbox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(final CompoundButton button,
                                         final boolean checked) {
                callback.setCheckboxValue(checked);
            }
        });
        if (parent == null) {
            final int padding = getViewPadding(builder);
            parent = new FrameLayout(builder.getContext());
            parent.setPadding(/* left */ padding, /* top */ 0,
                              /* right */ padding, /* bottom */ 0);
            builder.setView(parent);
        }
        parent.addView(checkbox);
        return builder;
    }

    public void alert(final GeckoView view, final String title, final String msg,
                      final AlertCallback callback) {
        final Activity activity = getActivity(view);
        if (activity == null) {
            callback.dismiss();
            return;
        }
        final AlertDialog.Builder builder = new AlertDialog.Builder(activity)
                .setTitle(title)
                .setMessage(msg)
                .setPositiveButton(android.R.string.ok, /* onClickListener */ null)
                .setOnDismissListener(new DialogInterface.OnDismissListener() {
                    @Override
                    public void onDismiss(final DialogInterface dialog) {
                        callback.dismiss();
                    }
                });
        addCheckbox(builder, /* parent */ null, callback).show();
    }

    public void promptForButton(final GeckoView view, final String title, final String msg,
                                final String[] btnMsg, final ButtonCallback callback)
    {
        final Activity activity = getActivity(view);
        if (activity == null) {
            callback.dismiss();
            return;
        }
        final AlertDialog.Builder builder = new AlertDialog.Builder(activity)
                .setTitle(title)
                .setMessage(msg)
                .setOnDismissListener(new DialogInterface.OnDismissListener() {
                    @Override
                    public void onDismiss(final DialogInterface dialog) {
                        callback.dismiss();
                    }
                });
        final DialogInterface.OnClickListener listener =
            new DialogInterface.OnClickListener() {
                @Override
                public void onClick(final DialogInterface dialog, final int which) {
                    if (which == DialogInterface.BUTTON_POSITIVE) {
                        callback.confirm(BUTTON_TYPE_POSITIVE);
                    } else if (which == DialogInterface.BUTTON_NEUTRAL) {
                        callback.confirm(BUTTON_TYPE_NEUTRAL);
                    } else if (which == DialogInterface.BUTTON_NEGATIVE) {
                        callback.confirm(BUTTON_TYPE_NEGATIVE);
                    } else {
                        callback.dismiss();
                    }
                }
            };
        if (btnMsg[BUTTON_TYPE_POSITIVE] != null) {
            builder.setPositiveButton(btnMsg[BUTTON_TYPE_POSITIVE], listener);
        }
        if (btnMsg[BUTTON_TYPE_NEUTRAL] != null) {
            builder.setNeutralButton(btnMsg[BUTTON_TYPE_NEUTRAL], listener);
        }
        if (btnMsg[BUTTON_TYPE_NEGATIVE] != null) {
            builder.setNegativeButton(btnMsg[BUTTON_TYPE_NEGATIVE], listener);
        }
        addCheckbox(builder, /* parent */ null, callback).show();
    }

    private int getViewPadding(final AlertDialog.Builder builder) {
        final TypedArray attr = builder.getContext().obtainStyledAttributes(
                new int[] { android.R.attr.listPreferredItemPaddingLeft });
        return attr.getDimensionPixelSize(0, 1);
    }

    private LinearLayout addStandardLayout(final AlertDialog.Builder builder,
                                           final String title, final String msg,
                                           final AlertCallback callback) {
        final ScrollView scrollView = new ScrollView(builder.getContext());
        final LinearLayout container = new LinearLayout(builder.getContext());
        final int horizontalPadding = getViewPadding(builder);
        final int verticalPadding = (msg == null || msg.isEmpty()) ? horizontalPadding : 0;
        container.setOrientation(LinearLayout.VERTICAL);
        container.setPadding(/* left */ horizontalPadding, /* top */ verticalPadding,
                             /* right */ horizontalPadding, /* bottom */ verticalPadding);
        scrollView.addView(container);
        builder.setTitle(title)
               .setMessage(msg)
               .setOnDismissListener(new DialogInterface.OnDismissListener() {
                    @Override
                    public void onDismiss(final DialogInterface dialog) {
                        callback.dismiss();
                    }
                })
               .setView(scrollView);
        return container;
    }

    public void promptForText(final GeckoView view, final String title, final String msg,
                              final String value, final TextCallback callback)
    {
        final Activity activity = getActivity(view);
        if (activity == null) {
            callback.dismiss();
            return;
        }
        final AlertDialog.Builder builder = new AlertDialog.Builder(activity);
        final LinearLayout container = addStandardLayout(builder, title, msg, callback);
        final EditText editText = new EditText(builder.getContext());
        editText.setText(value);
        container.addView(editText);

        builder.setNegativeButton(android.R.string.cancel, /* listener */ null)
               .setPositiveButton(android.R.string.ok,
                                  new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(final DialogInterface dialog, final int which) {
                        callback.confirm(editText.getText().toString());
                    }
                });

        addCheckbox(builder, container, callback).show();
    }

    public void promptForAuth(final GeckoView view, final String title, final String msg,
                              final GeckoBundle options, final AuthCallback callback)
    {
        final Activity activity = getActivity(view);
        if (activity == null) {
            callback.dismiss();
            return;
        }
        final AlertDialog.Builder builder = new AlertDialog.Builder(activity);
        final LinearLayout container = addStandardLayout(builder, title, msg, callback);

        final int flags = options.getInt("flags");
        final int level = options.getInt("level");
        final EditText username;
        if ((flags & AUTH_FLAG_ONLY_PASSWORD) == 0) {
            username = new EditText(builder.getContext());
            username.setHint(R.string.username);
            username.setText(options.getString("username"));
            container.addView(username);
        } else {
            username = null;
        }

        final EditText password = new EditText(builder.getContext());
        password.setHint(R.string.password);
        password.setText(options.getString("password"));
        password.setInputType(InputType.TYPE_CLASS_TEXT |
                              InputType.TYPE_TEXT_VARIATION_PASSWORD);
        container.addView(password);

        if (level != AUTH_LEVEL_NONE) {
            final ImageView secure = new ImageView(builder.getContext());
            secure.setImageResource(android.R.drawable.ic_lock_lock);
            container.addView(secure);
        }

        builder.setNegativeButton(android.R.string.cancel, /* listener */ null)
               .setPositiveButton(android.R.string.ok,
                                  new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(final DialogInterface dialog, final int which) {
                        if ((flags & AUTH_FLAG_ONLY_PASSWORD) == 0) {
                            callback.confirm(username.getText().toString(),
                                             password.getText().toString());
                        } else {
                            callback.confirm(password.getText().toString());
                        }
                    }
                });
        addCheckbox(builder, container, callback).show();
    }

    private void addChoiceItems(final int type, final ArrayAdapter<GeckoBundle> list,
                                final GeckoBundle[] items, final String indent) {
        if (type == CHOICE_TYPE_MENU) {
            list.addAll(items);
            return;
        }

        for (final GeckoBundle item : items) {
            final GeckoBundle[] children = item.getBundleArray("items");
            if (indent != null && children == null) {
                item.putString("label", indent + item.getString("label", ""));
            }
            list.add(item);

            if (children != null) {
                final String newIndent;
                if (type == CHOICE_TYPE_SINGLE || type == CHOICE_TYPE_MULTIPLE) {
                    newIndent = (indent != null) ? indent + '\t' : "\t";
                } else {
                    newIndent = null;
                }
                addChoiceItems(type, list, children, newIndent);
            }
        }
    }

    public void promptForChoice(final GeckoView view, final String title, final String msg,
                                final int type, final GeckoBundle[] choices,
                                final ChoiceCallback callback)
    {
        final Activity activity = getActivity(view);
        if (activity == null) {
            callback.dismiss();
            return;
        }
        final AlertDialog.Builder builder = new AlertDialog.Builder(activity);
        addStandardLayout(builder, title, msg, callback);

        final ListView list = new ListView(builder.getContext());
        if (type == CHOICE_TYPE_MULTIPLE) {
            list.setChoiceMode(ListView.CHOICE_MODE_MULTIPLE);
        }

        final ArrayAdapter<GeckoBundle> adapter = new ArrayAdapter<GeckoBundle>(
                builder.getContext(), android.R.layout.simple_list_item_1) {
            private static final int TYPE_MENU_ITEM = 0;
            private static final int TYPE_MENU_CHECK = 1;
            private static final int TYPE_SEPARATOR = 2;
            private static final int TYPE_GROUP = 3;
            private static final int TYPE_SINGLE = 4;
            private static final int TYPE_MULTIPLE = 5;
            private static final int TYPE_COUNT = 6;

            private LayoutInflater mInflater;
            private View mSeparator;

            @Override
            public int getViewTypeCount() {
                return TYPE_COUNT;
            }

            @Override
            public int getItemViewType(final int position) {
                final GeckoBundle item = getItem(position);
                if (item.getBoolean("separator")) {
                    return TYPE_SEPARATOR;
                } else if (type == CHOICE_TYPE_MENU) {
                    return item.getBoolean("selected") ? TYPE_MENU_CHECK : TYPE_MENU_ITEM;
                } else if (item.containsKey("items")) {
                    return TYPE_GROUP;
                } else if (type == CHOICE_TYPE_SINGLE) {
                    return TYPE_SINGLE;
                } else if (type == CHOICE_TYPE_MULTIPLE) {
                    return TYPE_MULTIPLE;
                } else {
                    throw new UnsupportedOperationException();
                }
            }

            @Override
            public boolean isEnabled(final int position) {
                final GeckoBundle item = getItem(position);
                return !item.getBoolean("separator") && !item.getBoolean("disabled") &&
                        ((type != CHOICE_TYPE_SINGLE && type != CHOICE_TYPE_MULTIPLE) ||
                         !item.containsKey("items"));
            }

            @Override
            public View getView(final int position, View view,
                                final ViewGroup parent) {
                final int itemType = getItemViewType(position);
                final int layoutId;
                if (itemType == TYPE_SEPARATOR) {
                    if (mSeparator == null) {
                        mSeparator = new View(getContext());
                        mSeparator.setLayoutParams(new ListView.LayoutParams(
                                ViewGroup.LayoutParams.MATCH_PARENT, 2, itemType));
                        final TypedArray attr = getContext().obtainStyledAttributes(
                                new int[] { android.R.attr.listDivider });
                        mSeparator.setBackgroundResource(attr.getResourceId(0, 0));
                        attr.recycle();
                    }
                    return mSeparator;
                } else if (itemType == TYPE_MENU_ITEM) {
                    layoutId = android.R.layout.simple_list_item_1;
                } else if (itemType == TYPE_MENU_CHECK) {
                    layoutId = android.R.layout.simple_list_item_checked;
                } else if (itemType == TYPE_GROUP) {
                    layoutId = android.R.layout.preference_category;
                } else if (itemType == TYPE_SINGLE) {
                    layoutId = android.R.layout.simple_list_item_single_choice;
                } else if (itemType == TYPE_MULTIPLE) {
                    layoutId = android.R.layout.simple_list_item_multiple_choice;
                } else {
                    throw new UnsupportedOperationException();
                }

                if (view == null) {
                    if (mInflater == null) {
                        mInflater = LayoutInflater.from(builder.getContext());
                    }
                    view = mInflater.inflate(layoutId, parent, false);
                }

                final GeckoBundle item = getItem(position);
                final TextView text = (TextView) view;
                text.setEnabled(!item.getBoolean("disabled"));
                text.setText(item.getString("label"));
                if (view instanceof CheckedTextView) {
                    final boolean selected = item.getBoolean("selected");
                    if (itemType == TYPE_MULTIPLE) {
                        list.setItemChecked(position, selected);
                    } else {
                        ((CheckedTextView) view).setChecked(selected);
                    }
                }
                return view;
            }
        };
        addChoiceItems(type, adapter, choices, /* indent */ null);

        list.setAdapter(adapter);
        builder.setView(list);

        final AlertDialog dialog;
        if (type == CHOICE_TYPE_SINGLE || type == CHOICE_TYPE_MENU) {
            dialog = builder.create();
            list.setOnItemClickListener(new AdapterView.OnItemClickListener() {
                @Override
                public void onItemClick(final AdapterView<?> parent, final View v,
                                        final int position, final long id) {
                    final GeckoBundle item = adapter.getItem(position);
                    if (type == CHOICE_TYPE_MENU) {
                        final GeckoBundle[] children = item.getBundleArray("items");
                        if (children != null) {
                            // Show sub-menu.
                            dialog.setOnDismissListener(null);
                            dialog.dismiss();
                            promptForChoice(view, item.getString("label"), /* msg */ null,
                                            type, children, callback);
                            return;
                        }
                    }
                    callback.confirm(item);
                    dialog.dismiss();
                }
            });
        } else if (type == CHOICE_TYPE_MULTIPLE) {
            list.setOnItemClickListener(new AdapterView.OnItemClickListener() {
                @Override
                public void onItemClick(final AdapterView<?> parent, final View v,
                                        final int position, final long id) {
                    final GeckoBundle item = adapter.getItem(position);
                    item.putBoolean("selected", ((CheckedTextView) v).isChecked());
                }
            });
            builder.setNegativeButton(android.R.string.cancel, /* listener */ null)
                   .setPositiveButton(android.R.string.ok,
                                      new DialogInterface.OnClickListener() {
                @Override
                public void onClick(final DialogInterface dialog,
                                    final int which) {
                    final int len = adapter.getCount();
                    ArrayList<String> items = new ArrayList<>(len);
                    for (int i = 0; i < len; i++) {
                        final GeckoBundle item = adapter.getItem(i);
                        if (item.getBoolean("selected")) {
                            items.add(item.getString("id"));
                        }
                    }
                    callback.confirm(items.toArray(new String[items.size()]));
                }
            });
            dialog = builder.create();
        } else {
            throw new UnsupportedOperationException();
        }
        dialog.show();
    }

    private static int parseColor(final String value, final int def) {
        try {
            return Color.parseColor(value);
        } catch (final IllegalArgumentException e) {
            return def;
        }
    }

    public void promptForColor(final GeckoView view, final String title,
                               final String value, final TextCallback callback)
    {
        final Activity activity = getActivity(view);
        if (activity == null) {
            callback.dismiss();
            return;
        }
        final AlertDialog.Builder builder = new AlertDialog.Builder(activity);
        addStandardLayout(builder, title, /* msg */ null, callback);

        final int initial = parseColor(value, /* def */ 0);
        final ArrayAdapter<Integer> adapter = new ArrayAdapter<Integer>(
                builder.getContext(), android.R.layout.simple_list_item_1) {
            private LayoutInflater mInflater;

            @Override
            public int getViewTypeCount() {
                return 2;
            }

            @Override
            public int getItemViewType(final int position) {
                return (getItem(position) == initial) ? 1 : 0;
            }

            @Override
            public View getView(final int position, View view,
                                final ViewGroup parent) {
                if (mInflater == null) {
                    mInflater = LayoutInflater.from(builder.getContext());
                }
                final int color = getItem(position);
                if (view == null) {
                    view = mInflater.inflate((color == initial) ?
                            android.R.layout.simple_list_item_checked :
                            android.R.layout.simple_list_item_1, parent, false);
                }
                view.setBackgroundResource(android.R.drawable.editbox_background);
                view.getBackground().setColorFilter(color, PorterDuff.Mode.MULTIPLY);
                return view;
            }
        };

        adapter.addAll(0xffff4444 /* holo_red_light */,
                       0xffcc0000 /* holo_red_dark */,
                       0xffffbb33 /* holo_orange_light */,
                       0xffff8800 /* holo_orange_dark */,
                       0xff99cc00 /* holo_green_light */,
                       0xff669900 /* holo_green_dark */,
                       0xff33b5e5 /* holo_blue_light */,
                       0xff0099cc /* holo_blue_dark */,
                       0xffaa66cc /* holo_purple */,
                       0xffffffff /* white */,
                       0xffaaaaaa /* lighter_gray */,
                       0xff555555 /* darker_gray */,
                       0xff000000 /* black */);

        final ListView list = new ListView(builder.getContext());
        list.setAdapter(adapter);
        builder.setView(list);

        final AlertDialog dialog = builder.create();
        list.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(final AdapterView<?> parent, final View v,
                                    final int position, final long id) {
                callback.confirm(String.format("#%06x", 0xffffff & adapter.getItem(position)));
                dialog.dismiss();
            }
        });
        dialog.show();
    }

    private static Date parseDate(final SimpleDateFormat formatter,
                                  final String value,
                                  final boolean defaultToNow) {
        try {
            if (value != null && !value.isEmpty()) {
                return formatter.parse(value);
            }
        } catch (final ParseException e) {
        }
        return defaultToNow ? new Date() : null;
    }

    @SuppressWarnings("deprecation")
    private static void setTimePickerTime(final TimePicker picker, final Calendar cal) {
        if (Build.VERSION.SDK_INT >= 23) {
            picker.setHour(cal.get(Calendar.HOUR_OF_DAY));
            picker.setMinute(cal.get(Calendar.MINUTE));
        } else {
            picker.setCurrentHour(cal.get(Calendar.HOUR_OF_DAY));
            picker.setCurrentMinute(cal.get(Calendar.MINUTE));
        }
    }

    @SuppressWarnings("deprecation")
    private static void setCalendarTime(final Calendar cal, final TimePicker picker) {
        if (Build.VERSION.SDK_INT >= 23) {
            cal.set(Calendar.HOUR_OF_DAY, picker.getHour());
            cal.set(Calendar.MINUTE, picker.getMinute());
        } else {
            cal.set(Calendar.HOUR_OF_DAY, picker.getCurrentHour());
            cal.set(Calendar.MINUTE, picker.getCurrentMinute());
        }
    }

    public void promptForDateTime(final GeckoView view, final String title, final int type,
                                  final String value, final String min, final String max,
                                  final TextCallback callback)
    {
        final Activity activity = getActivity(view);
        if (activity == null) {
            callback.dismiss();
            return;
        }
        final String format;
        if (type == DATETIME_TYPE_DATE) {
            format = "yyyy-MM-dd";
        } else if (type == DATETIME_TYPE_MONTH) {
            format = "yyyy-MM";
        } else if (type == DATETIME_TYPE_WEEK) {
            format = "yyyy-'W'ww";
        } else if (type == DATETIME_TYPE_TIME) {
            format = "HH:mm";
        } else if (type == DATETIME_TYPE_DATETIME_LOCAL) {
            format = "yyyy-MM-dd'T'HH:mm";
        } else {
            throw new UnsupportedOperationException();
        }

        final SimpleDateFormat formatter = new SimpleDateFormat(format, Locale.ROOT);
        final Date minDate = parseDate(formatter, min, /* defaultToNow */ false);
        final Date maxDate = parseDate(formatter, max, /* defaultToNow */ false);
        final Date date = parseDate(formatter, value, /* defaultToNow */ true);
        final Calendar cal = formatter.getCalendar();
        cal.setTime(date);

        final AlertDialog.Builder builder = new AlertDialog.Builder(activity);
        final LayoutInflater inflater = LayoutInflater.from(builder.getContext());
        final DatePicker datePicker;
        if (type == DATETIME_TYPE_DATE || type == DATETIME_TYPE_MONTH ||
            type == DATETIME_TYPE_WEEK || type == DATETIME_TYPE_DATETIME_LOCAL) {
            final int resId = builder.getContext().getResources().getIdentifier(
                    "date_picker_dialog", "layout", "android");
            DatePicker picker = null;
            if (resId != 0) {
                try {
                    picker = (DatePicker) inflater.inflate(resId, /* root */ null);
                } catch (final ClassCastException|InflateException e) {
                }
            }
            if (picker == null) {
                picker = new DatePicker(builder.getContext());
            }
            picker.init(cal.get(Calendar.YEAR), cal.get(Calendar.MONTH),
                        cal.get(Calendar.DAY_OF_MONTH), /* listener */ null);
            if (minDate != null) {
                picker.setMinDate(minDate.getTime());
            }
            if (maxDate != null) {
                picker.setMaxDate(maxDate.getTime());
            }
            datePicker = picker;
        } else {
            datePicker = null;
        }

        final TimePicker timePicker;
        if (type == DATETIME_TYPE_TIME || type == DATETIME_TYPE_DATETIME_LOCAL) {
            final int resId = builder.getContext().getResources().getIdentifier(
                    "time_picker_dialog", "layout", "android");
            TimePicker picker = null;
            if (resId != 0) {
                try {
                    picker = (TimePicker) inflater.inflate(resId, /* root */ null);
                } catch (final ClassCastException|InflateException e) {
                }
            }
            if (picker == null) {
                picker = new TimePicker(builder.getContext());
            }
            setTimePickerTime(picker, cal);
            picker.setIs24HourView(DateFormat.is24HourFormat(builder.getContext()));
            timePicker = picker;
        } else {
            timePicker = null;
        }

        final LinearLayout container = addStandardLayout(builder, title,
                                                         /* msg */ null, callback);
        container.setPadding(/* left */ 0, /* top */ 0, /* right */ 0, /* bottom */ 0);
        if (datePicker != null) {
            container.addView(datePicker);
        }
        if (timePicker != null) {
            container.addView(timePicker);
        }

        final DialogInterface.OnClickListener listener =
                new DialogInterface.OnClickListener() {
            @Override
            public void onClick(final DialogInterface dialog, final int which) {
                if (which == DialogInterface.BUTTON_NEUTRAL) {
                    // Clear
                    callback.confirm("");
                    return;
                }
                if (datePicker != null) {
                    cal.set(datePicker.getYear(), datePicker.getMonth(),
                            datePicker.getDayOfMonth());
                }
                if (timePicker != null) {
                    setCalendarTime(cal, timePicker);
                }
                callback.confirm(formatter.format(cal.getTime()));
            }
        };
        builder.setNegativeButton(android.R.string.cancel, /* listener */ null)
               .setNeutralButton(R.string.clear_field, listener)
               .setPositiveButton(android.R.string.ok, listener)
               .show();
    }

    public void promptForFile(GeckoView view, String title, int type,
                              String[] mimeTypes, FileCallback callback)
    {
        final Activity activity = getActivity(view);
        if (activity == null) {
            callback.dismiss();
            return;
        }

        // Merge all given MIME types into one, using wildcard if needed.
        String mimeType = null;
        String mimeSubtype = null;
        for (final String rawType : mimeTypes) {
            final String normalizedType = rawType.trim().toLowerCase(Locale.ROOT);
            final int len = normalizedType.length();
            int slash = normalizedType.indexOf('/');
            if (slash < 0) {
                slash = len;
            }
            final String newType = normalizedType.substring(0, slash);
            final String newSubtype = normalizedType.substring(Math.min(slash + 1, len));
            if (mimeType == null) {
                mimeType = newType;
            } else if (!mimeType.equals(newType)) {
                mimeType = "*";
            }
            if (mimeSubtype == null) {
                mimeSubtype = newSubtype;
            } else if (!mimeSubtype.equals(newSubtype)) {
                mimeSubtype = "*";
            }
        }

        final Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.setType((mimeType != null ? mimeType : "*") + '/' +
                       (mimeSubtype != null ? mimeSubtype : "*"));
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.putExtra(Intent.EXTRA_LOCAL_ONLY, true);
        if (type == FILE_TYPE_MULTIPLE) {
            intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        }
        if (mimeTypes.length > 0) {
            intent.putExtra(Intent.EXTRA_MIME_TYPES, mimeTypes);
        }

        try {
            mFileType = type;
            mFileCallback = callback;
            activity.startActivityForResult(intent, filePickerRequestCode);
        } catch (final ActivityNotFoundException e) {
            Log.e(LOGTAG, "Cannot launch activity", e);
            callback.dismiss();
        }
    }

    public void onFileCallbackResult(final int resultCode, final Intent data) {
        if (mFileCallback == null) {
            return;
        }

        final FileCallback callback = mFileCallback;
        mFileCallback = null;

        if (resultCode != Activity.RESULT_OK || data == null) {
            callback.dismiss();
            return;
        }

        final Uri uri = data.getData();
        final ClipData clip = data.getClipData();

        if (mFileType == FILE_TYPE_SINGLE ||
            (mFileType == FILE_TYPE_MULTIPLE && clip == null)) {
            callback.confirm(uri);

        } else if (mFileType == FILE_TYPE_MULTIPLE) {
            if (clip == null) {
                Log.w(LOGTAG, "No selected file");
                callback.dismiss();
                return;
            }
            final int count = clip.getItemCount();
            final ArrayList<Uri> uris = new ArrayList<>(count);
            for (int i = 0; i < count; i++) {
                uris.add(clip.getItemAt(i).getUri());
            }
            callback.confirm(uris.toArray(new Uri[uris.size()]));
        }
    }

    public void promptForPermission(final GeckoView view, final String title,
                                    final GeckoView.PermissionDelegate.Callback callback) {
        final Activity activity = getActivity(view);
        if (activity == null) {
            callback.reject();
            return;
        }
        final AlertDialog.Builder builder = new AlertDialog.Builder(activity);
        builder.setTitle(title)
               .setOnDismissListener(new DialogInterface.OnDismissListener() {
                   @Override
                   public void onDismiss(final DialogInterface dialog) {
                       callback.reject();
                   }
               })
               .setNegativeButton(android.R.string.cancel, /* onClickListener */ null)
               .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                   @Override
                   public void onClick(final DialogInterface dialog, final int which) {
                       callback.grant();
                   }
               })
               .show();
    }

    private Spinner addMediaSpinner(final Context context, final ViewGroup container,
                                    final GeckoBundle[] sources) {
        final ArrayAdapter<GeckoBundle> adapter = new ArrayAdapter<GeckoBundle>(
                context, android.R.layout.simple_spinner_item) {
            private View convertView(final int position, final View view) {
                if (view != null) {
                    final GeckoBundle item = getItem(position);
                    ((TextView) view).setText(item.getString("name"));
                }
                return view;
            }

            @Override
            public View getView(final int position, View view,
                                final ViewGroup parent) {
                return convertView(position, super.getView(position, view, parent));
            }

            @Override
            public View getDropDownView(final int position, final View view,
                                        final ViewGroup parent) {
                return convertView(position, super.getDropDownView(position, view, parent));
            }
        };
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        adapter.addAll(sources);

        final Spinner spinner = new Spinner(context);
        spinner.setAdapter(adapter);
        spinner.setSelection(0);
        container.addView(spinner);
        return spinner;
    }

    public void promptForMedia(final GeckoView view, final String title,
                               final GeckoBundle[] video, final GeckoBundle[] audio,
                               final GeckoView.PermissionDelegate.MediaCallback callback) {
        final Activity activity = getActivity(view);
        if (activity == null || (video == null && audio == null)) {
            callback.reject();
            return;
        }
        final AlertDialog.Builder builder = new AlertDialog.Builder(activity);
        final LinearLayout container = addStandardLayout(builder, title, /* msg */ null,
                                                         /* callback */ null);

        final Spinner videoSpinner;
        if (video != null) {
            videoSpinner = addMediaSpinner(builder.getContext(), container, video);
        } else {
            videoSpinner = null;
        }

        final Spinner audioSpinner;
        if (audio != null) {
            audioSpinner = addMediaSpinner(builder.getContext(), container, audio);
        } else {
            audioSpinner = null;
        }

        builder.setOnDismissListener(new DialogInterface.OnDismissListener() {
                    @Override
                    public void onDismiss(final DialogInterface dialog) {
                        callback.reject();
                    }
                })
               .setNegativeButton(android.R.string.cancel, /* listener */ null)
               .setPositiveButton(android.R.string.ok,
                                  new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(final DialogInterface dialog, final int which) {
                        final GeckoBundle video = (videoSpinner != null)
                                ? (GeckoBundle) videoSpinner.getSelectedItem() : null;
                        final GeckoBundle audio = (audioSpinner != null)
                                ? (GeckoBundle) audioSpinner.getSelectedItem() : null;
                        callback.grant(video, audio);
                    }
                })
               .show();
    }
}
