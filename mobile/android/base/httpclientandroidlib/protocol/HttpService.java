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

package ch.boye.httpclientandroidlib.protocol;

import java.io.IOException;

import ch.boye.httpclientandroidlib.ConnectionReuseStrategy;
import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpEntityEnclosingRequest;
import ch.boye.httpclientandroidlib.HttpException;
import ch.boye.httpclientandroidlib.HttpRequest;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.HttpResponseFactory;
import ch.boye.httpclientandroidlib.HttpServerConnection;
import ch.boye.httpclientandroidlib.HttpStatus;
import ch.boye.httpclientandroidlib.HttpVersion;
import ch.boye.httpclientandroidlib.MethodNotSupportedException;
import ch.boye.httpclientandroidlib.ProtocolException;
import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.UnsupportedHttpVersionException;
import ch.boye.httpclientandroidlib.entity.ByteArrayEntity;
import ch.boye.httpclientandroidlib.params.HttpParams;
import ch.boye.httpclientandroidlib.params.DefaultedHttpParams;
import ch.boye.httpclientandroidlib.util.EncodingUtils;
import ch.boye.httpclientandroidlib.util.EntityUtils;

/**
 * HttpService is a server side HTTP protocol handler based in the blocking
 * I/O model that implements the essential requirements of the HTTP protocol
 * for the server side message processing as described by RFC 2616.
 * <br>
 * HttpService relies on {@link HttpProcessor} to generate mandatory protocol
 * headers for all outgoing messages and apply common, cross-cutting message
 * transformations to all incoming and outgoing messages, whereas individual
 * {@link HttpRequestHandler}s are expected to take care of application specific
 * content generation and processing.
 * <br>
 * HttpService relies on {@link HttpRequestHandler} to resolve matching request
 * handler for a particular request URI of an incoming HTTP request.
 * <br>
 * HttpService can use optional {@link HttpExpectationVerifier} to ensure that
 * incoming requests meet server's expectations.
 *
 * @since 4.0
 */
public class HttpService {

    /**
     * TODO: make all variables final in the next major version
     */
    private volatile HttpParams params = null;
    private volatile HttpProcessor processor = null;
    private volatile HttpRequestHandlerResolver handlerResolver = null;
    private volatile ConnectionReuseStrategy connStrategy = null;
    private volatile HttpResponseFactory responseFactory = null;
    private volatile HttpExpectationVerifier expectationVerifier = null;

    /**
     * Create a new HTTP service.
     *
     * @param processor            the processor to use on requests and responses
     * @param connStrategy         the connection reuse strategy
     * @param responseFactory      the response factory
     * @param handlerResolver      the handler resolver. May be null.
     * @param expectationVerifier  the expectation verifier. May be null.
     * @param params               the HTTP parameters
     *
     * @since 4.1
     */
    public HttpService(
            final HttpProcessor processor,
            final ConnectionReuseStrategy connStrategy,
            final HttpResponseFactory responseFactory,
            final HttpRequestHandlerResolver handlerResolver,
            final HttpExpectationVerifier expectationVerifier,
            final HttpParams params) {
        super();
        if (processor == null) {
            throw new IllegalArgumentException("HTTP processor may not be null");
        }
        if (connStrategy == null) {
            throw new IllegalArgumentException("Connection reuse strategy may not be null");
        }
        if (responseFactory == null) {
            throw new IllegalArgumentException("Response factory may not be null");
        }
        if (params == null) {
            throw new IllegalArgumentException("HTTP parameters may not be null");
        }
        this.processor = processor;
        this.connStrategy = connStrategy;
        this.responseFactory = responseFactory;
        this.handlerResolver = handlerResolver;
        this.expectationVerifier = expectationVerifier;
        this.params = params;
    }

    /**
     * Create a new HTTP service.
     *
     * @param processor            the processor to use on requests and responses
     * @param connStrategy         the connection reuse strategy
     * @param responseFactory      the response factory
     * @param handlerResolver      the handler resolver. May be null.
     * @param params               the HTTP parameters
     *
     * @since 4.1
     */
    public HttpService(
            final HttpProcessor processor,
            final ConnectionReuseStrategy connStrategy,
            final HttpResponseFactory responseFactory,
            final HttpRequestHandlerResolver handlerResolver,
            final HttpParams params) {
        this(processor, connStrategy, responseFactory, handlerResolver, null, params);
    }

