/*
 * Copyright (C) 2014 Lucas Rocha
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.lucasr.dspec;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * Draw a baseline grid, keylines, and spacing markers on top of a {@link View}.
 *
 * A {@link DesignSpec} can be configure programmatically as follows:
 * <ol>
 * <li>Toggle baseline grid visibility with {@link #setBaselineGridVisible(boolean)}.</li>
 * <li>Change baseline grid cell width with {@link #setBaselineGridCellSize(float)}.
 * <li>Change baseline grid color with {@link #setBaselineGridColor(int)}.
 * <li>Toggle keylines visibility with {@link #setKeylinesVisible(boolean)}.
 * <li>Change keylines color with {@link #setKeylinesColor(int)}.
 * <li>Add keylines with {@link #addKeyline(float, From)}.
 * <li>Toggle spacings visibility with {@link #setSpacingsVisible(boolean)}.
 * <li>Change spacings color with {@link #setSpacingsColor(int)}.
 * <li>Add spacing with {@link #addSpacing(float, float, From)}.
 * </ol>
 *
 * You can also define a {@link DesignSpec} via a raw JSON resource as follows:
 * <pre>
 * {
 *     "baselineGridVisible": true,
 *     "baselineGridCellSize": 8,
 *     "keylines": [
 *         { "offset": 16,
 *           "from": "LEFT" },
 *         { "offset": 72,
 *           "from": "LEFT" },
 *         { "offset": 16,
 *           "from": "RIGHT" }
 *     ],
 *     "spacings": [
 *         { "offset": 0,
 *           "size": 16,
 *           "from": "LEFT" },
 *         { "offset": 56,
 *           "size": 16,
 *           "from": "LEFT" },
 *         { "offset": 0,
 *           "size": 16,
 *           "from": "RIGHT" }
 *     ]
 * }
 * </pre>
 *
 * The {@link From} arguments implicitly define the orientation of the given
 * keyline or spacing i.e. {@link From#LEFT}, {@link From#RIGHT}, {@link From#HORIZONTAL_CENTER}
 * are implicitly vertical; and {@link From#TOP}, {@link From#BOTTOM}, {@link From#VERTICAL_CENTER}
 * are implicitly horizontal.
 *
 * The {@link From} arguments also define the 'direction' of the offsets and sizes in keylines and
 * spacings. For example, a keyline using {@link From#RIGHT} will have its offset measured from
 * right to left of the target {@link View}.
 *
 * The easiest way to use a {@link DesignSpec} is by enclosing your target {@link View} with
 * a {@link DesignSpecFrameLayout} using the {@code designSpec} attribute as follows:
 * <pre>
 * <org.lucasr.dspec.DesignSpecFrameLayout
 *     xmlns:android="http://schemas.android.com/apk/res/android"
 *     android:id="@+id/design_spec"
 *     android:layout_width="match_parent"
 *     android:layout_height="match_parent"
 *     app:designSpec="@raw/my_spec">
 *
 *     ...
 *
 * </org.lucasr.dspec.DesignSpecFrameLayout>
 * </pre>
 *
 * Where {@code @raw/my_spec} is a raw JSON resource. Because the {@link DesignSpec} is
 * defined in an Android resource, you can vary it according to the target form factor using
 * well-known resource qualifiers making it easy to define different specs for phones and tablets.
 *
 * Because {@link DesignSpec} is a {@link Drawable}, you can simply add it to any
 * {@link android.view.ViewOverlay} if you're running your app on API level >= 18:
 *
 * <pre>
 * DesignSpec designSpec = DesignSpec.fromResource(someView, R.raw.some_spec);
 * someView.getOverlay().add(designSpec);
 * </pre>
 *
 * @see DesignSpecFrameLayout
 * @see #fromResource(View, int)
 */
public class DesignSpec extends Drawable {
    private static final boolean DEFAULT_BASELINE_GRID_VISIBLE = false;
    private static final boolean DEFAULT_KEYLINES_VISIBLE = true;
    private static final boolean DEFAULT_SPACINGS_VISIBLE = true;

    private static final int DEFAULT_BASELINE_GRID_CELL_SIZE_DIP = 8;

    private static final String DEFAULT_BASELINE_GRID_COLOR = "#44C2185B";
    private static final String DEFAULT_KEYLINE_COLOR = "#CCC2185B";
    private static final String DEFAULT_SPACING_COLOR = "#CC89FDFD";

    private static final float KEYLINE_STROKE_WIDTH_DIP = 1.1f;

    private static final String JSON_KEY_BASELINE_GRID_VISIBLE = "baselineGridVisible";
    private static final String JSON_KEY_BASELINE_GRID_CELL_SIZE = "baselineGridCellSize";
    private static final String JSON_KEY_BASELINE_GRID_COLOR = "baselineGridColor";

