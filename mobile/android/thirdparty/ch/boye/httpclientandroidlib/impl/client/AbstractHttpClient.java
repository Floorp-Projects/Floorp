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
import java.lang.reflect.UndeclaredThrowableException;

import ch.boye.httpclientandroidlib.androidextra.HttpClientAndroidLog;
/* LogFactory removed by HttpClient for Android script. */
import ch.boye.httpclientandroidlib.ConnectionReuseStrategy;
import ch.boye.httpclientandroidlib.HttpException;
import ch.boye.httpclientandroidlib.HttpHost;
import ch.boye.httpclientandroidlib.HttpRequest;
import ch.boye.httpclientandroidlib.HttpRequestInterceptor;
import ch.boye.httpclientandroidlib.HttpResponseInterceptor;
import ch.boye.httpclientandroidlib.annotation.GuardedBy;
import ch.boye.httpclientandroidlib.annotation.ThreadSafe;
import ch.boye.httpclientandroidlib.auth.AuthSchemeRegistry;
import ch.boye.httpclientandroidlib.client.AuthenticationHandler;
import ch.boye.httpclientandroidlib.client.AuthenticationStrategy;
import ch.boye.httpclientandroidlib.client.BackoffManager;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.client.ConnectionBackoffStrategy;
import ch.boye.httpclientandroidlib.client.CookieStore;
import ch.boye.httpclientandroidlib.client.CredentialsProvider;
import ch.boye.httpclientandroidlib.client.HttpRequestRetryHandler;
import ch.boye.httpclientandroidlib.client.RedirectHandler;
import ch.boye.httpclientandroidlib.client.RedirectStrategy;
import ch.boye.httpclientandroidlib.client.RequestDirector;
import ch.boye.httpclientandroidlib.client.UserTokenHandler;
import ch.boye.httpclientandroidlib.client.config.RequestConfig;
import ch.boye.httpclientandroidlib.client.methods.CloseableHttpResponse;
import ch.boye.httpclientandroidlib.client.params.AuthPolicy;
import ch.boye.httpclientandroidlib.client.params.ClientPNames;
import ch.boye.httpclientandroidlib.client.params.CookiePolicy;
import ch.boye.httpclientandroidlib.client.params.HttpClientParamConfig;
import ch.boye.httpclientandroidlib.client.protocol.ClientContext;
import ch.boye.httpclientandroidlib.conn.ClientConnectionManager;
import ch.boye.httpclientandroidlib.conn.ClientConnectionManagerFactory;
import ch.boye.httpclientandroidlib.conn.ConnectionKeepAliveStrategy;
import ch.boye.httpclientandroidlib.conn.routing.HttpRoute;
import ch.boye.httpclientandroidlib.conn.routing.HttpRoutePlanner;
import ch.boye.httpclientandroidlib.conn.scheme.SchemeRegistry;
import ch.boye.httpclientandroidlib.cookie.CookieSpecRegistry;
import ch.boye.httpclientandroidlib.impl.DefaultConnectionReuseStrategy;
import ch.boye.httpclientandroidlib.impl.auth.BasicSchemeFactory;
import ch.boye.httpclientandroidlib.impl.auth.DigestSchemeFactory;
/* KerberosSchemeFactory removed by HttpClient for Android script. */
import ch.boye.httpclientandroidlib.impl.auth.NTLMSchemeFactory;
/* SPNegoSchemeFactory removed by HttpClient for Android script. */
import ch.boye.httpclientandroidlib.impl.conn.BasicClientConnectionManager;
import ch.boye.httpclientandroidlib.impl.conn.DefaultHttpRoutePlanner;
import ch.boye.httpclientandroidlib.impl.conn.SchemeRegistryFactory;
import ch.boye.httpclientandroidlib.impl.cookie.BestMatchSpecFactory;
import ch.boye.httpclientandroidlib.impl.cookie.BrowserCompatSpecFactory;
import ch.boye.httpclientandroidlib.impl.cookie.IgnoreSpecFactory;
import ch.boye.httpclientandroidlib.impl.cookie.NetscapeDraftSpecFactory;
import ch.boye.httpclientandroidlib.impl.cookie.RFC2109SpecFactory;
import ch.boye.httpclientandroidlib.impl.cookie.RFC2965SpecFactory;
import ch.boye.httpclientandroidlib.params.HttpParams;
import ch.boye.httpclientandroidlib.protocol.BasicHttpContext;
import ch.boye.httpclientandroidlib.protocol.BasicHttpProcessor;
import ch.boye.httpclientandroidlib.protocol.DefaultedHttpContext;
import ch.boye.httpclientandroidlib.protocol.HttpContext;
import ch.boye.httpclientandroidlib.protocol.HttpProcessor;
import ch.boye.httpclientandroidlib.protocol.HttpRequestExecutor;
import ch.boye.httpclientandroidlib.protocol.ImmutableHttpProcessor;
import ch.boye.httpclientandroidlib.util.Args;

