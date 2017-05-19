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
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Paint.Style;
import android.graphics.PorterDuff.Mode;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.RelativeLayout.LayoutParams;

import com.leanplum.utils.BitmapUtil;
import com.leanplum.utils.SizeUtil;

/**
 * The image background on a Center Popup or Interstitial dialog.
 *
 * @author Martin Yanakiev
 */
public class BackgroundImageView extends ImageView {
  private Paint paint = new Paint();
  private boolean fullscreen;
  private Matrix emptyMatrix = new Matrix();
  private boolean loadedBitmap;

  public BackgroundImageView(Context context, AttributeSet attrs, int defStyle) {
    super(context, attrs, defStyle);
    init();
  }

  public BackgroundImageView(Context context, AttributeSet attrs) {
    super(context, attrs);
    init();
  }

  public BackgroundImageView(Context context, boolean fullscreen) {
    super(context);
    init();
    this.fullscreen = fullscreen;
  }

  private void init() {
    paint.setColor(0xFF00FF00);
    paint.setStrokeWidth(2);
    paint.setStyle(Style.FILL_AND_STROKE);
  }

  @Override
  protected void onDraw(Canvas canvas) {
    super.onDraw(canvas);
    if (fullscreen) {
      return;
    }
    if (loadedBitmap) {
      loadedBitmap = false;
      return;
    }
    Bitmap bitmap = loadBitmapFromView(this);
    canvas.drawColor(Color.TRANSPARENT, Mode.CLEAR);
    bitmap = BitmapUtil.getRoundedCornerBitmap(bitmap, SizeUtil.dp20);
    canvas.drawBitmap(bitmap, emptyMatrix, paint);
  }

  public Bitmap loadBitmapFromView(View view) {
    if (view.getMeasuredHeight() <= 0) {
      view.measure(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
    }
    Bitmap bitmap = Bitmap.createBitmap(view.getMeasuredWidth(), view.getMeasuredHeight(),
        Bitmap.Config.ARGB_8888);
    Canvas canvas = new Canvas(bitmap);
    view.layout(0, 0, view.getMeasuredWidth(), view.getMeasuredHeight());
    loadedBitmap = true;
    view.draw(canvas);
    return bitmap;
  }
}
