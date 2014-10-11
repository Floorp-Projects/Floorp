/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.overlays.ui;

import android.util.AttributeSet;
import org.mozilla.gecko.R;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * A button in the share overlay, such as the "Add to Reading List" button.
 * Has an associated icon and label, and two states: enabled and disabled.
 *
 * When disabled, tapping results in a "pop" animation causing the icon to pulse. When enabled,
 * tapping calls the OnClickListener set by the consumer in the usual way.
 */
public class OverlayDialogButton extends LinearLayout {
    private static final String LOGTAG = "GeckoOverlayDialogButton";

    // The views making up this button.
    private ImageView icon;
    private TextView label;

    // Label/icon used when enabled.
    private String enabledLabel;
    private Drawable enabledIcon;

    // Label/icon used when disabled.
    private String disabledLabel;
    private Drawable disabledIcon;

    // Click listeners used when enabled/disabled. Currently, disabledOnClickListener is set
    // internally to something that causes the icon to pulse.
    private OnClickListener enabledOnClickListener;
    private OnClickListener disabledOnClickListener;

    private boolean isEnabled = true;

    public OverlayDialogButton(Context context) {
        super(context);
        init(context);
    }

    public OverlayDialogButton(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context);
    }

    public OverlayDialogButton(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        init(context);
    }

    private void init(Context context) {
        setOrientation(HORIZONTAL);
        setPadding(0, 0, 0, 0);
        setBackgroundResource(R.drawable.overlay_share_button_background);

        LayoutInflater.from(context).inflate(R.layout.overlay_share_button, this);
        icon = (ImageView) findViewById(R.id.overlaybtn_icon);
        label = (TextView) findViewById(R.id.overlaybtn_label);
    }

    public void setEnabledLabelAndIcon(String s, Drawable d) {
        enabledLabel = s;
        enabledIcon = d;

        if (isEnabled) {
            updateViews();
        }
    }

    public void setDisabledLabelAndIcon(String s, Drawable d) {
        disabledLabel = s;
        disabledIcon = d;

        if (!isEnabled) {
            updateViews();
        }
    }

    /**
     * Assign the appropriate label and icon to the views, and update the onClickListener for this
     * view to the correct one (based on current enabledness state).
     */
    private void updateViews() {
        label.setEnabled(isEnabled);
        if (isEnabled) {
            label.setText(enabledLabel);
            icon.setImageDrawable(enabledIcon);
            super.setOnClickListener(enabledOnClickListener);
        } else {
            label.setText(disabledLabel);
            icon.setImageDrawable(disabledIcon);
            super.setOnClickListener(getPopListener());
        }
    }

    /**
     * Helper method to lazily-initialise disabledOnClickListener to a listener that performs the
     * "pop" animation on the icon.
     * updateViews handles making this the actual onClickListener for this view.
     */
    private OnClickListener getPopListener() {
        if (disabledOnClickListener == null) {
            disabledOnClickListener = new OnClickListener() {
                @Override
                public void onClick(View view) {
                    Animation anim = AnimationUtils.loadAnimation(getContext(), R.anim.overlay_pop);
                    icon.startAnimation(anim);
                }
            };
        }

        return disabledOnClickListener;
    }

    @Override
    public void setOnClickListener(OnClickListener l) {
        enabledOnClickListener = l;

        if (isEnabled) {
            updateViews();
        }
    }

    /**
     * Set the enabledness state of this view. We don't call super.setEnabled, as we want to remain
     * clickable even in the disabled state (but with a different click listener).
     */
    @Override
    public void setEnabled(boolean enabled) {
        if (enabled == isEnabled) {
            return;
        }

        isEnabled = enabled;
        updateViews();
    }
}
