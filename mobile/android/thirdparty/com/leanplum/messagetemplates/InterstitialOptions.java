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

/**
 * Options used by {@link Interstitial}.
 *
 * @author Martin Yanakiev
 */
public class InterstitialOptions extends BaseMessageOptions {
  public InterstitialOptions(ActionContext context) {
    super(context);
    // Set specific properties for interstitial popup.
  }

  public static ActionArgs toArgs(Context currentContext) {
    return BaseMessageOptions.toArgs(currentContext)
        .with(MessageTemplates.Args.MESSAGE_TEXT, MessageTemplates.Values.INTERSTITIAL_MESSAGE);
    // Add specific args for interstitial popup.
  }
}
