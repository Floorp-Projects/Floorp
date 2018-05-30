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

import com.leanplum.ActionArgs;
import com.leanplum.ActionContext;
import com.leanplum.messagetemplates.MessageTemplates.Args;
import com.leanplum.messagetemplates.MessageTemplates.Values;

/**
 * Options used by {@link WebInterstitial}.
 *
 * @author Atanas Dobrev
 */
@SuppressWarnings("WeakerAccess")
public class WebInterstitialOptions {
  private String url;
  private String closeUrl;
  private boolean hasDismissButton;

  protected WebInterstitialOptions(ActionContext context) {
    this.setUrl(context.stringNamed(Args.URL));
    this.setHasDismissButton(context.booleanNamed(Args.HAS_DISMISS_BUTTON));
    this.setCloseUrl(context.stringNamed(Args.CLOSE_URL));
  }

  public String getUrl() {
    return url;
  }

  private void setUrl(String url) {
    this.url = url;
  }

  public boolean hasDismissButton() {
    return hasDismissButton;
  }

  private void setHasDismissButton(boolean hasDismissButton) {
    this.hasDismissButton = hasDismissButton;
  }

  public String getCloseUrl() {
    return closeUrl;
  }

  private void setCloseUrl(String closeUrl) {
    this.closeUrl = closeUrl;
  }

  public static ActionArgs toArgs() {
    return new ActionArgs()
        .with(Args.URL, Values.DEFAULT_URL)
        .with(Args.CLOSE_URL, Values.DEFAULT_CLOSE_URL)
        .with(Args.HAS_DISMISS_BUTTON, Values.DEFAULT_HAS_DISMISS_BUTTON);
  }
}