    /**
     * Create a new HTTP service.
     *
     * @param proc             the processor to use on requests and responses
     * @param connStrategy     the connection reuse strategy
     * @param responseFactory  the response factory
     *
     * @deprecated use {@link HttpService#HttpService(HttpProcessor,
     *  ConnectionReuseStrategy, HttpResponseFactory, HttpRequestHandlerResolver, HttpParams)}
     */
    public HttpService(
            final HttpProcessor proc,
            final ConnectionReuseStrategy connStrategy,
            final HttpResponseFactory responseFactory) {
        super();
        setHttpProcessor(proc);
        setConnReuseStrategy(connStrategy);
        setResponseFactory(responseFactory);
    }

    /**
     * @deprecated set {@link HttpProcessor} using constructor
     */
    public void setHttpProcessor(final HttpProcessor processor) {
        if (processor == null) {
            throw new IllegalArgumentException("HTTP processor may not be null");
        }
        this.processor = processor;
    }

    /**
     * @deprecated set {@link ConnectionReuseStrategy} using constructor
     */
    public void setConnReuseStrategy(final ConnectionReuseStrategy connStrategy) {
        if (connStrategy == null) {
            throw new IllegalArgumentException("Connection reuse strategy may not be null");
        }
        this.connStrategy = connStrategy;
    }

    /**
     * @deprecated set {@link HttpResponseFactory} using constructor
     */
    public void setResponseFactory(final HttpResponseFactory responseFactory) {
        if (responseFactory == null) {
            throw new IllegalArgumentException("Response factory may not be null");
        }
        this.responseFactory = responseFactory;
    }

    /**
     * @deprecated set {@link HttpResponseFactory} using constructor
     */
    public void setParams(final HttpParams params) {
        this.params = params;
    }

    /**
     * @deprecated set {@link HttpRequestHandlerResolver} using constructor
     */
    public void setHandlerResolver(final HttpRequestHandlerResolver handlerResolver) {
        this.handlerResolver = handlerResolver;
    }

    /**
     * @deprecated set {@link HttpExpectationVerifier} using constructor
     */
    public void setExpectationVerifier(final HttpExpectationVerifier expectationVerifier) {
        this.expectationVerifier = expectationVerifier;
    }

    public HttpParams getParams() {
        return this.params;
    }

