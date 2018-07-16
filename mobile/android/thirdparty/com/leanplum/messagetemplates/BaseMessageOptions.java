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

package com.leanplum.messagetemplates;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.util.Log;

import com.leanplum.ActionArgs;
import com.leanplum.ActionContext;
import com.leanplum.messagetemplates.MessageTemplates.Args;
import com.leanplum.utils.BitmapUtil;

import java.io.InputStream;

/**
 * Options used by Center Popup and Interstitial.
 *
 * @author Martin Yanakiev
 */
abstract class BaseMessageOptions {
  private ActionContext context;
  private String title;
  private int titleColor;
  private String messageText;
  private int messageColor;
  private Bitmap backgroundImage;
  private int backgroundColor;
  private String acceptButtonText;
  private int acceptButtonBackgroundColor;
  private int acceptButtonTextColor;

  protected BaseMessageOptions(ActionContext context) {
    this.context = context;
    setTitle(context.stringNamed(Args.TITLE_TEXT));
    setTitleColor(context.numberNamed(Args.TITLE_COLOR).intValue());
    setMessageText(context.stringNamed(Args.MESSAGE_TEXT));
    setMessageColor(context.numberNamed(Args.MESSAGE_COLOR).intValue());
    InputStream imageStream = context.streamNamed(Args.BACKGROUND_IMAGE);
    if (imageStream != null) {
      try {
        setBackgroundImage(BitmapFactory.decodeStream(imageStream));
      } catch (Throwable t) {
        Log.e("Leanplum", "Error loading background image", t);
      }
    }
    setBackgroundColor(context.numberNamed(Args.BACKGROUND_COLOR).intValue());
    setAcceptButtonText(context.stringNamed(Args.ACCEPT_BUTTON_TEXT));
    setAcceptButtonBackgroundColor(context.numberNamed(
        Args.ACCEPT_BUTTON_BACKGROUND_COLOR).intValue());
    setAcceptButtonTextColor(context.numberNamed(
        Args.ACCEPT_BUTTON_TEXT_COLOR).intValue());
  }

  public int getBackgroundColor() {
    return backgroundColor;
  }

  private void setBackgroundColor(int backgroundColor) {
    this.backgroundColor = backgroundColor;
  }

  public String getAcceptButtonText() {
    return acceptButtonText;
  }

  private void setAcceptButtonText(String acceptButtonText) {
    this.acceptButtonText = acceptButtonText;
  }

  public String getTitle() {
    return title;
  }

  private void setTitle(String title) {
    this.title = title;
  }

  public int getTitleColor() {
    return titleColor;
  }

  private void setTitleColor(int titleColor) {
    this.titleColor = titleColor;
  }

  public String getMessageText() {
    return messageText;
  }

  private void setMessageText(String messageText) {
    this.messageText = messageText;
  }

  public int getMessageColor() {
    return messageColor;
  }

  private void setMessageColor(int messageColor) {
    this.messageColor = messageColor;
  }

  public Bitmap getBackgroundImage() {
    return backgroundImage;
  }

  public Bitmap getBackgroundImageRounded(int pixels) {
    return BitmapUtil.getRoundedCornerBitmap(backgroundImage, pixels);
  }

  private void setBackgroundImage(Bitmap backgroundImage) {
    this.backgroundImage = backgroundImage;
  }

  public int getAcceptButtonBackgroundColor() {
    return acceptButtonBackgroundColor;
  }

  private void setAcceptButtonBackgroundColor(int color) {
    this.acceptButtonBackgroundColor = color;
  }

  public int getAcceptButtonTextColor() {
    return acceptButtonTextColor;
  }

  private void setAcceptButtonTextColor(int color) {
    this.acceptButtonTextColor = color;
  }

  public void accept() {
    context.runTrackedActionNamed(Args.ACCEPT_ACTION);
  }

  public static ActionArgs toArgs(Context currentContext) {
    return new ActionArgs()
        .with(Args.TITLE_TEXT,
            MessageTemplates.getApplicationName(currentContext))
        .withColor(Args.TITLE_COLOR, Color.BLACK)
        .with(Args.MESSAGE_TEXT, MessageTemplates.Values.POPUP_MESSAGE)
        .withColor(Args.MESSAGE_COLOR, Color.BLACK)
        .withFile(Args.BACKGROUND_IMAGE, null)
        .withColor(Args.BACKGROUND_COLOR, Color.WHITE)
        .with(Args.ACCEPT_BUTTON_TEXT, MessageTemplates.Values.OK_TEXT)
        .withColor(Args.ACCEPT_BUTTON_BACKGROUND_COLOR, Color.WHITE)
        .withColor(Args.ACCEPT_BUTTON_TEXT_COLOR, Color.argb(255, 0, 122, 255))
        .withAction(Args.ACCEPT_ACTION, null);
  }
}
