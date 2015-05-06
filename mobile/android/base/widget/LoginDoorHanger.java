/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.text.method.PasswordTransformationMethod;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;
import ch.boye.httpclientandroidlib.util.TextUtils;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.R;
import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.favicons.OnFaviconLoadedListener;

public class LoginDoorHanger extends DoorHanger {
    private static final String LOGTAG = "LoginDoorHanger";
    private enum ActionType { EDIT };

    private final TextView mTitle;
    private final TextView mLogin;
    private int mCallbackID;

    public LoginDoorHanger(Context context, DoorhangerConfig config) {
        super(context, config, Type.LOGIN);

        mTitle = (TextView) findViewById(R.id.doorhanger_title);
        mLogin = (TextView) findViewById(R.id.doorhanger_login);

        loadConfig(config);
    }

    @Override
    protected void loadConfig(DoorhangerConfig config) {
        setOptions(config.getOptions());
        setMessage(config.getMessage());
        setButtons(config);
    }

    @Override
    protected void setOptions(final JSONObject options) {
        super.setOptions(options);

        final JSONObject titleObj = options.optJSONObject("title");
        if (titleObj != null) {

            try {
                final String text = titleObj.getString("text");
                mTitle.setText(text);
            } catch (JSONException e) {
                Log.e(LOGTAG, "Error loading title from options JSON", e);
            }

            final String resource = titleObj.optString("resource");
            if (resource != null) {
                Favicons.getSizedFaviconForPageFromLocal(mContext, resource, 32, new OnFaviconLoadedListener() {
                    @Override
                    public void onFaviconLoaded(String url, String faviconURL, Bitmap favicon) {
                        if (favicon != null) {
                            mTitle.setCompoundDrawablesWithIntrinsicBounds(new BitmapDrawable(mResources, favicon), null, null, null);
                            mTitle.setCompoundDrawablePadding((int) mResources.getDimension(R.dimen.doorhanger_drawable_padding));
                        }
                    }
                });
            }
        }

        final JSONObject actionText = options.optJSONObject("actionText");
        addActionText(actionText);
    }

    @Override
    protected Button createButtonInstance(final String text, final int id) {
        // HACK: Confirm button will the the rightmost/last button added. Bug 1147064 should add differentiation of the two.
        mCallbackID = id;

        final Button button = (Button) LayoutInflater.from(getContext()).inflate(R.layout.doorhanger_button, null);
        button.setText(text);

        button.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View v) {
                final JSONObject response = new JSONObject();
                try {
                    response.put("callback", id);
                } catch (JSONException e) {
                    Log.e(LOGTAG, "Error making doorhanger response message", e);
                }
                mOnButtonClickListener.onButtonClick(response, LoginDoorHanger.this);
            }
        });

        return button;
    }

    /**
     * Add sub-text to the doorhanger and add the click action.
     *
     * If the parsing the action from the JSON throws, the text is left visible, but there is no
     * click action.
     * @param actionTextObj JSONObject containing blob for making an action.
     */
    private void addActionText(JSONObject actionTextObj) {
        if (actionTextObj == null) {
            mLogin.setVisibility(View.GONE);
            return;
        }

        boolean hasUsername = true;
        String text = actionTextObj.optString("text");
        if (TextUtils.isEmpty(text)) {
            hasUsername = false;
            text = mResources.getString(R.string.doorhanger_login_no_username);
        }
        mLogin.setText(text);
        mLogin.setVisibility(View.VISIBLE);

        // Make action.
        try {
            final JSONObject bundle = actionTextObj.getJSONObject("bundle");
            final ActionType type = ActionType.valueOf(actionTextObj.getString("type"));
            final AlertDialog.Builder builder = new AlertDialog.Builder(mContext);

            switch (type) {
                case EDIT:
                    builder.setTitle(mResources.getString(R.string.doorhanger_login_edit_title));

                    final View view = LayoutInflater.from(mContext).inflate(R.layout.login_edit_dialog, null);
                    final EditText username = (EditText) view.findViewById(R.id.username_edit);
                    username.setText(bundle.getString("username"));
                    final EditText password = (EditText) view.findViewById(R.id.password_edit);
                    password.setText(bundle.getString("password"));
                    final CheckBox passwordCheckbox = (CheckBox) view.findViewById(R.id.checkbox_toggle_password);
                    passwordCheckbox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                        @Override
                        public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                            if (isChecked) {
                                password.setTransformationMethod(null);
                            } else {
                                password.setTransformationMethod(PasswordTransformationMethod.getInstance());
                            }
                        }
                    });
                    builder.setView(view);

                    builder.setPositiveButton(R.string.button_remember, new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialog, int which) {
                            JSONObject response = new JSONObject();
                            try {
                                response.put("callback", mCallbackID);
                                final JSONObject inputs = new JSONObject();
                                inputs.put("username", username.getText());
                                inputs.put("password", password.getText());
                                response.put("inputs", inputs);
                            } catch (JSONException e) {
                                Log.e(LOGTAG, "Error creating doorhanger reply message");
                                response = null;
                                Toast.makeText(mContext, mResources.getString(R.string.doorhanger_login_edit_toast_error), Toast.LENGTH_SHORT).show();
                            }
                            mOnButtonClickListener.onButtonClick(response, LoginDoorHanger.this);
                            dialog.dismiss();
                        }
                    });
                    builder.setNegativeButton(R.string.button_cancel, new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialog, int which) {
                            dialog.dismiss();
                        }
                    });
            }

            final Dialog dialog = builder.create();
            mLogin.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    dialog.show();
                }
            });

        } catch (JSONException e) {
            // Log an error, but leave the text visible if there was a username.
            Log.e(LOGTAG, "Error fetching actionText from JSON", e);
            if (!hasUsername) {
                mLogin.setVisibility(View.GONE);
            }
        }
    }
}