/**
 * Base class for {@link ch.boye.httpclientandroidlib.client.HttpClient} implementations.
 * This class acts as a facade to a number of special purpose handler or
 * strategy implementations responsible for handling of a particular aspect
 * of the HTTP protocol such as redirect or authentication handling or
 * making decision about connection persistence and keep alive duration.
 * This enables the users to selectively replace default implementation
 * of those aspects with custom, application specific ones. This class
 * also provides factory methods to instantiate those objects:
 * <ul>
 *   <li>{@link HttpRequestExecutor}</li> object used to transmit messages
 *    over HTTP connections. The {@link #createRequestExecutor()} must be
 *    implemented by concrete super classes to instantiate this object.
 *   <li>{@link BasicHttpProcessor}</li> object to manage a list of protocol
 *    interceptors and apply cross-cutting protocol logic to all incoming
 *    and outgoing HTTP messages. The {@link #createHttpProcessor()} must be
 *    implemented by concrete super classes to instantiate this object.
 *   <li>{@link HttpRequestRetryHandler}</li> object used to decide whether
 *    or not a failed HTTP request is safe to retry automatically.
 *    The {@link #createHttpRequestRetryHandler()} must be
 *    implemented by concrete super classes to instantiate this object.
 *   <li>{@link ClientConnectionManager}</li> object used to manage
 *    persistent HTTP connections.
 *   <li>{@link ConnectionReuseStrategy}</li> object used to decide whether
 *    or not a HTTP connection can be kept alive and re-used for subsequent
 *    HTTP requests. The {@link #createConnectionReuseStrategy()} must be
 *    implemented by concrete super classes to instantiate this object.
 *   <li>{@link ConnectionKeepAliveStrategy}</li> object used to decide how
 *    long a persistent HTTP connection can be kept alive.
 *    The {@link #createConnectionKeepAliveStrategy()} must be
 *    implemented by concrete super classes to instantiate this object.
 *   <li>{@link CookieSpecRegistry}</li> object used to maintain a list of
 *    supported cookie specifications.
 *    The {@link #createCookieSpecRegistry()} must be implemented by concrete
 *    super classes to instantiate this object.
 *   <li>{@link CookieStore}</li> object used to maintain a collection of
 *    cookies. The {@link #createCookieStore()} must be implemented by
 *    concrete super classes to instantiate this object.
 *   <li>{@link AuthSchemeRegistry}</li> object used to maintain a list of
 *    supported authentication schemes.
 *    The {@link #createAuthSchemeRegistry()} must be implemented by concrete
 *    super classes to instantiate this object.
 *   <li>{@link CredentialsProvider}</li> object used to maintain
 *    a collection user credentials. The {@link #createCredentialsProvider()}
 *    must be implemented by concrete super classes to instantiate
 *    this object.
 *   <li>{@link AuthenticationStrategy}</li> object used to authenticate
 *    against the target host.
 *    The {@link #createTargetAuthenticationStrategy()} must be implemented
 *    by concrete super classes to instantiate this object.
 *   <li>{@link AuthenticationStrategy}</li> object used to authenticate
 *    against the proxy host.
 *    The {@link #createProxyAuthenticationStrategy()} must be implemented
 *    by concrete super classes to instantiate this object.
 *   <li>{@link HttpRoutePlanner}</li> object used to calculate a route
 *    for establishing a connection to the target host. The route
 *    may involve multiple intermediate hops.
 *    The {@link #createHttpRoutePlanner()} must be implemented
 *    by concrete super classes to instantiate this object.
 *   <li>{@link RedirectStrategy}</li> object used to determine if an HTTP
 *    request should be redirected to a new location in response to an HTTP
 *    response received from the target server.
 *   <li>{@link UserTokenHandler}</li> object used to determine if the
 *    execution context is user identity specific.
 *    The {@link #createUserTokenHandler()} must be implemented by
 *    concrete super classes to instantiate this object.
 * </ul>
 * <p>
 *   This class also maintains a list of protocol interceptors intended
 *   for processing outgoing requests and incoming responses and provides
 *   methods for managing those interceptors. New protocol interceptors can be
 *   introduced to the protocol processor chain or removed from it if needed.
 *   Internally protocol interceptors are stored in a simple
 *   {@link java.util.ArrayList}. They are executed in the same natural order
 *   as they are added to the list.
 * <p>
 *   AbstractHttpClient is thread safe. It is recommended that the same
 *   instance of this class is reused for multiple request executions.
 *   When an instance of DefaultHttpClient is no longer needed and is about
 *   to go out of scope the connection manager associated with it must be
 *   shut down by calling {@link ClientConnectionManager#shutdown()}!
 *
 * @since 4.0
 *
 * @deprecated (4.3) use {@link HttpClientBuilder}.
 */
