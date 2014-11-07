/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.Shape;

public class ResizablePathDrawable extends ShapeDrawable {
    private final ColorStateList colorStateList;
    private int currentColor;

    public ResizablePathDrawable(NonScaledPathShape shape, int color) {
        this(shape, ColorStateList.valueOf(color));
    }

    public ResizablePathDrawable(NonScaledPathShape shape, ColorStateList colorStateList) {
        super(shape);
        this.colorStateList = colorStateList;
        updateColor(getState());
    }

    private boolean updateColor(int[] stateSet) {
        int newColor = colorStateList.getColorForState(stateSet, Color.WHITE);
        if (newColor != currentColor) {
            currentColor = newColor;
            invalidateSelf();
            return true;
        }

        return false;
    }

    public Path getPath() {
        final NonScaledPathShape shape = (NonScaledPathShape) getShape();
        return shape.path;
    }

    @Override
    public boolean isStateful() {
        return true;
    }

    @Override
    protected void onDraw(Shape shape, Canvas canvas, Paint paint) {
        paint.setColor(currentColor);
        super.onDraw(shape, canvas, paint);
    }

    @Override
    protected boolean onStateChange(int[] stateSet) {
        return updateColor(stateSet);
    }

    /**
     * Path-based shape implementation that re-creates the path
     * when it gets resized as opposed to PathShape's scaling
     * behaviour.
     */
    public static class NonScaledPathShape extends Shape {
        private Path path;

        public NonScaledPathShape() {
            path = new Path();
        }

        @Override
        public void draw(Canvas canvas, Paint paint) {
            // No point in drawing the shape if it's not
            // going to be visible.
            if (paint.getColor() == Color.TRANSPARENT) {
                return;
            }

            canvas.drawPath(path, paint);
        }

        protected Path getPath() {
            return path;
        }

        @Override
        public NonScaledPathShape clone() throws CloneNotSupportedException {
            final NonScaledPathShape clonedShape = (NonScaledPathShape) super.clone();
            clonedShape.path = new Path(path);
            return clonedShape;
        }
    }
}