    private static final String JSON_KEY_KEYLINES_VISIBLE = "keylinesVisible";
    private static final String JSON_KEY_KEYLINES_COLOR = "keylinesColor";
    private static final String JSON_KEY_KEYLINES = "keylines";

    private static final String JSON_KEY_OFFSET = "offset";
    private static final String JSON_KEY_SIZE = "size";
    private static final String JSON_KEY_FROM = "from";

    private static final String JSON_KEY_SPACINGS_VISIBLE = "spacingsVisible";
    private static final String JSON_KEY_SPACINGS_COLOR = "spacingsColor";
    private static final String JSON_KEY_SPACINGS = "spacings";

    /**
     * Defined the reference point from which keyline/spacing offsets and sizes
     * will be calculated.
     */
    public enum From {
        LEFT,
        RIGHT,
        TOP,
        BOTTOM,
        VERTICAL_CENTER,
        HORIZONTAL_CENTER
    }

    private static class Keyline {
        public final float position;
        public final From from;

        public Keyline(float position, From from) {
            this.position = position;
            this.from = from;
        }

        @Override
        public boolean equals(Object o) {
            if (!(o instanceof Keyline)) {
                return false;
            }

            if (o == this) {
                return true;
            }

            final Keyline other = (Keyline) o;
            return (this.position == other.position && this.from == other.from);
        }
    }

    private static class Spacing {
        public final float offset;
        public final float size;
        public final From from;

        public Spacing(float offset, float size, From from) {
            this.offset = offset;
            this.size = size;
            this.from = from;
        }

        @Override
        public boolean equals(Object o) {
            if (!(o instanceof Keyline)) {
                return false;
            }

            if (o == this) {
                return true;
            }

            final Spacing other = (Spacing) o;
            return (this.offset == other.offset &&
                    this.size == other.size &&
                    this.from == other.from);
        }
    }

    private final View mHostView;

    private final float mDensity;

    private boolean mBaselineGridVisible = DEFAULT_BASELINE_GRID_VISIBLE;
    private float mBaselineGridCellSize;
    private final Paint mBaselineGridPaint;

    private boolean mKeylinesVisible = DEFAULT_KEYLINES_VISIBLE;
    private final Paint mKeylinesPaint;
    private final List<Keyline> mKeylines;

    private boolean mSpacingsVisible = DEFAULT_SPACINGS_VISIBLE;
    private final Paint mSpacingsPaint;
    private final List<Spacing> mSpacings;

    public DesignSpec(Resources resources, View hostView) {
        mHostView = hostView;
        mDensity = resources.getDisplayMetrics().density;

        mKeylines = new ArrayList<Keyline>();
        mSpacings = new ArrayList<Spacing>();

        mBaselineGridPaint = new Paint();
        mBaselineGridPaint.setColor(Color.parseColor(DEFAULT_BASELINE_GRID_COLOR));

        mKeylinesPaint = new Paint();
        mKeylinesPaint.setStrokeWidth(KEYLINE_STROKE_WIDTH_DIP * mDensity);
        mKeylinesPaint.setColor(Color.parseColor(DEFAULT_KEYLINE_COLOR));

        mSpacingsPaint = new Paint();
        mSpacingsPaint.setColor(Color.parseColor(DEFAULT_SPACING_COLOR));

        mBaselineGridCellSize = mDensity * DEFAULT_BASELINE_GRID_CELL_SIZE_DIP;
    }

    /**
     * Whether or not the baseline grid should be drawn.
     */
    public boolean isBaselineGridVisible() {
        return mBaselineGridVisible;
    }

    /**
     * Sets the baseline grid visibility.
     */
    public DesignSpec setBaselineGridVisible(boolean visible) {
        if (mBaselineGridVisible == visible) {
            return this;
        }

        mBaselineGridVisible = visible;
        invalidateSelf();

        return this;
    }

    /**
     * Sets the size of the baseline grid cells. By default, it uses the
     * material design 8dp cell size.
     */
    public DesignSpec setBaselineGridCellSize(float cellSize) {
        if (mBaselineGridCellSize == cellSize) {
            return this;
        }

        mBaselineGridCellSize = cellSize;
        invalidateSelf();

        return this;
    }

    /**
     * Sets the baseline grid color.
     */
    public DesignSpec setBaselineGridColor(int color) {
        if (mBaselineGridPaint.getColor() == color) {
            return this;
        }

        mBaselineGridPaint.setColor(color);
        invalidateSelf();

        return this;
    }

    /**
     * Whether or not the keylines should be drawn.
     */
    public boolean areKeylinesVisible() {
        return mKeylinesVisible;
    }

    /**
     * Sets the visibility of keylines.
     */
    public DesignSpec setKeylinesVisible(boolean visible) {
        if (mKeylinesVisible == visible) {
            return this;
        }

        mKeylinesVisible = visible;
        invalidateSelf();

        return this;
    }

