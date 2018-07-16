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

package com.leanplum;

import android.app.Activity;

/**
 * Describes the API of the visual editor package.
 *
 * @deprecated {@link LeanplumUIEditor} will be made private in future releases, since it is not
 * intended to be public API.
 */
@Deprecated
public interface LeanplumUIEditor {
  /**
   * Enable interface editing via Leanplum.com Visual Editor.
   */
  void allowInterfaceEditing(Boolean isDevelopmentModeEnabled);

  /**
   * Enables Interface editing for the desired activity.
   *
   * @param activity The activity to enable interface editing for.
   */
  void applyInterfaceEdits(Activity activity);

  /**
   * Sets the update flag to true.
   */
  void startUpdating();

  /**
   * Sets the update flag to false.
   */
  void stopUpdating();

  /**
   * Send an immediate update of the UI to the LP server.
   */
  void sendUpdate();

  /**
   * Send an update with given delay of the UI to the LP server.
   */
  void sendUpdateDelayed(int delay);

  /**
   * Send an update of the UI to the LP server, delayed by the default time.
   */
  void sendUpdateDelayedDefault();

  /**
   * Returns the current editor mode.
   *
   * @return The current editor mode.
   */
  LeanplumEditorMode getMode();

  /**
   * Sets the current editor mode.
   *
   * @param mode The editor mode to set.
   */
  void setMode(LeanplumEditorMode mode);
}