@ThreadSafe
@Deprecated
public abstract class AbstractHttpClient extends CloseableHttpClient {

    public HttpClientAndroidLog log = new HttpClientAndroidLog(getClass());

    /** The parameters. */
    @GuardedBy("this")
    private HttpParams defaultParams;

    /** The request executor. */
    @GuardedBy("this")
    private HttpRequestExecutor requestExec;

    /** The connection manager. */
    @GuardedBy("this")
    private ClientConnectionManager connManager;

    /** The connection re-use strategy. */
    @GuardedBy("this")
    private ConnectionReuseStrategy reuseStrategy;

    /** The connection keep-alive strategy. */
    @GuardedBy("this")
    private ConnectionKeepAliveStrategy keepAliveStrategy;

    /** The cookie spec registry. */
    @GuardedBy("this")
    private CookieSpecRegistry supportedCookieSpecs;

    /** The authentication scheme registry. */
    @GuardedBy("this")
    private AuthSchemeRegistry supportedAuthSchemes;

    /** The HTTP protocol processor and its immutable copy. */
    @GuardedBy("this")
    private BasicHttpProcessor mutableProcessor;

    @GuardedBy("this")
    private ImmutableHttpProcessor protocolProcessor;

    /** The request retry handler. */
    @GuardedBy("this")
    private HttpRequestRetryHandler retryHandler;

    /** The redirect handler. */
    @GuardedBy("this")
    private RedirectStrategy redirectStrategy;

    /** The target authentication handler. */
    @GuardedBy("this")
    private AuthenticationStrategy targetAuthStrategy;

    /** The proxy authentication handler. */
    @GuardedBy("this")
    private AuthenticationStrategy proxyAuthStrategy;

    /** The cookie store. */
    @GuardedBy("this")
    private CookieStore cookieStore;

    /** The credentials provider. */
    @GuardedBy("this")
    private CredentialsProvider credsProvider;

    /** The route planner. */
    @GuardedBy("this")
    private HttpRoutePlanner routePlanner;

    /** The user token handler. */
    @GuardedBy("this")
    private UserTokenHandler userTokenHandler;

    /** The connection backoff strategy. */
    @GuardedBy("this")
    private ConnectionBackoffStrategy connectionBackoffStrategy;

    /** The backoff manager. */
    @GuardedBy("this")
    private BackoffManager backoffManager;

    /**
     * Creates a new HTTP client.
     *
     * @param conman    the connection manager
     * @param params    the parameters
     */
    protected AbstractHttpClient(
            final ClientConnectionManager conman,
            final HttpParams params) {
        super();
        defaultParams        = params;
        connManager          = conman;
    } // constructor


    protected abstract HttpParams createHttpParams();


    protected abstract BasicHttpProcessor createHttpProcessor();


    protected HttpContext createHttpContext() {
        final HttpContext context = new BasicHttpContext();
        context.setAttribute(
                ClientContext.SCHEME_REGISTRY,
                getConnectionManager().getSchemeRegistry());
        context.setAttribute(
                ClientContext.AUTHSCHEME_REGISTRY,
                getAuthSchemes());
        context.setAttribute(
                ClientContext.COOKIESPEC_REGISTRY,
                getCookieSpecs());
        context.setAttribute(
                ClientContext.COOKIE_STORE,
                getCookieStore());
        context.setAttribute(
                ClientContext.CREDS_PROVIDER,
                getCredentialsProvider());
        return context;
    }