    /**
     * Sets the keyline color.
     */
    public DesignSpec setKeylinesColor(int color) {
        if (mKeylinesPaint.getColor() == color) {
            return this;
        }

        mKeylinesPaint.setColor(color);
        invalidateSelf();

        return this;
    }

    /**
     * Adds a keyline to the {@link DesignSpec}.
     */
    public DesignSpec addKeyline(float position, From from) {
        final Keyline keyline = new Keyline(position * mDensity, from);
        if (mKeylines.contains(keyline)) {
            return this;
        }

        mKeylines.add(keyline);
        return this;
    }

    /**
     * Whether or not the spacing markers should be drawn.
     */
    public boolean areSpacingsVisible() {
        return mSpacingsVisible;
    }

    /**
     * Sets the visibility of spacing markers.
     */
    public DesignSpec setSpacingsVisible(boolean visible) {
        if (mSpacingsVisible == visible) {
            return this;
        }

        mSpacingsVisible = visible;
        invalidateSelf();

        return this;
    }

    /**
     * Sets the spacing mark color.
     */
    public DesignSpec setSpacingsColor(int color) {
        if (mSpacingsPaint.getColor() == color) {
            return this;
        }

        mSpacingsPaint.setColor(color);
        invalidateSelf();

        return this;
    }

    /**
     * Adds a spacing mark to the {@link DesignSpec}.
     */
    public DesignSpec addSpacing(float position, float size, From from) {
        final Spacing spacing = new Spacing(position * mDensity, size * mDensity, from);
        if (mSpacings.contains(spacing)) {
            return this;
        }

        mSpacings.add(spacing);
        return this;
    }

    private void drawBaselineGrid(Canvas canvas) {
        if (!mBaselineGridVisible) {
            return;
        }

        final int width = getIntrinsicWidth();
        final int height = getIntrinsicHeight();

        float x = mBaselineGridCellSize;
        while (x < width) {
            canvas.drawLine(x, 0, x, height, mBaselineGridPaint);
            x += mBaselineGridCellSize;
        }

        float y = mBaselineGridCellSize;
        while (y < height) {
            canvas.drawLine(0, y, width, y, mBaselineGridPaint);
            y += mBaselineGridCellSize;
        }
    }

    private void drawKeylines(Canvas canvas) {
        if (!mKeylinesVisible) {
            return;
        }

        final int width = getIntrinsicWidth();
        final int height = getIntrinsicHeight();

        final int count = mKeylines.size();
        for (int i = 0; i < count; i++) {
            final Keyline keyline = mKeylines.get(i);

            final float position;
            switch (keyline.from) {
                case LEFT:
                case TOP:
                    position = keyline.position;
                    break;

                case RIGHT:
                    position = width - keyline.position;
                    break;

                case BOTTOM:
                    position = height - keyline.position;
                    break;

                case VERTICAL_CENTER:
                    position = (height / 2) + keyline.position;
                    break;

                case HORIZONTAL_CENTER:
                    position = (width / 2) + keyline.position;
                    break;

                default:
                    throw new IllegalStateException("Invalid keyline offset");
            }

            switch (keyline.from) {
                case LEFT:
                case RIGHT:
                case HORIZONTAL_CENTER:
                    canvas.drawLine(position, 0, position, height, mKeylinesPaint);
                    break;

                case TOP:
                case BOTTOM:
                case VERTICAL_CENTER:
                    canvas.drawLine(0, position, width, position, mKeylinesPaint);
                    break;
            }
        }
    }

    private void drawSpacings(Canvas canvas) {
        if (!mSpacingsVisible) {
            return;
        }

        final int width = getIntrinsicWidth();
        final int height = getIntrinsicHeight();

        final int count = mSpacings.size();
        for (int i = 0; i < count; i++) {
            final Spacing spacing = mSpacings.get(i);

            final float position1;
            final float position2;
            switch (spacing.from) {
                case LEFT:
                case TOP:
                    position1 = spacing.offset;
                    position2 = position1 + spacing.size;
                    break;

                case RIGHT:
                    position1 = width - spacing.offset + spacing.size;
                    position2 = width - spacing.offset;
                    break;

                case BOTTOM:
                    position1 = height - spacing.offset + spacing.size;
                    position2 = height - spacing.offset;
                    break;

                case VERTICAL_CENTER:
                    position1 = (height / 2) + spacing.offset;
                    position2 = position1 + spacing.size;
                    break;

                case HORIZONTAL_CENTER:
                    position1 = (width / 2) + spacing.offset;
                    position2 = position1 + spacing.size;
                    break;

                default:
                    throw new IllegalStateException("Invalid spacing offset");
            }

            switch (spacing.from) {
                case LEFT:
                case RIGHT:
                case HORIZONTAL_CENTER:
                    canvas.drawRect(position1, 0, position2, height, mSpacingsPaint);
                    break;

                case TOP:
                case BOTTOM:
                case VERTICAL_CENTER:
                    canvas.drawRect(0, position1, width, position2, mSpacingsPaint);
                    break;
            }
        }
    }

