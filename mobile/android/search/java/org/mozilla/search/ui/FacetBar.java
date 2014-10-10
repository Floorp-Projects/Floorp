/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search.ui;

import org.mozilla.gecko.R;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.widget.RadioButton;
import android.widget.RadioGroup;

public class FacetBar extends RadioGroup {

    // Ensure facets have equal width and match the bar's height. Supplying these
    // in styles.xml/FacetButtonStyle does not work. See:
    //   http://stackoverflow.com/questions/24213193/android-ignores-layout-weight-parameter-from-styles-xml
    private static final RadioGroup.LayoutParams FACET_LAYOUT_PARAMS =
            new RadioGroup.LayoutParams(0, LayoutParams.MATCH_PARENT, 1.0f);

    // A loud default color to make it obvious that setUnderlineColor should be called.
    private int underlineColor = Color.RED;

    // Used for assigning unique view ids when facet buttons are being created.
    private int nextButtonId = 0;

    public FacetBar(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Add a new button to the facet bar.
     *
     * @param facetName The text to be used in the button.
     */
    public void addFacet(String facetName) {
        addFacet(facetName, false);
    }

    /**
     * Add a new button to the facet bar.
     *
     * @param facetName The text to be used in the button.
     * @param checked   Whether the button should be checked. If true, the
     *                  onCheckChange listener *will* be fired.
     */
    public void addFacet(String facetName, boolean checked) {
        final FacetButton button = new FacetButton(getContext(), facetName, underlineColor);

        // The ids are used internally by RadioGroup to manage which button is
        // currently checked. Since we are programmatically creating the buttons,
        // we need to manually assign an id.
        button.setId(nextButtonId++);

        // Ensure the buttons are equally spaced.
        button.setLayoutParams(FACET_LAYOUT_PARAMS);

        // If true, this will fire the onCheckChange listener.
        button.setChecked(checked);

        addView(button);
    }

    /**
     * Update the brand color for all of the buttons.
     */
    public void setUnderlineColor(int underlineColor) {
        this.underlineColor = underlineColor;

        if (getChildCount() > 0) {
            for (int i = 0; i < getChildCount(); i++) {
                ((FacetButton) getChildAt(i)).setUnderlineColor(underlineColor);
            }
        }
    }

    /**
     * A custom TextView that includes a bottom border. The bottom border
     * can have a custom color and thickness.
     */
    private static class FacetButton extends RadioButton {

        private final Paint underlinePaint = new Paint();

        public FacetButton(Context context, String text, int color) {
            super(context, null, R.attr.facetButtonStyle);

            setText(text);

            underlinePaint.setStyle(Paint.Style.STROKE);
            underlinePaint.setStrokeWidth(getResources().getDimension(R.dimen.facet_button_underline_thickness));
            underlinePaint.setColor(color);
        }

        @Override
        public void setChecked(boolean checked) {
            super.setChecked(checked);

            // Force the button to redraw to update the underline state.
            invalidate();
        }

        @Override
        protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);
            if (isChecked()) {
                // Translate the line upward so that it isn't clipped by the button's boundary.
                // We divide by 2 since, without offset, the line would be drawn with its
                // midpoint at the bottom of the button -- half of the stroke going up,
                // and half of the stroke getting clipped.
                final float yPos = getHeight() - underlinePaint.getStrokeWidth() / 2;
                canvas.drawLine(0, yPos, getWidth(), yPos, underlinePaint);
            }
        }

        public void setUnderlineColor(int color) {
            underlinePaint.setColor(color);
        }
    }
}
