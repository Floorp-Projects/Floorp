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

package ch.boye.httpclientandroidlib.client;

import java.io.IOException;

import ch.boye.httpclientandroidlib.HttpHost;
import ch.boye.httpclientandroidlib.HttpRequest;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.methods.HttpUriRequest;
import ch.boye.httpclientandroidlib.conn.ClientConnectionManager;
import ch.boye.httpclientandroidlib.params.HttpParams;
import ch.boye.httpclientandroidlib.protocol.HttpContext;

/**
 * This interface represents only the most basic contract for HTTP request
 * execution. It imposes no restrictions or particular details on the request
 * execution process and leaves the specifics of state management,
 * authentication and redirect handling up to individual implementations.
 * This should make it easier to decorate the interface with additional
 * functionality such as response content caching.
 * <p/>
 * The usual execution flow can be demonstrated by the code snippet below:
 * <PRE>
 * HttpClient httpclient = new DefaultHttpClient();
 *
 * // Prepare a request object
 * HttpGet httpget = new HttpGet("http://www.apache.org/");
 *
 * // Execute the request
 * HttpResponse response = httpclient.execute(httpget);
 *
 * // Examine the response status
 * System.out.println(response.getStatusLine());
 *
 * // Get hold of the response entity
 * HttpEntity entity = response.getEntity();
 *
 * // If the response does not enclose an entity, there is no need
 * // to worry about connection release
 * if (entity != null) {
 *     InputStream instream = entity.getContent();
 *     try {
 *
 *         BufferedReader reader = new BufferedReader(
 *                 new InputStreamReader(instream));
 *         // do something useful with the response
 *         System.out.println(reader.readLine());
 *
 *     } catch (IOException ex) {
 *
 *         // In case of an IOException the connection will be released
 *         // back to the connection manager automatically
 *         throw ex;
 *
 *     } catch (RuntimeException ex) {
 *
 *         // In case of an unexpected exception you may want to abort
 *         // the HTTP request in order to shut down the underlying
 *         // connection and release it back to the connection manager.
 *         httpget.abort();
 *         throw ex;
 *
 *     } finally {
 *
 *         // Closing the input stream will trigger connection release
 *         instream.close();
 *
 *     }
 *
 *     // When HttpClient instance is no longer needed,
 *     // shut down the connection manager to ensure
 *     // immediate deallocation of all system resources
 *     httpclient.getConnectionManager().shutdown();
 * }
 * </PRE>
 *
 * @since 4.0
 */
public interface HttpClient {


    /**
     * Obtains the parameters for this client.
     * These parameters will become defaults for all requests being
     * executed with this client, and for the parameters of
     * dependent objects in this client.
     *
     * @return  the default parameters
     */
    HttpParams getParams();

    /**
     * Obtains the connection manager used by this client.
     *
     * @return  the connection manager
     */
    ClientConnectionManager getConnectionManager();

    /**
     * Executes a request using the default context.
     *
     * @param request   the request to execute
     *
     * @return  the response to the request. This is always a final response,
     *          never an intermediate response with an 1xx status code.
     *          Whether redirects or authentication challenges will be returned
     *          or handled automatically depends on the implementation and
     *          configuration of this client.
     * @throws IOException in case of a problem or the connection was aborted
     * @throws ClientProtocolException in case of an http protocol error
     */
    HttpResponse execute(HttpUriRequest request)
        throws IOException, ClientProtocolException;

    /**
     * Executes a request using the given context.
     * The route to the target will be determined by the HTTP client.
     *
     * @param request   the request to execute
     * @param context   the context to use for the execution, or
     *                  <code>null</code> to use the default context
     *
     * @return  the response to the request. This is always a final response,
     *          never an intermediate response with an 1xx status code.
     *          Whether redirects or authentication challenges will be returned
     *          or handled automatically depends on the implementation and
     *          configuration of this client.
     * @throws IOException in case of a problem or the connection was aborted
     * @throws ClientProtocolException in case of an http protocol error
     */
    HttpResponse execute(HttpUriRequest request, HttpContext context)
        throws IOException, ClientProtocolException;

