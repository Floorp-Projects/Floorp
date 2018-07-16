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

import android.app.Activity;
import android.content.Context;

import com.leanplum.ActionContext;
import com.leanplum.Leanplum;
import com.leanplum.LeanplumActivityHelper;
import com.leanplum.callbacks.ActionCallback;
import com.leanplum.callbacks.PostponableAction;

/**
 * Registers a Leanplum action that displays a fullscreen Web Interstitial.
 *
 * @author Atanas Dobrev
 */
public class WebInterstitial extends BaseMessageDialog {
  private static final String NAME = "Web Interstitial";

  public WebInterstitial(Activity activity, WebInterstitialOptions options) {
    super(activity, true, null, options, null);
    this.webOptions = options;
  }

  public static void register() {
    Leanplum.defineAction(NAME, Leanplum.ACTION_KIND_MESSAGE | Leanplum.ACTION_KIND_ACTION,
        WebInterstitialOptions.toArgs(), new ActionCallback() {
          @Override
          public boolean onResponse(final ActionContext context) {
            LeanplumActivityHelper.queueActionUponActive(new PostponableAction() {
              @Override
              public void run() {
                Activity activity = LeanplumActivityHelper.getCurrentActivity();
                if (activity == null) {
                  return;
                }
                WebInterstitial webInterstitial = new WebInterstitial(activity,
                    new WebInterstitialOptions(context));
                if (!activity.isFinishing()) {
                  webInterstitial.show();
                }
              }
            });
            return true;
          }
        });
  }
}
