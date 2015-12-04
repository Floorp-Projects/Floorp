/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.prompts;

import org.json.JSONObject;
import org.mozilla.gecko.R;
import org.mozilla.gecko.widget.BasicColorPicker;

import android.content.Context;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;

public class ColorPickerInput extends PromptInput {
    public static final String INPUT_TYPE = "color";
    public static final String LOGTAG = "GeckoColorPickerInput";

    private final boolean mShowAdvancedButton = true;
    private final int mInitialColor;

    public ColorPickerInput(JSONObject obj) {
        super(obj);
        String init = obj.optString("value");
        mInitialColor = Color.rgb(Integer.parseInt(init.substring(1,3), 16),
                                  Integer.parseInt(init.substring(3,5), 16),
                                  Integer.parseInt(init.substring(5,7), 16));
    }

    @Override
    public View getView(Context context) throws UnsupportedOperationException {
        LayoutInflater inflater = LayoutInflater.from(context);
        mView = inflater.inflate(R.layout.basic_color_picker_dialog, null);

        BasicColorPicker cp = (BasicColorPicker) mView.findViewById(R.id.colorpicker);
        cp.setColor(mInitialColor);

        return mView;
    }

    @Override
    public Object getValue() {
        BasicColorPicker cp = (BasicColorPicker) mView.findViewById(R.id.colorpicker);
        int color = cp.getColor();
        return "#" + Integer.toHexString(color).substring(2);
    }

    @Override
    public boolean getScrollable() {
        return true;
    }

    @Override
    public boolean canApplyInputStyle() {
        return false;
    }
}
