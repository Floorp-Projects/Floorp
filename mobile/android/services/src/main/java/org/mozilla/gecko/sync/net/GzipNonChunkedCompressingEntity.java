/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.client.entity.GzipCompressingEntity;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Wrapping entity that compresses content when {@link #writeTo writing}.
 *
 * This differs from {@link GzipCompressingEntity} in that it does not chunk
 * the sent data, therefore replacing the "Transfer-Encoding" HTTP header with
 * the "Content-Length" header required by some servers.
 *
 * However, to measure the content length, the gzipped content will be temporarily
 * stored in memory so be careful what content you send!
 */
public class GzipNonChunkedCompressingEntity extends GzipCompressingEntity {
    final int MAX_BUFFER_SIZE_BYTES = 10 * 1000 * 1000; // 10 MB.

    private byte[] gzippedContent;

    public GzipNonChunkedCompressingEntity(final HttpEntity entity) {
        super(entity);
    }

    /**
     * @return content length for gzipped content or -1 if there is an error
     */
    @Override
    public long getContentLength() {
        try {
            initBuffer();
        } catch (final IOException e) {
            // GzipCompressingEntity always returns -1 in which case a 'Content-Length' header is omitted.
            // Presumably, without it the request will fail (either client-side or server-side).
            return -1;
        }
        return gzippedContent.length;
    }

    @Override
    public boolean isChunked() {
        // "Content-Length" & chunked encoding are mutually exclusive:
        //   https://en.wikipedia.org/wiki/Chunked_transfer_encoding
        return false;
    }

    @Override
    public InputStream getContent() throws IOException {
        initBuffer();
        return new ByteArrayInputStream(gzippedContent);
    }

    @Override
    public void writeTo(final OutputStream outstream) throws IOException {
        initBuffer();
        outstream.write(gzippedContent);
    }

    private void initBuffer() throws IOException {
        if (gzippedContent != null) {
            return;
        }

        final long unzippedContentLength = wrappedEntity.getContentLength();
        if (unzippedContentLength > MAX_BUFFER_SIZE_BYTES) {
            throw new IOException(
                    "Wrapped entity content length, " + unzippedContentLength + " bytes, exceeds max: " + MAX_BUFFER_SIZE_BYTES);
        }

        // The buffer size needed by the gzipped content should be smaller than this,
        // but it's more efficient just to allocate one larger buffer than allocate
        // twice if the gzipped content is too large for the default buffer.
        final ByteArrayOutputStream s = new ByteArrayOutputStream((int) unzippedContentLength);
        try {
            super.writeTo(s);
        } finally {
            s.close();
        }

        gzippedContent = s.toByteArray();
    }
}
