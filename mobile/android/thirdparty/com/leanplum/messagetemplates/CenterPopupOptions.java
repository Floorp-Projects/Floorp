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

/**
 * Options used by {@link CenterPopup}.
 *
 * @author Martin Yanakiev
 */
public class CenterPopupOptions extends BaseMessageOptions {
  private int width;
  private int height;

  public CenterPopupOptions(ActionContext context) {
    super(context);
    setWidth(context.numberNamed(Args.LAYOUT_WIDTH).intValue());
    setHeight(context.numberNamed(Args.LAYOUT_HEIGHT).intValue());
  }

  public int getWidth() {
    return width;
  }

  private void setWidth(int width) {
    this.width = width;
  }

  public int getHeight() {
    return height;
  }

  private void setHeight(int height) {
    this.height = height;
  }

  public static ActionArgs toArgs(Context currentContext) {
    return BaseMessageOptions.toArgs(currentContext)
        .with(Args.LAYOUT_WIDTH, MessageTemplates.Values.CENTER_POPUP_WIDTH)
        .with(Args.LAYOUT_HEIGHT, MessageTemplates.Values.CENTER_POPUP_HEIGHT);
  }
}
