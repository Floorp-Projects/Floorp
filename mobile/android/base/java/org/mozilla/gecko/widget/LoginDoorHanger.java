/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.text.Html;
import android.text.Spanned;
import android.text.TextUtils;
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

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.util.GeckoBundle;

import java.util.Locale;

public class LoginDoorHanger extends DoorHanger {
    private static final String LOGTAG = "LoginDoorHanger";
    private enum ActionType { EDIT, SELECT }

    private final TextView mMessage;
    private final DoorhangerConfig.ButtonConfig mButtonConfig;

    public LoginDoorHanger(Context context, DoorhangerConfig config) {
        super(context, config, Type.LOGIN);

        mMessage = (TextView) findViewById(R.id.doorhanger_message);
        mIcon.setImageResource(R.drawable.icon_key);
        mIcon.setVisibility(View.VISIBLE);

        mButtonConfig = config.getPositiveButtonConfig();

        loadConfig(config);
    }

    private void setMessage(String message) {
        Spanned markupMessage = Html.fromHtml(message);
        mMessage.setText(markupMessage);
    }

    @Override
    protected void loadConfig(DoorhangerConfig config) {
        setOptions(config.getOptions());
        setMessage(config.getMessage());
        // Store the positive callback id for nested dialogs that need the same callback id.
        addButtonsToLayout(config);
    }

    @Override
    protected int getContentResource() {
        return R.layout.login_doorhanger;
    }

    @Override
    protected void setOptions(final GeckoBundle options) {
        super.setOptions(options);

        final GeckoBundle actionText = options.getBundle("actionText");
        addActionText(actionText);
    }

    @Override
    protected OnClickListener makeOnButtonClickListener(final int id, final String telemetryExtra) {
        return new Button.OnClickListener() {
            @Override
            public void onClick(View v) {
                final String expandedExtra = mType.toString().toLowerCase(Locale.US) + "-" + telemetryExtra;
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.DOORHANGER, expandedExtra);

                final JSONObject response = new JSONObject();
                try {
                    response.put("callback", id);
                } catch (JSONException e) {
                    Log.e(LOGTAG, "Error making doorhanger response message", e);
                }
                mOnButtonClickListener.onButtonClick(response, LoginDoorHanger.this);
            }
        };
    }

    /**
     * Add sub-text to the doorhanger and add the click action.
     *
     * If the parsing the action from the JSON throws, the text is left visible, but there is no
     * click action.
     * @param actionTextObj JSONObject containing blob for making an action.
     */
    private void addActionText(final GeckoBundle actionTextObj) {
        if (actionTextObj == null) {
            mLink.setVisibility(View.GONE);
            return;
        }

        // Make action.
        final GeckoBundle bundle = actionTextObj.getBundle("bundle");
        final ActionType type = ActionType.valueOf(actionTextObj.getString("type"));
        final AlertDialog.Builder builder = new AlertDialog.Builder(mContext);

        switch (type) {
            case EDIT:
                builder.setTitle(mResources.getString(R.string.doorhanger_login_edit_title));

                final View view = LayoutInflater.from(mContext).inflate(
                        R.layout.login_edit_dialog, null);
                final EditText username = (EditText) view.findViewById(R.id.username_edit);
                username.setText(bundle.getString("username"));
                final EditText password = (EditText) view.findViewById(R.id.password_edit);
                password.setText(bundle.getString("password"));
                final CheckBox passwordCheckbox = (CheckBox) view.findViewById(
                        R.id.checkbox_toggle_password);

                passwordCheckbox.setOnCheckedChangeListener(
                        new CompoundButton.OnCheckedChangeListener() {
                    @Override
                    public void onCheckedChanged(final CompoundButton buttonView,
                                                 final boolean isChecked) {
                        if (isChecked) {
                            password.setTransformationMethod(null);
                        } else {
                            password.setTransformationMethod(
                                    PasswordTransformationMethod.getInstance());
                        }
                    }
                });
                builder.setView(view);

                builder.setPositiveButton(
                        mButtonConfig.label, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(final DialogInterface dialog, final int which) {
                        JSONObject response = new JSONObject();
                        try {
                            response.put("callback", mButtonConfig.callback);
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
                    }
                });
                builder.setNegativeButton(
                        R.string.button_cancel, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(final DialogInterface dialog, final int which) {
                        dialog.dismiss();
                    }
                });

                String text = actionTextObj.getString("text");
                if (TextUtils.isEmpty(text)) {
                    text = mResources.getString(R.string.doorhanger_login_no_username);
                }
                mLink.setText(text);
                mLink.setVisibility(View.VISIBLE);
                break;

            case SELECT:
                builder.setTitle(mResources.getString(R.string.doorhanger_login_select_title));
                final GeckoBundle[] logins = bundle.getBundleArray("logins");
                final int numLogins = logins.length;
                final CharSequence[] usernames = new CharSequence[numLogins];
                final String[] passwords = new String[numLogins];
                final String noUser = mResources.getString(R.string.doorhanger_login_no_username);

                for (int i = 0; i < numLogins; i++) {
                    final GeckoBundle login = logins[i];
                    String user = login.getString("username");
                    if (TextUtils.isEmpty(user)) {
                        user = noUser;
                    }
                    usernames[i] = user;
                    passwords[i] = login.getString("password");
                }

                builder.setItems(usernames, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        final JSONObject response = new JSONObject();
                        try {
                            response.put("callback", mButtonConfig.callback);
                            response.put("password", passwords[which]);
                        } catch (JSONException e) {
                            Log.e(LOGTAG, "Error making login select dialog JSON", e);
                        }
                        mOnButtonClickListener.onButtonClick(response, LoginDoorHanger.this);
                    }
                });
                builder.setNegativeButton(
                        R.string.button_cancel, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        dialog.dismiss();
                    }
                });

                mLink.setText(R.string.doorhanger_login_select_action_text);
                mLink.setVisibility(View.VISIBLE);
                break;
        }

        final Dialog dialog = builder.create();
        mLink.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                dialog.show();
            }
        });
    }
}
