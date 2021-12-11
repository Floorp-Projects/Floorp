/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.mozilla.thirdparty.com.google.android.exoplayer2.metadata;

import androidx.annotation.Nullable;

/**
 * Decodes metadata from binary data.
 */
public interface MetadataDecoder {

  /**
   * Decodes a {@link Metadata} element from the provided input buffer.
   *
   * @param inputBuffer The input buffer to decode.
   * @return The decoded metadata object, or null if the metadata could not be decoded.
   */
  @Nullable
  Metadata decode(MetadataInputBuffer inputBuffer);
}
