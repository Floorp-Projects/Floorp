/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.ActivityUtils;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.Context;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.LinearLayout;
import android.widget.TextView;

public class FindInPageBar extends LinearLayout
        implements TextWatcher, View.OnClickListener, BundleEventListener {
    private static final String LOGTAG = "GeckoFindInPageBar";
    private static final String REQUEST_ID = "FindInPageBar";

    /* package */ CustomEditText mFindText;
    private TextView mStatusText;
    private boolean mInflated;
    private final GeckoApp geckoApp;

    public FindInPageBar(Context context, AttributeSet attrs) {
        super(context, attrs);
        setFocusable(true);

        geckoApp = (GeckoApp) ActivityUtils.getActivityFromContext(context);
    }

    public void inflateContent() {
        LayoutInflater inflater = LayoutInflater.from(getContext());
        View content = inflater.inflate(R.layout.find_in_page_content, this);

        content.findViewById(R.id.find_prev).setOnClickListener(this);
        content.findViewById(R.id.find_next).setOnClickListener(this);
        content.findViewById(R.id.find_close).setOnClickListener(this);

        // Capture clicks on the rest of the view to prevent them from
        // leaking into other views positioned below.
        content.setOnClickListener(this);

        mFindText = (CustomEditText) content.findViewById(R.id.find_text);
        mFindText.addTextChangedListener(this);
        mFindText.setOnKeyPreImeListener(new CustomEditText.OnKeyPreImeListener() {
            @Override
            public boolean onKeyPreIme(View v, int keyCode, KeyEvent event) {
                if (keyCode == KeyEvent.KEYCODE_BACK) {
                    hide();
                    return true;
                }
                return false;
            }
        });

        mStatusText = (TextView) content.findViewById(R.id.find_status);

        mInflated = true;
        EventDispatcher.getInstance().registerUiThreadListener(this,
            "FindInPage:MatchesCountResult");
    }

    public void show() {
        if (!mInflated)
            inflateContent();

        setVisibility(VISIBLE);
        mFindText.requestFocus();

        // handleMessage() receives response message and determines initial state of softInput

        geckoApp.getAppEventDispatcher().dispatch("TextSelection:Get", null, new EventCallback() {
            @Override
            public void sendSuccess(final Object result) {
                onTextSelectionData((String) result);
            }

            @Override
            public void sendError(final Object error) {
                Log.e(LOGTAG, "TextSelection:Get failed: " + error);
            }
        });

        EventDispatcher.getInstance().dispatch("FindInPage:Opened", null);
    }

    public void hide() {
        if (!mInflated || getVisibility() == View.GONE) {
            // There's nothing to hide yet.
            return;
        }

        // Always clear the Find string, primarily for privacy.
        mFindText.setText("");

        // Only close the IMM if its EditText is the one with focus.
        if (mFindText.isFocused()) {
          getInputMethodManager(mFindText).hideSoftInputFromWindow(mFindText.getWindowToken(), 0);
        }

        // Close the FIPB / FindHelper state.
        setVisibility(GONE);
        EventDispatcher.getInstance().dispatch("FindInPage:Closed", null);
    }

    private InputMethodManager getInputMethodManager(View view) {
        Context context = view.getContext();
        return (InputMethodManager) context.getSystemService(Context.INPUT_METHOD_SERVICE);
     }

    public void onDestroy() {
        if (!mInflated) {
            return;
        }
        EventDispatcher.getInstance().unregisterUiThreadListener(this,
            "FindInPage:MatchesCountResult");
    }

    private void onMatchesCountResult(final int total, final int current, final int limit, final String searchString) {
        if (total == -1) {
            updateResult(Integer.toString(limit) + "+");
        } else if (total > 0) {
            updateResult(Integer.toString(current) + "/" + Integer.toString(total));
        } else if (TextUtils.isEmpty(searchString)) {
            updateResult("");
        } else {
            // We display 0/0, when there were no
            // matches found, or if matching has been turned off by setting
            // pref accessibility.typeaheadfind.matchesCountLimit to 0.
            updateResult("0/0");
        }
    }

    private void updateResult(final String statusText) {
        mStatusText.setVisibility(statusText.isEmpty() ? View.GONE : View.VISIBLE);
        mStatusText.setText(statusText);
    }

    // TextWatcher implementation

    @Override
    public void afterTextChanged(Editable s) {
        sendRequestToFinderHelper("FindInPage:Find", s.toString());
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
        // ignore
    }

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {
        // ignore
    }

    // View.OnClickListener implementation

    @Override
    public void onClick(View v) {
        final int viewId = v.getId();

        String extras = getResources().getResourceEntryName(viewId);
        Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, extras);

        if (viewId == R.id.find_prev) {
            sendRequestToFinderHelper("FindInPage:Prev", mFindText.getText().toString());
            getInputMethodManager(mFindText).hideSoftInputFromWindow(mFindText.getWindowToken(), 0);
            return;
        }

        if (viewId == R.id.find_next) {
            sendRequestToFinderHelper("FindInPage:Next", mFindText.getText().toString());
            getInputMethodManager(mFindText).hideSoftInputFromWindow(mFindText.getWindowToken(), 0);
            return;
        }

        if (viewId == R.id.find_close) {
            hide();
        }
    }

    @Override // BundleEventListener
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if ("FindInPage:MatchesCountResult".equals(event)) {
            onMatchesCountResult(message.getInt("total", 0),
                message.getInt("current", 0),
                message.getInt("limit", 0),
                message.getString("searchString", ""));
            return;
        }
    }

    /* package */ void onTextSelectionData(final String text) {
        // Populate an initial find string, virtual keyboard not required.
        if (!TextUtils.isEmpty(text)) {
            // Populate initial selection
            mFindText.setText(text);
            return;
        }

        // Show the virtual keyboard.
        if (mFindText.hasWindowFocus()) {
            getInputMethodManager(mFindText).showSoftInput(mFindText, 0);
        } else {
            // showSoftInput won't work until after the window is focused.
            mFindText.setOnWindowFocusChangeListener(new CustomEditText.OnWindowFocusChangeListener() {
                @Override
                public void onWindowFocusChanged(boolean hasFocus) {
                    if (!hasFocus)
                        return;

                    mFindText.setOnWindowFocusChangeListener(null);
                    getInputMethodManager(mFindText).showSoftInput(mFindText, 0);
               }
            });
        }
    }

    /**
     * Request find operation, and update matchCount results (current count and total).
     */
    private void sendRequestToFinderHelper(final String request, final String searchString) {
        final GeckoBundle data = new GeckoBundle(1);
        data.putString("searchString", searchString);
        EventDispatcher.getInstance().dispatch(request, data);
    }
}