    protected ClientConnectionManager createClientConnectionManager() {
        final SchemeRegistry registry = SchemeRegistryFactory.createDefault();

        ClientConnectionManager connManager = null;
        final HttpParams params = getParams();

        ClientConnectionManagerFactory factory = null;

        final String className = (String) params.getParameter(
                ClientPNames.CONNECTION_MANAGER_FACTORY_CLASS_NAME);
        if (className != null) {
            try {
                final Class<?> clazz = Class.forName(className);
                factory = (ClientConnectionManagerFactory) clazz.newInstance();
            } catch (final ClassNotFoundException ex) {
                throw new IllegalStateException("Invalid class name: " + className);
            } catch (final IllegalAccessException ex) {
                throw new IllegalAccessError(ex.getMessage());
            } catch (final InstantiationException ex) {
                throw new InstantiationError(ex.getMessage());
            }
        }
        if (factory != null) {
            connManager = factory.newInstance(params, registry);
        } else {
            connManager = new BasicClientConnectionManager(registry);
        }

        return connManager;
    }


    protected AuthSchemeRegistry createAuthSchemeRegistry() {
        final AuthSchemeRegistry registry = new AuthSchemeRegistry();
        registry.register(
                AuthPolicy.BASIC,
                new BasicSchemeFactory());
        registry.register(
                AuthPolicy.DIGEST,
                new DigestSchemeFactory());
        registry.register(
                AuthPolicy.NTLM,
                new NTLMSchemeFactory());
        /* SPNegoSchemeFactory removed by HttpClient for Android script. */
        /* KerberosSchemeFactory removed by HttpClient for Android script. */
        return registry;
    }


    protected CookieSpecRegistry createCookieSpecRegistry() {
        final CookieSpecRegistry registry = new CookieSpecRegistry();
        registry.register(
                CookiePolicy.BEST_MATCH,
                new BestMatchSpecFactory());
        registry.register(
                CookiePolicy.BROWSER_COMPATIBILITY,
                new BrowserCompatSpecFactory());
        registry.register(
                CookiePolicy.NETSCAPE,
                new NetscapeDraftSpecFactory());
        registry.register(
                CookiePolicy.RFC_2109,
                new RFC2109SpecFactory());
        registry.register(
                CookiePolicy.RFC_2965,
                new RFC2965SpecFactory());
        registry.register(
                CookiePolicy.IGNORE_COOKIES,
                new IgnoreSpecFactory());
        return registry;
    }

    protected HttpRequestExecutor createRequestExecutor() {
        return new HttpRequestExecutor();
    }

    protected ConnectionReuseStrategy createConnectionReuseStrategy() {
        return new DefaultConnectionReuseStrategy();
    }

    protected ConnectionKeepAliveStrategy createConnectionKeepAliveStrategy() {
        return new DefaultConnectionKeepAliveStrategy();
    }

    protected HttpRequestRetryHandler createHttpRequestRetryHandler() {
        return new DefaultHttpRequestRetryHandler();
    }

    /**
     * @deprecated (4.1) do not use
     */
    @Deprecated
    protected RedirectHandler createRedirectHandler() {
        return new DefaultRedirectHandler();
    }

    protected AuthenticationStrategy createTargetAuthenticationStrategy() {
        return new TargetAuthenticationStrategy();
    }

    /**
     * @deprecated (4.2) do not use
     */
    @Deprecated
    protected AuthenticationHandler createTargetAuthenticationHandler() {
        return new DefaultTargetAuthenticationHandler();
    }

    protected AuthenticationStrategy createProxyAuthenticationStrategy() {
        return new ProxyAuthenticationStrategy();
    }

    /**
     * @deprecated (4.2) do not use
     */
    @Deprecated
    protected AuthenticationHandler createProxyAuthenticationHandler() {
        return new DefaultProxyAuthenticationHandler();
    }

    protected CookieStore createCookieStore() {
        return new BasicCookieStore();
    }

    protected CredentialsProvider createCredentialsProvider() {
        return new BasicCredentialsProvider();
    }