    /**
     * Executes a request to the target using the default context.
     *
     * @param target    the target host for the request.
     *                  Implementations may accept <code>null</code>
     *                  if they can still determine a route, for example
     *                  to a default target or by inspecting the request.
     * @param request   the request to execute
     *
     * @return  the response to the request. This is always a final response,
     *          never an intermediate response with an 1xx status code.
     *          Whether redirects or authentication challenges will be returned
     *          or handled automatically depends on the implementation and
     *          configuration of this client.
     * @throws IOException in case of a problem or the connection was aborted
     * @throws ClientProtocolException in case of an http protocol error
     */
    HttpResponse execute(HttpHost target, HttpRequest request)
        throws IOException, ClientProtocolException;

    /**
     * Executes a request to the target using the given context.
     *
     * @param target    the target host for the request.
     *                  Implementations may accept <code>null</code>
     *                  if they can still determine a route, for example
     *                  to a default target or by inspecting the request.
     * @param request   the request to execute
     * @param context   the context to use for the execution, or
     *                  <code>null</code> to use the default context
     *
     * @return  the response to the request. This is always a final response,
     *          never an intermediate response with an 1xx status code.
     *          Whether redirects or authentication challenges will be returned
     *          or handled automatically depends on the implementation and
     *          configuration of this client.
     * @throws IOException in case of a problem or the connection was aborted
     * @throws ClientProtocolException in case of an http protocol error
     */
    HttpResponse execute(HttpHost target, HttpRequest request,
                         HttpContext context)
        throws IOException, ClientProtocolException;

    /**
     * Executes a request using the default context and processes the
     * response using the given response handler.
     *
     * @param request   the request to execute
     * @param responseHandler the response handler
     *
     * @return  the response object as generated by the response handler.
     * @throws IOException in case of a problem or the connection was aborted
     * @throws ClientProtocolException in case of an http protocol error
     */
    <T> T execute(
            HttpUriRequest request,
            ResponseHandler<? extends T> responseHandler)
        throws IOException, ClientProtocolException;

    /**
     * Executes a request using the given context and processes the
     * response using the given response handler.
     *
     * @param request   the request to execute
     * @param responseHandler the response handler
     *
     * @return  the response object as generated by the response handler.
     * @throws IOException in case of a problem or the connection was aborted
     * @throws ClientProtocolException in case of an http protocol error
     */
    <T> T execute(
            HttpUriRequest request,
            ResponseHandler<? extends T> responseHandler,
            HttpContext context)
        throws IOException, ClientProtocolException;

    /**
     * Executes a request to the target using the default context and
     * processes the response using the given response handler.
     *
     * @param target    the target host for the request.
     *                  Implementations may accept <code>null</code>
     *                  if they can still determine a route, for example
     *                  to a default target or by inspecting the request.
     * @param request   the request to execute
     * @param responseHandler the response handler
     *
     * @return  the response object as generated by the response handler.
     * @throws IOException in case of a problem or the connection was aborted
     * @throws ClientProtocolException in case of an http protocol error
     */
    <T> T execute(
            HttpHost target,
            HttpRequest request,
            ResponseHandler<? extends T> responseHandler)
        throws IOException, ClientProtocolException;

    /**
     * Executes a request to the target using the given context and
     * processes the response using the given response handler.
     *
     * @param target    the target host for the request.
     *                  Implementations may accept <code>null</code>
     *                  if they can still determine a route, for example
     *                  to a default target or by inspecting the request.
     * @param request   the request to execute
     * @param responseHandler the response handler
     * @param context   the context to use for the execution, or
     *                  <code>null</code> to use the default context
     *
     * @return  the response object as generated by the response handler.
     * @throws IOException in case of a problem or the connection was aborted
     * @throws ClientProtocolException in case of an http protocol error
     */
    <T> T execute(
            HttpHost target,
            HttpRequest request,
            ResponseHandler<? extends T> responseHandler,
            HttpContext context)
        throws IOException, ClientProtocolException;

}
