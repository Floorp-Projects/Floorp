/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search.autocomplete;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.search.providers.SearchEngine;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

public class SearchBar extends FrameLayout {

    private final EditText editText;
    private final ImageButton clearButton;
    private final ImageView engineIcon;

    private final Drawable focusedBackground;
    private final Drawable defaultBackgound;

    private final InputMethodManager inputMethodManager;

    private TextListener listener;

    private boolean active;

    public interface TextListener {
        public void onChange(String text);
        public void onSubmit(String text);
        public void onFocusChange(boolean hasFocus);
    }

    public SearchBar(Context context, AttributeSet attrs) {
        super(context, attrs);

        LayoutInflater.from(context).inflate(R.layout.search_bar, this);

        editText = (EditText) findViewById(R.id.edit_text);
        editText.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {
            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
            }

            @Override
            public void afterTextChanged(Editable s) {
                if (listener != null) {
                    listener.onChange(s.toString());
                }

                updateClearButtonVisibility();
            }
        });

        // Attach a listener for the "search" key on the keyboard.
        editText.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (listener != null && actionId == EditorInfo.IME_ACTION_SEARCH) {
                    // The user searched without using search engine suggestions.
                    Telemetry.sendUIEvent(TelemetryContract.Event.SEARCH, TelemetryContract.Method.ACTIONBAR, "text");
                    listener.onSubmit(v.getText().toString());
                    return true;
                }
                return false;
            }
        });

        editText.setOnFocusChangeListener(new View.OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                if (listener != null) {
                    listener.onFocusChange(hasFocus);
                }
            }
        });

        clearButton = (ImageButton) findViewById(R.id.clear_button);
        clearButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                editText.setText("");
            }
        });
        engineIcon = (ImageView) findViewById(R.id.engine_icon);

        focusedBackground = getResources().getDrawable(R.drawable.edit_text_focused);
        defaultBackgound = getResources().getDrawable(R.drawable.edit_text_default);

        inputMethodManager = (InputMethodManager) context.getSystemService(Context.INPUT_METHOD_SERVICE);
    }

    public void setText(String text) {
        editText.setText(text);

        // Move cursor to end of search input.
        editText.setSelection(text.length());
    }

    public String getText() {
        return editText.getText().toString();
    }

    public void setEngine(SearchEngine engine) {
        final String iconURL = engine.getIconURL();
        final Bitmap bitmap = BitmapUtils.getBitmapFromDataURI(iconURL);
        final BitmapDrawable d = new BitmapDrawable(getResources(), bitmap);
        engineIcon.setImageDrawable(d);
        engineIcon.setContentDescription(engine.getName());

        // Update the focused background color.
        int color = BitmapUtils.getDominantColor(bitmap);

        // BitmapUtils#getDominantColor ignores black and white pixels, but it will
        // return white if no dominant color was found. We don't want to create a
        // white underline for the search bar, so we default to black instead.
        if (color == Color.WHITE) {
            color = Color.BLACK;
        }
        focusedBackground.setColorFilter(new PorterDuffColorFilter(color, PorterDuff.Mode.MULTIPLY));

        editText.setHint(getResources().getString(R.string.search_bar_hint, engine.getName()));
    }

    @SuppressWarnings("deprecation")
    public void setActive(boolean active) {
        if (this.active == active) {
            return;
        }
        this.active = active;

        updateClearButtonVisibility();

        editText.setFocusable(active);
        editText.setFocusableInTouchMode(active);

        final int leftDrawable = active ? R.drawable.search_icon_active : R.drawable.search_icon_inactive;
        editText.setCompoundDrawablesWithIntrinsicBounds(leftDrawable, 0, 0, 0);

        // We can't use a selector drawable because we apply a color filter to the focused
        // background at run time.
        // TODO: setBackgroundDrawable is deprecated in API level 16
        editText.setBackgroundDrawable(active ? focusedBackground : defaultBackgound);

        if (active) {
            editText.requestFocus();
            inputMethodManager.showSoftInput(editText, InputMethodManager.SHOW_IMPLICIT);
        } else {
            editText.clearFocus();
            inputMethodManager.hideSoftInputFromWindow(editText.getWindowToken(), 0);
        }
    }

    private void updateClearButtonVisibility() {
        // Only show the clear button when there is text in the input.
        final boolean visible = active && (editText.getText().length() > 0);
        clearButton.setVisibility(visible ? View.VISIBLE : View.GONE);
        engineIcon.setVisibility(visible ? View.GONE : View.VISIBLE);
    }

    public void setTextListener(TextListener listener) {
        this.listener = listener;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent e) {
        // When the view is active, pass touch events to child views.
        // Otherwise, intercept touch events to allow click listeners on the view to
        // fire no matter where the user clicks.
        return !active;
    }
}