    protected HttpRoutePlanner createHttpRoutePlanner() {
        return new DefaultHttpRoutePlanner(getConnectionManager().getSchemeRegistry());
    }

    protected UserTokenHandler createUserTokenHandler() {
        return new DefaultUserTokenHandler();
    }

    // non-javadoc, see interface HttpClient
    public synchronized final HttpParams getParams() {
        if (defaultParams == null) {
            defaultParams = createHttpParams();
        }
        return defaultParams;
    }

    /**
     * Replaces the parameters.
     * The implementation here does not update parameters of dependent objects.
     *
     * @param params    the new default parameters
     */
    public synchronized void setParams(final HttpParams params) {
        defaultParams = params;
    }


    public synchronized final ClientConnectionManager getConnectionManager() {
        if (connManager == null) {
            connManager = createClientConnectionManager();
        }
        return connManager;
    }


    public synchronized final HttpRequestExecutor getRequestExecutor() {
        if (requestExec == null) {
            requestExec = createRequestExecutor();
        }
        return requestExec;
    }


    public synchronized final AuthSchemeRegistry getAuthSchemes() {
        if (supportedAuthSchemes == null) {
            supportedAuthSchemes = createAuthSchemeRegistry();
        }
        return supportedAuthSchemes;
    }

    public synchronized void setAuthSchemes(final AuthSchemeRegistry registry) {
        supportedAuthSchemes = registry;
    }

    public synchronized final ConnectionBackoffStrategy getConnectionBackoffStrategy() {
        return connectionBackoffStrategy;
    }

    public synchronized void setConnectionBackoffStrategy(final ConnectionBackoffStrategy strategy) {
        connectionBackoffStrategy = strategy;
    }

    public synchronized final CookieSpecRegistry getCookieSpecs() {
        if (supportedCookieSpecs == null) {
            supportedCookieSpecs = createCookieSpecRegistry();
        }
        return supportedCookieSpecs;
    }

    public synchronized final BackoffManager getBackoffManager() {
        return backoffManager;
    }

    public synchronized void setBackoffManager(final BackoffManager manager) {
        backoffManager = manager;
    }

    public synchronized void setCookieSpecs(final CookieSpecRegistry registry) {
        supportedCookieSpecs = registry;
    }

    public synchronized final ConnectionReuseStrategy getConnectionReuseStrategy() {
        if (reuseStrategy == null) {
            reuseStrategy = createConnectionReuseStrategy();
        }
        return reuseStrategy;
    }


    public synchronized void setReuseStrategy(final ConnectionReuseStrategy strategy) {
        this.reuseStrategy = strategy;
    }


    public synchronized final ConnectionKeepAliveStrategy getConnectionKeepAliveStrategy() {
        if (keepAliveStrategy == null) {
            keepAliveStrategy = createConnectionKeepAliveStrategy();
        }
        return keepAliveStrategy;
    }


    public synchronized void setKeepAliveStrategy(final ConnectionKeepAliveStrategy strategy) {
        this.keepAliveStrategy = strategy;
    }


    public synchronized final HttpRequestRetryHandler getHttpRequestRetryHandler() {
        if (retryHandler == null) {
            retryHandler = createHttpRequestRetryHandler();
        }
        return retryHandler;
    }

    public synchronized void setHttpRequestRetryHandler(final HttpRequestRetryHandler handler) {
        this.retryHandler = handler;
    }

    /**
     * @deprecated (4.1) do not use
     */
    @Deprecated
    public synchronized final RedirectHandler getRedirectHandler() {
        return createRedirectHandler();
    }

    /**
     * @deprecated (4.1) do not use
     */
    @Deprecated
    public synchronized void setRedirectHandler(final RedirectHandler handler) {
        this.redirectStrategy = new DefaultRedirectStrategyAdaptor(handler);
    }

    /**
     * @since 4.1
     */
    public synchronized final RedirectStrategy getRedirectStrategy() {
        if (redirectStrategy == null) {
            redirectStrategy = new DefaultRedirectStrategy();
        }
        return redirectStrategy;
    }

    /**
     * @since 4.1
     */
    public synchronized void setRedirectStrategy(final RedirectStrategy strategy) {
        this.redirectStrategy = strategy;
    }

    /**
     * @deprecated (4.2) do not use
     */
    @Deprecated
    public synchronized final AuthenticationHandler getTargetAuthenticationHandler() {
        return createTargetAuthenticationHandler();
    }

