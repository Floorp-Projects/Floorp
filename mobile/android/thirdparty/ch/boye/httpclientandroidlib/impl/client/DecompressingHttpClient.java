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
package ch.boye.httpclientandroidlib.impl.client;

import java.io.IOException;
import java.net.URI;

import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpEntityEnclosingRequest;
import ch.boye.httpclientandroidlib.HttpException;
import ch.boye.httpclientandroidlib.HttpHost;
import ch.boye.httpclientandroidlib.HttpRequest;
import ch.boye.httpclientandroidlib.HttpRequestInterceptor;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.HttpResponseInterceptor;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.client.HttpClient;
import ch.boye.httpclientandroidlib.client.ResponseHandler;
import ch.boye.httpclientandroidlib.client.methods.HttpUriRequest;
import ch.boye.httpclientandroidlib.client.protocol.RequestAcceptEncoding;
import ch.boye.httpclientandroidlib.client.protocol.ResponseContentEncoding;
import ch.boye.httpclientandroidlib.client.utils.URIUtils;
import ch.boye.httpclientandroidlib.conn.ClientConnectionManager;
import ch.boye.httpclientandroidlib.params.HttpParams;
import ch.boye.httpclientandroidlib.protocol.BasicHttpContext;
import ch.boye.httpclientandroidlib.protocol.HttpContext;
import ch.boye.httpclientandroidlib.util.EntityUtils;

/**
 * <p>Decorator adding support for compressed responses. This class sets
 * the <code>Accept-Encoding</code> header on requests to indicate
 * support for the <code>gzip</code> and <code>deflate</code>
 * compression schemes; it then checks the <code>Content-Encoding</code>
 * header on the response to uncompress any compressed response bodies.
 * The {@link java.io.InputStream} of the entity will contain the uncompressed
 * content.</p>
 *
 * <p><b>N.B.</b> Any upstream clients of this class need to be aware that
 * this effectively obscures visibility into the length of a server
 * response body, since the <code>Content-Length</code> header will
 * correspond to the compressed entity length received from the server,
 * but the content length experienced by reading the response body may
 * be different (hopefully higher!).</p>
 *
 * <p>That said, this decorator is compatible with the
 * <code>CachingHttpClient</code> in that the two decorators can be added
 * in either order and still have cacheable responses be cached.</p>
 *
 * @since 4.2
 *
 * @deprecated (4.3) use {@link HttpClientBuilder}
 */
@Deprecated
public class DecompressingHttpClient implements HttpClient {

    private final HttpClient backend;
    private final HttpRequestInterceptor acceptEncodingInterceptor;
    private final HttpResponseInterceptor contentEncodingInterceptor;

    /**
     * Constructs a decorator to ask for and handle compressed
     * entities on the fly.
     */
    public DecompressingHttpClient() {
        this(new DefaultHttpClient());
    }

    /**
     * Constructs a decorator to ask for and handle compressed
     * entities on the fly.
     * @param backend the {@link HttpClient} to use for actually
     *   issuing requests
     */
    public DecompressingHttpClient(final HttpClient backend) {
        this(backend, new RequestAcceptEncoding(), new ResponseContentEncoding());
    }

    DecompressingHttpClient(final HttpClient backend,
            final HttpRequestInterceptor requestInterceptor,
            final HttpResponseInterceptor responseInterceptor) {
        this.backend = backend;
        this.acceptEncodingInterceptor = requestInterceptor;
        this.contentEncodingInterceptor = responseInterceptor;
    }

    public HttpParams getParams() {
        return backend.getParams();
    }

    public ClientConnectionManager getConnectionManager() {
        return backend.getConnectionManager();
    }

    public HttpResponse execute(final HttpUriRequest request) throws IOException,
            ClientProtocolException {
        return execute(getHttpHost(request), request, (HttpContext)null);
    }

    /**
     * Gets the HttpClient to issue request.
     *
     * @return the HttpClient to issue request
     */
    public HttpClient getHttpClient() {
        return this.backend;
    }

    HttpHost getHttpHost(final HttpUriRequest request) {
        final URI uri = request.getURI();
        return URIUtils.extractHost(uri);
    }

    public HttpResponse execute(final HttpUriRequest request, final HttpContext context)
            throws IOException, ClientProtocolException {
        return execute(getHttpHost(request), request, context);
    }

    public HttpResponse execute(final HttpHost target, final HttpRequest request)
            throws IOException, ClientProtocolException {
        return execute(target, request, (HttpContext)null);
    }

    public HttpResponse execute(final HttpHost target, final HttpRequest request,
            final HttpContext context) throws IOException, ClientProtocolException {
        try {
            final HttpContext localContext = context != null ? context : new BasicHttpContext();
            final HttpRequest wrapped;
            if (request instanceof HttpEntityEnclosingRequest) {
                wrapped = new EntityEnclosingRequestWrapper((HttpEntityEnclosingRequest) request);
            } else {
                wrapped = new RequestWrapper(request);
            }
            acceptEncodingInterceptor.process(wrapped, localContext);
            final HttpResponse response = backend.execute(target, wrapped, localContext);
            try {
                contentEncodingInterceptor.process(response, localContext);
                if (Boolean.TRUE.equals(localContext.getAttribute(ResponseContentEncoding.UNCOMPRESSED))) {
                    response.removeHeaders("Content-Length");
                    response.removeHeaders("Content-Encoding");
                    response.removeHeaders("Content-MD5");
                }
                return response;
            } catch (final HttpException ex) {
                EntityUtils.consume(response.getEntity());
                throw ex;
            } catch (final IOException ex) {
                EntityUtils.consume(response.getEntity());
                throw ex;
            } catch (final RuntimeException ex) {
                EntityUtils.consume(response.getEntity());
                throw ex;
            }
        } catch (final HttpException e) {
            throw new ClientProtocolException(e);
        }
    }

    public <T> T execute(final HttpUriRequest request,
            final ResponseHandler<? extends T> responseHandler) throws IOException,
            ClientProtocolException {
        return execute(getHttpHost(request), request, responseHandler);
    }

    public <T> T execute(final HttpUriRequest request,
            final ResponseHandler<? extends T> responseHandler, final HttpContext context)
            throws IOException, ClientProtocolException {
        return execute(getHttpHost(request), request, responseHandler, context);
    }

    public <T> T execute(final HttpHost target, final HttpRequest request,
            final ResponseHandler<? extends T> responseHandler) throws IOException,
            ClientProtocolException {
        return execute(target, request, responseHandler, null);
    }

    public <T> T execute(final HttpHost target, final HttpRequest request,
            final ResponseHandler<? extends T> responseHandler, final HttpContext context)
            throws IOException, ClientProtocolException {
        final HttpResponse response = execute(target, request, context);
        try {
            return responseHandler.handleResponse(response);
        } finally {
            final HttpEntity entity = response.getEntity();
            if (entity != null) {
                EntityUtils.consume(entity);
            }
        }
    }

}
