/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.overlays.ui;

import org.mozilla.gecko.R;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.util.Log;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.view.LayoutInflater;
import android.view.View;
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

    // We can't use super.isEnabled(), since we want to stay clickable in disabled state.
    private boolean isEnabled = true;

    private final ImageView iconView;
    private final TextView labelView;

    private String enabledText = "";
    private String disabledText = "";

    private OnClickListener enabledOnClickListener;

    public OverlayDialogButton(Context context) {
        this(context, null);
    }

    public OverlayDialogButton(Context context, AttributeSet attrs) {
        super(context, attrs);

        setOrientation(LinearLayout.HORIZONTAL);

        LayoutInflater.from(context).inflate(R.layout.overlay_share_button, this);

        iconView = (ImageView) findViewById(R.id.overlaybtn_icon);
        labelView = (TextView) findViewById(R.id.overlaybtn_label);

        super.setOnClickListener(new OnClickListener() {

            @Override
            public void onClick(View v) {

                if (isEnabled) {
                    if (enabledOnClickListener != null) {
                        enabledOnClickListener.onClick(v);
                    } else {
                        Log.e(LOGTAG, "enabledOnClickListener is null.");
                    }
                } else {
                    Animation anim = AnimationUtils.loadAnimation(getContext(), R.anim.overlay_pop);
                    iconView.startAnimation(anim);
                }
            }
        });

        final TypedArray typedArray = context.obtainStyledAttributes(attrs, R.styleable.OverlayDialogButton);

        Drawable drawable = typedArray.getDrawable(R.styleable.OverlayDialogButton_drawable);
        if (drawable != null) {
            setDrawable(drawable);
        }

        String disabledText = typedArray.getString(R.styleable.OverlayDialogButton_disabledText);
        if (disabledText != null) {
            this.disabledText = disabledText;
        }

        String enabledText = typedArray.getString(R.styleable.OverlayDialogButton_enabledText);
        if (enabledText != null) {
            this.enabledText = enabledText;
        }

        typedArray.recycle();

        setEnabled(true);
    }

    public void setDrawable(Drawable drawable) {
        iconView.setImageDrawable(drawable);
    }

    public void setText(String text) {
        labelView.setText(text);
    }

    @Override
    public void setOnClickListener(OnClickListener listener) {
        enabledOnClickListener = listener;
    }

    /**
     * Set the enabledness state of this view. We don't call super.setEnabled, as we want to remain
     * clickable even in the disabled state (but with a different click listener).
     */
    @Override
    public void setEnabled(boolean enabled) {
        isEnabled = enabled;
        iconView.setEnabled(enabled);
        labelView.setEnabled(enabled);

        if (enabled) {
            setText(enabledText);
        } else {
            setText(disabledText);
        }
    }

}
