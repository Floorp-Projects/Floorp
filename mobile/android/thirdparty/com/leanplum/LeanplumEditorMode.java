/*
 * Copyright 2016, Leanplum, Inc. All rights reserved.
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

/**
 * Enum for describing the Editor Mode.
 *
 * @author Ben Marten
 * @deprecated {@link LeanplumEditorMode} will be made private in future releases, since it is not
 * intended to be public API.
 */
@Deprecated
public enum LeanplumEditorMode {
  LP_EDITOR_MODE_INTERFACE(0),
  LP_EDITOR_MODE_EVENT(1);

  private final int value;

  /**
   * Creates a new EditorMode enum with given value.
   */
  LeanplumEditorMode(final int newValue) {
    value = newValue;
  }

  /**
   * Returns the value of the enum entry.
   *
   * @return The value of the entry.
   */
  public int getValue() {
    return value;
  }
}