    /**
     * Handles receives one HTTP request over the given connection within the
     * given execution context and sends a response back to the client.
     *
     * @param conn the active connection to the client
     * @param context the actual execution context.
     * @throws IOException in case of an I/O error.
     * @throws HttpException in case of HTTP protocol violation or a processing
     *   problem.
     */
    public void handleRequest(
            final HttpServerConnection conn,
            final HttpContext context) throws IOException, HttpException {

        context.setAttribute(ExecutionContext.HTTP_CONNECTION, conn);

        HttpResponse response = null;

        try {

            HttpRequest request = conn.receiveRequestHeader();
            request.setParams(
                    new DefaultedHttpParams(request.getParams(), this.params));

            ProtocolVersion ver =
                request.getRequestLine().getProtocolVersion();
            if (!ver.lessEquals(HttpVersion.HTTP_1_1)) {
                // Downgrade protocol version if greater than HTTP/1.1
                ver = HttpVersion.HTTP_1_1;
            }

            if (request instanceof HttpEntityEnclosingRequest) {

                if (((HttpEntityEnclosingRequest) request).expectContinue()) {
                    response = this.responseFactory.newHttpResponse(ver,
                            HttpStatus.SC_CONTINUE, context);
                    response.setParams(
                            new DefaultedHttpParams(response.getParams(), this.params));

                    if (this.expectationVerifier != null) {
                        try {
                            this.expectationVerifier.verify(request, response, context);
                        } catch (HttpException ex) {
                            response = this.responseFactory.newHttpResponse(HttpVersion.HTTP_1_0,
                                    HttpStatus.SC_INTERNAL_SERVER_ERROR, context);
                            response.setParams(
                                    new DefaultedHttpParams(response.getParams(), this.params));
                            handleException(ex, response);
                        }
                    }
                    if (response.getStatusLine().getStatusCode() < 200) {
                        // Send 1xx response indicating the server expections
                        // have been met
                        conn.sendResponseHeader(response);
                        conn.flush();
                        response = null;
                        conn.receiveRequestEntity((HttpEntityEnclosingRequest) request);
                    }
                } else {
                    conn.receiveRequestEntity((HttpEntityEnclosingRequest) request);
                }
            }

            if (response == null) {
                response = this.responseFactory.newHttpResponse(ver, HttpStatus.SC_OK, context);
                response.setParams(
                        new DefaultedHttpParams(response.getParams(), this.params));

                context.setAttribute(ExecutionContext.HTTP_REQUEST, request);
                context.setAttribute(ExecutionContext.HTTP_RESPONSE, response);

                this.processor.process(request, context);
                doService(request, response, context);
            }

            // Make sure the request content is fully consumed
            if (request instanceof HttpEntityEnclosingRequest) {
                HttpEntity entity = ((HttpEntityEnclosingRequest)request).getEntity();
                EntityUtils.consume(entity);
            }

        } catch (HttpException ex) {
            response = this.responseFactory.newHttpResponse
                (HttpVersion.HTTP_1_0, HttpStatus.SC_INTERNAL_SERVER_ERROR,
                 context);
            response.setParams(
                    new DefaultedHttpParams(response.getParams(), this.params));
            handleException(ex, response);
        }

        this.processor.process(response, context);
        conn.sendResponseHeader(response);
        conn.sendResponseEntity(response);
        conn.flush();

        if (!this.connStrategy.keepAlive(response, context)) {
            conn.close();
        }
    }

    /**
     * Handles the given exception and generates an HTTP response to be sent
     * back to the client to inform about the exceptional condition encountered
     * in the course of the request processing.
     *
     * @param ex the exception.
     * @param response the HTTP response.
     */
    protected void handleException(final HttpException ex, final HttpResponse response) {
        if (ex instanceof MethodNotSupportedException) {
            response.setStatusCode(HttpStatus.SC_NOT_IMPLEMENTED);
        } else if (ex instanceof UnsupportedHttpVersionException) {
            response.setStatusCode(HttpStatus.SC_HTTP_VERSION_NOT_SUPPORTED);
        } else if (ex instanceof ProtocolException) {
            response.setStatusCode(HttpStatus.SC_BAD_REQUEST);
        } else {
            response.setStatusCode(HttpStatus.SC_INTERNAL_SERVER_ERROR);
        }
        byte[] msg = EncodingUtils.getAsciiBytes(ex.getMessage());
        ByteArrayEntity entity = new ByteArrayEntity(msg);
        entity.setContentType("text/plain; charset=US-ASCII");
        response.setEntity(entity);
    }

    /**
     * The default implementation of this method attempts to resolve an
     * {@link HttpRequestHandler} for the request URI of the given request
     * and, if found, executes its
     * {@link HttpRequestHandler#handle(HttpRequest, HttpResponse, HttpContext)}
     * method.
     * <p>
     * Super-classes can override this method in order to provide a custom
     * implementation of the request processing logic.
     *
     * @param request the HTTP request.
     * @param response the HTTP response.
     * @param context the execution context.
     * @throws IOException in case of an I/O error.
     * @throws HttpException in case of HTTP protocol violation or a processing
     *   problem.
     */
    protected void doService(
            final HttpRequest request,
            final HttpResponse response,
            final HttpContext context) throws HttpException, IOException {
        HttpRequestHandler handler = null;
        if (this.handlerResolver != null) {
            String requestURI = request.getRequestLine().getUri();
            handler = this.handlerResolver.lookup(requestURI);
        }
        if (handler != null) {
            handler.handle(request, response, context);
        } else {
            response.setStatusCode(HttpStatus.SC_NOT_IMPLEMENTED);
        }
    }

}
