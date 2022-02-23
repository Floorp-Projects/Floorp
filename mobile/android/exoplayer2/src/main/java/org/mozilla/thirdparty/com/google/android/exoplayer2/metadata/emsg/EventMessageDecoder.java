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
package org.mozilla.thirdparty.com.google.android.exoplayer2.metadata.emsg;

import org.mozilla.thirdparty.com.google.android.exoplayer2.metadata.Metadata;
import org.mozilla.thirdparty.com.google.android.exoplayer2.metadata.MetadataDecoder;
import org.mozilla.thirdparty.com.google.android.exoplayer2.metadata.MetadataInputBuffer;
import org.mozilla.thirdparty.com.google.android.exoplayer2.util.Assertions;
import org.mozilla.thirdparty.com.google.android.exoplayer2.util.ParsableByteArray;
import java.nio.ByteBuffer;
import java.util.Arrays;

/** Decodes data encoded by {@link EventMessageEncoder}. */
public final class EventMessageDecoder implements MetadataDecoder {

  @SuppressWarnings("ByteBufferBackingArray")
  @Override
  public Metadata decode(MetadataInputBuffer inputBuffer) {
    ByteBuffer buffer = Assertions.checkNotNull(inputBuffer.data);
    byte[] data = buffer.array();
    int size = buffer.limit();
    return new Metadata(decode(new ParsableByteArray(data, size)));
  }

  public EventMessage decode(ParsableByteArray emsgData) {
    String schemeIdUri = Assertions.checkNotNull(emsgData.readNullTerminatedString());
    String value = Assertions.checkNotNull(emsgData.readNullTerminatedString());
    long durationMs = emsgData.readUnsignedInt();
    long id = emsgData.readUnsignedInt();
    byte[] messageData =
        Arrays.copyOfRange(emsgData.data, emsgData.getPosition(), emsgData.limit());
    return new EventMessage(schemeIdUri, value, durationMs, id, messageData);
  }
}