    /**
     * @deprecated (4.2) do not use
     */
    @Deprecated
    public synchronized void setTargetAuthenticationHandler(final AuthenticationHandler handler) {
        this.targetAuthStrategy = new AuthenticationStrategyAdaptor(handler);
    }

    /**
     * @since 4.2
     */
    public synchronized final AuthenticationStrategy getTargetAuthenticationStrategy() {
        if (targetAuthStrategy == null) {
            targetAuthStrategy = createTargetAuthenticationStrategy();
        }
        return targetAuthStrategy;
    }

    /**
     * @since 4.2
     */
    public synchronized void setTargetAuthenticationStrategy(final AuthenticationStrategy strategy) {
        this.targetAuthStrategy = strategy;
    }

    /**
     * @deprecated (4.2) do not use
     */
    @Deprecated
    public synchronized final AuthenticationHandler getProxyAuthenticationHandler() {
        return createProxyAuthenticationHandler();
    }

    /**
     * @deprecated (4.2) do not use
     */
    @Deprecated
    public synchronized void setProxyAuthenticationHandler(final AuthenticationHandler handler) {
        this.proxyAuthStrategy = new AuthenticationStrategyAdaptor(handler);
    }

    /**
     * @since 4.2
     */
    public synchronized final AuthenticationStrategy getProxyAuthenticationStrategy() {
        if (proxyAuthStrategy == null) {
            proxyAuthStrategy = createProxyAuthenticationStrategy();
        }
        return proxyAuthStrategy;
    }

    /**
     * @since 4.2
     */
    public synchronized void setProxyAuthenticationStrategy(final AuthenticationStrategy strategy) {
        this.proxyAuthStrategy = strategy;
    }

    public synchronized final CookieStore getCookieStore() {
        if (cookieStore == null) {
            cookieStore = createCookieStore();
        }
        return cookieStore;
    }

    public synchronized void setCookieStore(final CookieStore cookieStore) {
        this.cookieStore = cookieStore;
    }

    public synchronized final CredentialsProvider getCredentialsProvider() {
        if (credsProvider == null) {
            credsProvider = createCredentialsProvider();
        }
        return credsProvider;
    }

    public synchronized void setCredentialsProvider(final CredentialsProvider credsProvider) {
        this.credsProvider = credsProvider;
    }

    public synchronized final HttpRoutePlanner getRoutePlanner() {
        if (this.routePlanner == null) {
            this.routePlanner = createHttpRoutePlanner();
        }
        return this.routePlanner;
    }

    public synchronized void setRoutePlanner(final HttpRoutePlanner routePlanner) {
        this.routePlanner = routePlanner;
    }

    public synchronized final UserTokenHandler getUserTokenHandler() {
        if (this.userTokenHandler == null) {
            this.userTokenHandler = createUserTokenHandler();
        }
        return this.userTokenHandler;
    }

    public synchronized void setUserTokenHandler(final UserTokenHandler handler) {
        this.userTokenHandler = handler;
    }

    protected synchronized final BasicHttpProcessor getHttpProcessor() {
        if (mutableProcessor == null) {
            mutableProcessor = createHttpProcessor();
        }
        return mutableProcessor;
    }

    private synchronized HttpProcessor getProtocolProcessor() {
        if (protocolProcessor == null) {
            // Get mutable HTTP processor
            final BasicHttpProcessor proc = getHttpProcessor();
            // and create an immutable copy of it
            final int reqc = proc.getRequestInterceptorCount();
            final HttpRequestInterceptor[] reqinterceptors = new HttpRequestInterceptor[reqc];
            for (int i = 0; i < reqc; i++) {
                reqinterceptors[i] = proc.getRequestInterceptor(i);
            }
            final int resc = proc.getResponseInterceptorCount();
            final HttpResponseInterceptor[] resinterceptors = new HttpResponseInterceptor[resc];
            for (int i = 0; i < resc; i++) {
                resinterceptors[i] = proc.getResponseInterceptor(i);
            }
            protocolProcessor = new ImmutableHttpProcessor(reqinterceptors, resinterceptors);
        }
        return protocolProcessor;
    }

    public synchronized int getResponseInterceptorCount() {
        return getHttpProcessor().getResponseInterceptorCount();
    }