    /**
     * Draws the {@link DesignSpec}. You should call this in your {@link View}'s
     * {@link View#onDraw(Canvas)} method if you're not simply enclosing it with a
     * {@link DesignSpecFrameLayout}.
     */
    @Override
    public void draw(Canvas canvas) {
        drawSpacings(canvas);
        drawBaselineGrid(canvas);
        drawKeylines(canvas);
    }

    @Override
    public int getIntrinsicWidth() {
        return mHostView.getWidth();
    }

    @Override
    public int getIntrinsicHeight() {
        return mHostView.getHeight();
    }

    @Override
    public void setAlpha(int alpha) {
        mBaselineGridPaint.setAlpha(alpha);
        mKeylinesPaint.setAlpha(alpha);
        mSpacingsPaint.setAlpha(alpha);
    }

    @Override
    public void setColorFilter(ColorFilter cf) {
        mBaselineGridPaint.setColorFilter(cf);
        mKeylinesPaint.setColorFilter(cf);
        mSpacingsPaint.setColorFilter(cf);
    }

    @Override
    public int getOpacity() {
        return  PixelFormat.TRANSLUCENT;
    }

    /**
     * Creates a new {@link DesignSpec} instance from a resource ID using a {@link View}
     * that will provide the {@link DesignSpec}'s intrinsic dimensions.
     *
     * @param view The {@link View} who will own the new {@link DesignSpec} instance.
     * @param resId The resource ID pointing to a raw JSON resource.
     * @return The newly created {@link DesignSpec} instance.
     */
    public static DesignSpec fromResource(View view, int resId) {
        final Resources resources = view.getResources();
        final DesignSpec spec = new DesignSpec(resources, view);
        if (resId == 0) {
            return spec;
        }

        final JSONObject json;
        try {
            json = RawResource.getAsJSON(resources, resId);
        } catch (IOException e) {
            throw new IllegalStateException("Could not read design spec resource", e);
        }

        final float density = resources.getDisplayMetrics().density;

        spec.setBaselineGridCellSize(density * json.optInt(JSON_KEY_BASELINE_GRID_CELL_SIZE,
                DEFAULT_BASELINE_GRID_CELL_SIZE_DIP));

        spec.setBaselineGridVisible(json.optBoolean(JSON_KEY_BASELINE_GRID_VISIBLE,
                DEFAULT_BASELINE_GRID_VISIBLE));
        spec.setKeylinesVisible(json.optBoolean(JSON_KEY_KEYLINES_VISIBLE,
                DEFAULT_KEYLINES_VISIBLE));
        spec.setSpacingsVisible(json.optBoolean(JSON_KEY_SPACINGS_VISIBLE,
                DEFAULT_SPACINGS_VISIBLE));

        spec.setBaselineGridColor(Color.parseColor(json.optString(JSON_KEY_BASELINE_GRID_COLOR,
                DEFAULT_BASELINE_GRID_COLOR)));
        spec.setKeylinesColor(Color.parseColor(json.optString(JSON_KEY_KEYLINES_COLOR,
                DEFAULT_KEYLINE_COLOR)));
        spec.setSpacingsColor(Color.parseColor(json.optString(JSON_KEY_SPACINGS_COLOR,
                DEFAULT_SPACING_COLOR)));

        final JSONArray keylines = json.optJSONArray(JSON_KEY_KEYLINES);
        if (keylines != null) {
            final int keylineCount = keylines.length();
            for (int i = 0; i < keylineCount; i++) {
                try {
                    final JSONObject keyline = keylines.getJSONObject(i);
                    spec.addKeyline(keyline.getInt(JSON_KEY_OFFSET),
                            From.valueOf(keyline.getString(JSON_KEY_FROM).toUpperCase()));
                } catch (JSONException e) {
                    continue;
                }
            }
        }

        final JSONArray spacings = json.optJSONArray(JSON_KEY_SPACINGS);
        if (spacings != null) {
            final int spacingCount = spacings.length();
            for (int i = 0; i < spacingCount; i++) {
                try {
                    final JSONObject spacing = spacings.getJSONObject(i);
                    spec.addSpacing(spacing.getInt(JSON_KEY_OFFSET), spacing.getInt(JSON_KEY_SIZE),
                            From.valueOf(spacing.getString(JSON_KEY_FROM).toUpperCase()));
                } catch (JSONException e) {
                    continue;
                }
            }
        }

        return spec;
    }
}
