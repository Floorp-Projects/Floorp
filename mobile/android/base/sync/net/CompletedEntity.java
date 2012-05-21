/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HttpEntity;

/**
 * It's an entity without a body.
 * @author rnewman
 *
 */
public class CompletedEntity implements HttpEntity {

  protected HttpEntity innerEntity;
  public CompletedEntity(HttpEntity entity) {
    innerEntity = entity;
  }

  @Override
  public void consumeContent() throws IOException {
    throw new IOException("Already processed.");
  }

  @Override
  public InputStream getContent() throws IOException, IllegalStateException {
    return null;
  }

  @Override
  public Header getContentEncoding() {
    return innerEntity.getContentEncoding();
  }

  @Override
  public long getContentLength() {
    return innerEntity.getContentLength();
  }

  @Override
  public Header getContentType() {
    return innerEntity.getContentType();
  }

  @Override
  public boolean isChunked() {
    return innerEntity.isChunked();
  }

  @Override
  public boolean isRepeatable() {
    return false;
  }

  @Override
  public boolean isStreaming() {
    return false;
  }

  @Override
  public void writeTo(OutputStream arg0) throws IOException {
    throw new IOException("Already processed.");
  }

}
