/*
 * Copyright (C) 2017 The Android Open Source Project
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
package org.mozilla.thirdparty.com.google.android.exoplayer2.source.hls;

import org.mozilla.thirdparty.com.google.android.exoplayer2.C;
import org.mozilla.thirdparty.com.google.android.exoplayer2.upstream.DataSource;

/**
 * Creates {@link DataSource}s for HLS playlists, encryption and media chunks.
 */
public interface HlsDataSourceFactory {

  /**
   * Creates a {@link DataSource} for the given data type.
   *
   * @param dataType The data type for which the {@link DataSource} will be used. One of {@link C}
   *     {@code .DATA_TYPE_*} constants.
   * @return A {@link DataSource} for the given data type.
   */
  DataSource createDataSource(int dataType);

}