    public synchronized HttpResponseInterceptor getResponseInterceptor(final int index) {
        return getHttpProcessor().getResponseInterceptor(index);
    }

    public synchronized HttpRequestInterceptor getRequestInterceptor(final int index) {
        return getHttpProcessor().getRequestInterceptor(index);
    }

    public synchronized int getRequestInterceptorCount() {
        return getHttpProcessor().getRequestInterceptorCount();
    }

    public synchronized void addResponseInterceptor(final HttpResponseInterceptor itcp) {
        getHttpProcessor().addInterceptor(itcp);
        protocolProcessor = null;
    }

    public synchronized void addResponseInterceptor(final HttpResponseInterceptor itcp, final int index) {
        getHttpProcessor().addInterceptor(itcp, index);
        protocolProcessor = null;
    }

    public synchronized void clearResponseInterceptors() {
        getHttpProcessor().clearResponseInterceptors();
        protocolProcessor = null;
    }

    public synchronized void removeResponseInterceptorByClass(final Class<? extends HttpResponseInterceptor> clazz) {
        getHttpProcessor().removeResponseInterceptorByClass(clazz);
        protocolProcessor = null;
    }

    public synchronized void addRequestInterceptor(final HttpRequestInterceptor itcp) {
        getHttpProcessor().addInterceptor(itcp);
        protocolProcessor = null;
    }

    public synchronized void addRequestInterceptor(final HttpRequestInterceptor itcp, final int index) {
        getHttpProcessor().addInterceptor(itcp, index);
        protocolProcessor = null;
    }

    public synchronized void clearRequestInterceptors() {
        getHttpProcessor().clearRequestInterceptors();
        protocolProcessor = null;
    }

    public synchronized void removeRequestInterceptorByClass(final Class<? extends HttpRequestInterceptor> clazz) {
        getHttpProcessor().removeRequestInterceptorByClass(clazz);
        protocolProcessor = null;
    }

    @Override
    protected final CloseableHttpResponse doExecute(final HttpHost target, final HttpRequest request,
                                      final HttpContext context)
        throws IOException, ClientProtocolException {

        Args.notNull(request, "HTTP request");
        // a null target may be acceptable, this depends on the route planner
        // a null context is acceptable, default context created below

        HttpContext execContext = null;
        RequestDirector director = null;
        HttpRoutePlanner routePlanner = null;
        ConnectionBackoffStrategy connectionBackoffStrategy = null;
        BackoffManager backoffManager = null;

        // Initialize the request execution context making copies of
        // all shared objects that are potentially threading unsafe.
        synchronized (this) {

            final HttpContext defaultContext = createHttpContext();
            if (context == null) {
                execContext = defaultContext;
            } else {
                execContext = new DefaultedHttpContext(context, defaultContext);
            }
            final HttpParams params = determineParams(request);
            final RequestConfig config = HttpClientParamConfig.getRequestConfig(params);
            execContext.setAttribute(ClientContext.REQUEST_CONFIG, config);

            // Create a director for this request
            director = createClientRequestDirector(
                    getRequestExecutor(),
                    getConnectionManager(),
                    getConnectionReuseStrategy(),
                    getConnectionKeepAliveStrategy(),
                    getRoutePlanner(),
                    getProtocolProcessor(),
                    getHttpRequestRetryHandler(),
                    getRedirectStrategy(),
                    getTargetAuthenticationStrategy(),
                    getProxyAuthenticationStrategy(),
                    getUserTokenHandler(),
                    params);
            routePlanner = getRoutePlanner();
            connectionBackoffStrategy = getConnectionBackoffStrategy();
            backoffManager = getBackoffManager();
        }

        try {
            if (connectionBackoffStrategy != null && backoffManager != null) {
                final HttpHost targetForRoute = (target != null) ? target
                        : (HttpHost) determineParams(request).getParameter(
                                ClientPNames.DEFAULT_HOST);
                final HttpRoute route = routePlanner.determineRoute(targetForRoute, request, execContext);

                final CloseableHttpResponse out;
                try {
                    out = CloseableHttpResponseProxy.newProxy(
                            director.execute(target, request, execContext));
                } catch (final RuntimeException re) {
                    if (connectionBackoffStrategy.shouldBackoff(re)) {
                        backoffManager.backOff(route);
                    }
                    throw re;
                } catch (final Exception e) {
                    if (connectionBackoffStrategy.shouldBackoff(e)) {
                        backoffManager.backOff(route);
                    }
                    if (e instanceof HttpException) {
                        throw (HttpException)e;
                    }
                    if (e instanceof IOException) {
                        throw (IOException)e;
                    }
                    throw new UndeclaredThrowableException(e);
                }
                if (connectionBackoffStrategy.shouldBackoff(out)) {
                    backoffManager.backOff(route);
                } else {
                    backoffManager.probe(route);
                }
                return out;
            } else {
                return CloseableHttpResponseProxy.newProxy(
                        director.execute(target, request, execContext));
            }
        } catch(final HttpException httpException) {
            throw new ClientProtocolException(httpException);
        }
    }

