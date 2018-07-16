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
package com.leanplum.activities;

import android.content.res.Resources;
import android.preference.PreferenceActivity;

import com.leanplum.Leanplum;
import com.leanplum.LeanplumActivityHelper;

/**
 *  @deprecated due to rising minimal API to 14. This class will be removed in a
 *  future major release. Please use {@link LeanplumActivityHelper} to track your activities
 *  automatically.
 */
@Deprecated
public class LeanplumPreferenceActivity extends PreferenceActivity {
  private LeanplumActivityHelper helper;

  private LeanplumActivityHelper getHelper() {
    if (helper == null) {
      helper = new LeanplumActivityHelper(this);
    }
    return helper;
  }

  @Override
  protected void onPause() {
    super.onPause();
    getHelper().onPause();
  }

  @Override
  protected void onStop() {
    super.onStop();
    getHelper().onStop();
  }

  @Override
  protected void onResume() {
    super.onResume();
    getHelper().onResume();
  }

  @Override
  public Resources getResources() {
    if (Leanplum.isTestModeEnabled() || !Leanplum.isResourceSyncingEnabled()) {
      return super.getResources();
    }
    return getHelper().getLeanplumResources(super.getResources());
  }

  @Override
  public void setContentView(final int layoutResID) {
    if (Leanplum.isTestModeEnabled() || !Leanplum.isResourceSyncingEnabled()) {
      super.setContentView(layoutResID);
      return;
    }
    getHelper().setContentView(layoutResID);
  }
}
