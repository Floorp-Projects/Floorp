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

package ch.boye.httpclientandroidlib.entity;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.util.EntityUtils;

/**
 * A wrapping entity that buffers it content if necessary.
 * The buffered entity is always repeatable.
 * If the wrapped entity is repeatable itself, calls are passed through.
 * If the wrapped entity is not repeatable, the content is read into a
 * buffer once and provided from there as often as required.
 *
 * @since 4.0
 */
public class BufferedHttpEntity extends HttpEntityWrapper {

    private final byte[] buffer;

    /**
     * Creates a new buffered entity wrapper.
     *
     * @param entity   the entity to wrap, not null
     * @throws IllegalArgumentException if wrapped is null
     */
    public BufferedHttpEntity(final HttpEntity entity) throws IOException {
        super(entity);
        if (!entity.isRepeatable() || entity.getContentLength() < 0) {
            this.buffer = EntityUtils.toByteArray(entity);
        } else {
            this.buffer = null;
        }
    }

    public long getContentLength() {
        if (this.buffer != null) {
            return this.buffer.length;
        } else {
            return wrappedEntity.getContentLength();
        }
    }

    public InputStream getContent() throws IOException {
        if (this.buffer != null) {
            return new ByteArrayInputStream(this.buffer);
        } else {
            return wrappedEntity.getContent();
        }
    }

    /**
     * Tells that this entity does not have to be chunked.
     *
     * @return  <code>false</code>
     */
    public boolean isChunked() {
        return (buffer == null) && wrappedEntity.isChunked();
    }

    /**
     * Tells that this entity is repeatable.
     *
     * @return  <code>true</code>
     */
    public boolean isRepeatable() {
        return true;
    }


    public void writeTo(final OutputStream outstream) throws IOException {
        if (outstream == null) {
            throw new IllegalArgumentException("Output stream may not be null");
        }
        if (this.buffer != null) {
            outstream.write(this.buffer);
        } else {
            wrappedEntity.writeTo(outstream);
        }
    }


    // non-javadoc, see interface HttpEntity
    public boolean isStreaming() {
        return (buffer == null) && wrappedEntity.isStreaming();
    }

} // class BufferedHttpEntity