    /**
     * @deprecated (4.1) do not use
     */
    @Deprecated
    protected RequestDirector createClientRequestDirector(
            final HttpRequestExecutor requestExec,
            final ClientConnectionManager conman,
            final ConnectionReuseStrategy reustrat,
            final ConnectionKeepAliveStrategy kastrat,
            final HttpRoutePlanner rouplan,
            final HttpProcessor httpProcessor,
            final HttpRequestRetryHandler retryHandler,
            final RedirectHandler redirectHandler,
            final AuthenticationHandler targetAuthHandler,
            final AuthenticationHandler proxyAuthHandler,
            final UserTokenHandler userTokenHandler,
            final HttpParams params) {
        return new DefaultRequestDirector(
                requestExec,
                conman,
                reustrat,
                kastrat,
                rouplan,
                httpProcessor,
                retryHandler,
                redirectHandler,
                targetAuthHandler,
                proxyAuthHandler,
                userTokenHandler,
                params);
    }

    /**
     * @deprecated (4.2) do not use
     */
    @Deprecated
    protected RequestDirector createClientRequestDirector(
            final HttpRequestExecutor requestExec,
            final ClientConnectionManager conman,
            final ConnectionReuseStrategy reustrat,
            final ConnectionKeepAliveStrategy kastrat,
            final HttpRoutePlanner rouplan,
            final HttpProcessor httpProcessor,
            final HttpRequestRetryHandler retryHandler,
            final RedirectStrategy redirectStrategy,
            final AuthenticationHandler targetAuthHandler,
            final AuthenticationHandler proxyAuthHandler,
            final UserTokenHandler userTokenHandler,
            final HttpParams params) {
        return new DefaultRequestDirector(
                log,
                requestExec,
                conman,
                reustrat,
                kastrat,
                rouplan,
                httpProcessor,
                retryHandler,
                redirectStrategy,
                targetAuthHandler,
                proxyAuthHandler,
                userTokenHandler,
                params);
    }


    /**
     * @since 4.2
     */
    protected RequestDirector createClientRequestDirector(
            final HttpRequestExecutor requestExec,
            final ClientConnectionManager conman,
            final ConnectionReuseStrategy reustrat,
            final ConnectionKeepAliveStrategy kastrat,
            final HttpRoutePlanner rouplan,
            final HttpProcessor httpProcessor,
            final HttpRequestRetryHandler retryHandler,
            final RedirectStrategy redirectStrategy,
            final AuthenticationStrategy targetAuthStrategy,
            final AuthenticationStrategy proxyAuthStrategy,
            final UserTokenHandler userTokenHandler,
            final HttpParams params) {
        return new DefaultRequestDirector(
                log,
                requestExec,
                conman,
                reustrat,
                kastrat,
                rouplan,
                httpProcessor,
                retryHandler,
                redirectStrategy,
                targetAuthStrategy,
                proxyAuthStrategy,
                userTokenHandler,
                params);
    }

    /**
     * Obtains parameters for executing a request.
     * The default implementation in this class creates a new
     * {@link ClientParamsStack} from the request parameters
     * and the client parameters.
     * <br/>
     * This method is called by the default implementation of
     * {@link #execute(HttpHost,HttpRequest,HttpContext)}
     * to obtain the parameters for the
     * {@link DefaultRequestDirector}.
     *
     * @param req    the request that will be executed
     *
     * @return  the parameters to use
     */
    protected HttpParams determineParams(final HttpRequest req) {
        return new ClientParamsStack
            (null, getParams(), req.getParams(), null);
    }


    public void close() {
        getConnectionManager().shutdown();
    }

}
