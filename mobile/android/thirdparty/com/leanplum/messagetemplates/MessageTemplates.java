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

/**
 * Registers all of the built-in message templates.
 *
 * @author Andrew First
 */
public class MessageTemplates {
  static class Args {
    // Open URL
    static final String URL = "URL";

    // Alert/confirm arguments.
    static final String TITLE = "Title";
    static final String MESSAGE = "Message";
    static final String ACCEPT_TEXT = "Accept text";
    static final String CANCEL_TEXT = "Cancel text";
    static final String DISMISS_TEXT = "Dismiss text";
    static final String ACCEPT_ACTION = "Accept action";
    static final String CANCEL_ACTION = "Cancel action";
    static final String DISMISS_ACTION = "Dismiss action";

    // Center popup/interstitial arguments.
    static final String TITLE_TEXT = "Title.Text";
    static final String TITLE_COLOR = "Title.Color";
    static final String MESSAGE_TEXT = "Message.Text";
    static final String MESSAGE_COLOR = "Message.Color";
    static final String ACCEPT_BUTTON_TEXT = "Accept button.Text";
    static final String ACCEPT_BUTTON_BACKGROUND_COLOR = "Accept button.Background color";
    static final String ACCEPT_BUTTON_TEXT_COLOR = "Accept button.Text color";
    static final String BACKGROUND_IMAGE = "Background image";
    static final String BACKGROUND_COLOR = "Background color";
    static final String LAYOUT_WIDTH = "Layout.Width";
    static final String LAYOUT_HEIGHT = "Layout.Height";
    static final String HTML_WIDTH = "HTML Width";
    static final String HTML_HEIGHT = "HTML Height";
    static final String HTML_Y_OFFSET = "HTML Y Offset";
    static final String HTML_TAP_OUTSIDE_TO_CLOSE = "Tap Outside to Close";
    static final String HTML_ALIGN = "HTML Align";
    static final String HTML_ALIGN_TOP = "Top";
    static final String HTML_ALIGN_BOTTOM = "Bottom";

    // Web interstitial arguments.
    static final String CLOSE_URL = "Close URL";
    static final String HAS_DISMISS_BUTTON = "Has dismiss button";

    // HTML Template arguments.
    static final String OPEN_URL = "Open URL";
    static final String TRACK_URL = "Track URL";
    static final String ACTION_URL = "Action URL";
    static final String TRACK_ACTION_URL = "Track Action URL";
  }

  static class Values {
    static final String ALERT_MESSAGE = "Alert message goes here.";
    static final String CONFIRM_MESSAGE = "Confirmation message goes here.";
    static final String POPUP_MESSAGE = "Popup message goes here.";
    static final String INTERSTITIAL_MESSAGE = "Interstitial message goes here.";
    static final String OK_TEXT = "OK";
    static final String YES_TEXT = "Yes";
    static final String NO_TEXT = "No";
    static final int CENTER_POPUP_WIDTH = 300;
    static final int CENTER_POPUP_HEIGHT = 250;
    static final int DEFAULT_HTML_HEIGHT = 0;
    static final String DEFAULT_HTML_ALING = Args.HTML_ALIGN_TOP;

    // Open URL.
    static final String DEFAULT_URL = "http://www.example.com";

    // Web interstitial values.
    static final String DEFAULT_CLOSE_URL = "http://leanplum:close";
    static final boolean DEFAULT_HAS_DISMISS_BUTTON = true;

    // HTML Template values.
    public static final String FILE_PREFIX = "__file__";
    public static final String HTML_TEMPLATE_PREFIX = "__file__Template";
    static final String DEFAULT_OPEN_URL = "http://leanplum:loadFinished";
    static final String DEFAULT_TRACK_URL = "http://leanplum:track";
    static final String DEFAULT_ACTION_URL = "http://leanplum:runAction";
    static final String DEFAULT_TRACK_ACTION_URL = "http://leanplum:runTrackedAction";

  }

  private static boolean registered = false;

  static String getApplicationName(Context context) {
    int stringId = context.getApplicationInfo().labelRes;
    if (stringId == 0) {
      return context.getApplicationInfo().loadLabel(context.getPackageManager()).toString();
    }
    return context.getString(stringId);
  }

  public synchronized static void register(Context currentContext) {
    if (registered) {
      return;
    }
    registered = true;
    OpenURL.register();
    Alert.register(currentContext);
    Confirm.register(currentContext);
    CenterPopup.register(currentContext);
    Interstitial.register(currentContext);
    WebInterstitial.register();
    HTMLTemplate.register();
  }
}
