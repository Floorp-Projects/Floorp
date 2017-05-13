/*
 * Copyright 2014, Leanplum, Inc. All rights reserved.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

package com.leanplum.views;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Paint.Style;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

import com.leanplum.utils.SizeUtil;

/**
 * The close button on a Center Popup or Interstitial dialog.
 *
 * @author Martin Yanakiev
 */
public class CloseButton extends View {
  private Paint circlePaint = new Paint();
  private Paint circlePressedPaint = new Paint();
  private Paint linePaint = new Paint();
  private float size;
  private float x1;
  private float y1;
  private float x2;
  private float y2;
  private boolean isPressed = false;

  public CloseButton(Context context) {
    super(context);
    initLabelView();
  }

  public CloseButton(Context context, AttributeSet attrs) {
    super(context, attrs);
    initLabelView();
  }

  private void initLabelView() {
    circlePaint.setAntiAlias(true);
    circlePaint.setColor(0xFFDDDDDD);
    circlePaint.setStrokeWidth(2);
    circlePaint.setStyle(Style.FILL_AND_STROKE);
    circlePressedPaint.setAntiAlias(true);
    circlePressedPaint.setColor(0xFF999999);
    circlePressedPaint.setStrokeWidth(2);
    circlePressedPaint.setStyle(Style.FILL_AND_STROKE);
    linePaint.setAntiAlias(true);
    linePaint.setColor(0xFF000000);
    linePaint.setStrokeWidth(3);
    linePaint.setStyle(Style.FILL_AND_STROKE);
    size = SizeUtil.dp30;
    x1 = size * (2 / (float) 6);
    x2 = size * (4 / (float) 6);
    y1 = size * (2 / (float) 6);
    y2 = size * (4 / (float) 6);
  }

  @Override
  public boolean performClick() {
    return super.performClick();
  }

  @Override
  public boolean onTouchEvent(MotionEvent event) {
    if (event.getAction() == MotionEvent.ACTION_DOWN) {
      isPressed = true;
      invalidate();
      return true;
    } else if (event.getAction() == MotionEvent.ACTION_UP) {
      isPressed = false;
      invalidate();
      performClick();
      return true;
    }
    return super.onTouchEvent(event);
  }

  @Override
  protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
    super.onMeasure(widthMeasureSpec, widthMeasureSpec);
    setMeasuredDimension((int) size, (int) size);
  }

  @Override
  protected void onDraw(Canvas canvas) {
    super.onDraw(canvas);
    Paint backgroundPaint = isPressed ? circlePressedPaint : circlePaint;
    canvas.drawCircle(getWidth() / 2f, getHeight() / 2f, (getWidth() / 2f) - 1, backgroundPaint);
    canvas.drawLine(x1, y1, x2, y2, linePaint);
    canvas.drawLine(x2, y1, x1, y2, linePaint);
  }
}
