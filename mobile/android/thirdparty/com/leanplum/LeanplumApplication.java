/*
 * Copyright 2013, Leanplum, Inc. All rights reserved.
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

package com.leanplum;

import android.annotation.SuppressLint;
import android.app.Application;
import android.content.Context;
import android.content.res.Resources;

import com.leanplum.annotations.Parser;
import com.leanplum.internal.Constants;

/**
 * Base class for your Application that handles lifecycle events.
 *
 * @author Andrew First
 */
@SuppressLint("Registered")
public class LeanplumApplication extends Application {
  private static LeanplumApplication instance;

  public static LeanplumApplication getInstance() {
    return instance;
  }

  public static Context getContext() {
    return instance;
  }

  @Override
  public void onCreate() {
    super.onCreate();
    instance = this;
    LeanplumActivityHelper.enableLifecycleCallbacks(this);
    Parser.parseVariables(this);
  }

  @Override
  public Resources getResources() {
    if (Constants.isNoop() || !Leanplum.isResourceSyncingEnabled()) {
      return super.getResources();
    }
    return new LeanplumResources(super.getResources());
  }
}
