/*
 * Copyright 2017, Leanplum, Inc. All rights reserved.
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
import android.graphics.Point;
import android.support.annotation.NonNull;
import android.util.Log;
import android.view.MotionEvent;
import com.leanplum.ActionContext;
import com.leanplum.Leanplum;
import com.leanplum.LeanplumActivityHelper;
import com.leanplum.callbacks.ActionCallback;
import com.leanplum.callbacks.PostponableAction;
import com.leanplum.callbacks.VariablesChangedCallback;
import com.leanplum.utils.SizeUtil;

/**
 * Registers a Leanplum action that displays a HTML message.
 *
 * @author Anna Orlova
 */
@SuppressWarnings("WeakerAccess")
public class HTMLTemplate extends BaseMessageDialog {
  private static final String NAME = "HTML";

  public HTMLTemplate(Activity activity, HTMLOptions htmlOptions) {
    super(activity, htmlOptions.isFullScreen(), null, null, htmlOptions);
    this.htmlOptions = htmlOptions;
  }

  @Override
  public boolean dispatchTouchEvent(@NonNull MotionEvent ev) {
      if (!htmlOptions.isFullScreen()) {
          Point size = SizeUtil.getDisplaySize(activity);
          int dialogWidth = webView.getWidth();
          int left = (size.x - dialogWidth) / 2;
          int right = (size.x + dialogWidth) / 2;
          int height = SizeUtil.dpToPx(Leanplum.getContext(), htmlOptions.getHtmlHeight());
          int statusBarHeight = SizeUtil.getStatusBarHeight(Leanplum.getContext());
          int htmlYOffset = htmlOptions.getHtmlYOffset(activity);
          int top;
          int bottom;
          if (MessageTemplates.Args.HTML_ALIGN_BOTTOM.equals(htmlOptions.getHtmlAlign())) {
              top = size.y - height - statusBarHeight - htmlYOffset;
              bottom = size.y - htmlYOffset - statusBarHeight;
          } else {
              top = htmlYOffset + statusBarHeight;
              bottom = height + statusBarHeight + htmlYOffset;
          }

          if (ev.getY() < top || ev.getY() > bottom || ev.getX() < left || ev.getX() > right) {
              if (htmlOptions.isHtmlTabOutsideToClose()) {
                  cancel();
              }
              activity.dispatchTouchEvent(ev);
          }
      }
      return super.dispatchTouchEvent(ev);
  }

  public static void register() {
    Leanplum.defineAction(NAME, Leanplum.ACTION_KIND_MESSAGE | Leanplum.ACTION_KIND_ACTION,
        HTMLOptions.toArgs(), new ActionCallback() {
          @Override
          public boolean onResponse(final ActionContext context) {
            Leanplum.addOnceVariablesChangedAndNoDownloadsPendingHandler(
                new VariablesChangedCallback() {
                  @Override
                  public void variablesChanged() {
                    LeanplumActivityHelper.queueActionUponActive(
                        new PostponableAction() {
                          @Override
                          public void run() {
                            try {
                              HTMLOptions htmlOptions = new HTMLOptions(context);
                              if (htmlOptions.getHtmlTemplate() == null) {
                                return;
                              }
                              final Activity activity = LeanplumActivityHelper.getCurrentActivity();
                              if (activity != null && !activity.isFinishing()) {
                                new HTMLTemplate(activity, htmlOptions);
                              }
                            } catch (Throwable t) {
                              Log.e("Leanplum", "Fail on show HTML In-App message.", t);
                            }
                          }
                        });
                  }
                });
            return true;
          }
        });
  }
}