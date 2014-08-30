/*
 * ====================================================================
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 */
package ch.boye.httpclientandroidlib.entity.mime.content;

import java.io.IOException;
import java.io.OutputStream;

import ch.boye.httpclientandroidlib.entity.ContentType;
import ch.boye.httpclientandroidlib.entity.mime.MIME;
import ch.boye.httpclientandroidlib.util.Args;

/**
 * Binary body part backed by a byte array.
 *
 * @see ch.boye.httpclientandroidlib.entity.mime.MultipartEntityBuilder
 *
 * @since 4.1
 */
public class ByteArrayBody extends AbstractContentBody {

    /**
     * The contents of the file contained in this part.
     */
    private final byte[] data;

    /**
     * The name of the file contained in this part.
     */
    private final String filename;

    /**
     * Creates a new ByteArrayBody.
     *
     * @param data The contents of the file contained in this part.
     * @param mimeType The MIME type of the file contained in this part.
     * @param filename The name of the file contained in this part.
     *
     * @deprecated (4.3) use {@link ByteArrayBody#ByteArrayBody(byte[], ContentType, String)}
     *   or {@link ch.boye.httpclientandroidlib.entity.mime.MultipartEntityBuilder}
     */
    @Deprecated
    public ByteArrayBody(final byte[] data, final String mimeType, final String filename) {
        this(data, ContentType.create(mimeType), filename);
    }

    /**
     * @since 4.3
     */
    public ByteArrayBody(final byte[] data, final ContentType contentType, final String filename) {
        super(contentType);
        Args.notNull(data, "byte[]");
        this.data = data;
        this.filename = filename;
    }

    /**
     * Creates a new ByteArrayBody.
     *
     * @param data The contents of the file contained in this part.
     * @param filename The name of the file contained in this part.
     */
    public ByteArrayBody(final byte[] data, final String filename) {
        this(data, "application/octet-stream", filename);
    }

    public String getFilename() {
        return filename;
    }

    public void writeTo(final OutputStream out) throws IOException {
        out.write(data);
    }

    @Override
    public String getCharset() {
        return null;
    }

    public String getTransferEncoding() {
        return MIME.ENC_BINARY;
    }

    public long getContentLength() {
        return data.length;
    }

}
