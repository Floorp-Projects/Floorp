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
package ch.boye.httpclientandroidlib.impl.client.cache;

import java.io.File;

import ch.boye.httpclientandroidlib.client.cache.HttpCacheInvalidator;
import ch.boye.httpclientandroidlib.client.cache.HttpCacheStorage;
import ch.boye.httpclientandroidlib.client.cache.ResourceFactory;
import ch.boye.httpclientandroidlib.impl.client.HttpClientBuilder;
import ch.boye.httpclientandroidlib.impl.execchain.ClientExecChain;

/**
 * Builder for {@link ch.boye.httpclientandroidlib.impl.client.CloseableHttpClient}
 * instances capable of client-side caching.
 *
 * @since 4.3
 */
public class CachingHttpClientBuilder extends HttpClientBuilder {

    private ResourceFactory resourceFactory;
    private HttpCacheStorage storage;
    private File cacheDir;
    private CacheConfig cacheConfig;
    private SchedulingStrategy schedulingStrategy;
    private HttpCacheInvalidator httpCacheInvalidator;

    public static CachingHttpClientBuilder create() {
        return new CachingHttpClientBuilder();
    }

    protected CachingHttpClientBuilder() {
        super();
    }

    public final CachingHttpClientBuilder setResourceFactory(
            final ResourceFactory resourceFactory) {
        this.resourceFactory = resourceFactory;
        return this;
    }

    public final CachingHttpClientBuilder setHttpCacheStorage(
            final HttpCacheStorage storage) {
        this.storage = storage;
        return this;
    }

    public final CachingHttpClientBuilder setCacheDir(
            final File cacheDir) {
        this.cacheDir = cacheDir;
        return this;
    }

    public final CachingHttpClientBuilder setCacheConfig(
            final CacheConfig cacheConfig) {
        this.cacheConfig = cacheConfig;
        return this;
    }

    public final CachingHttpClientBuilder setSchedulingStrategy(
            final SchedulingStrategy schedulingStrategy) {
        this.schedulingStrategy = schedulingStrategy;
        return this;
    }

    public final CachingHttpClientBuilder setHttpCacheInvalidator(
            final HttpCacheInvalidator cacheInvalidator) {
        this.httpCacheInvalidator = cacheInvalidator;
        return this;
    }

    @Override
    protected ClientExecChain decorateMainExec(final ClientExecChain mainExec) {
        final CacheConfig config = this.cacheConfig != null ? this.cacheConfig : CacheConfig.DEFAULT;
        ResourceFactory resourceFactory = this.resourceFactory;
        if (resourceFactory == null) {
            if (this.cacheDir == null) {
                resourceFactory = new HeapResourceFactory();
            } else {
                resourceFactory = new FileResourceFactory(cacheDir);
            }
        }
        HttpCacheStorage storage = this.storage;
        if (storage == null) {
            if (this.cacheDir == null) {
                storage = new BasicHttpCacheStorage(config);
            } else {
                final ManagedHttpCacheStorage managedStorage = new ManagedHttpCacheStorage(config);
                addCloseable(managedStorage);
                storage = managedStorage;
            }
        }
        final AsynchronousValidator revalidator = createAsynchronousRevalidator(config);
        final CacheKeyGenerator uriExtractor = new CacheKeyGenerator();

        HttpCacheInvalidator cacheInvalidator = this.httpCacheInvalidator;
        if (cacheInvalidator == null) {
            cacheInvalidator = new CacheInvalidator(uriExtractor, storage);
        }

        return new CachingExec(mainExec,
                new BasicHttpCache(
                        resourceFactory,
                        storage, config,
                        uriExtractor,
                        cacheInvalidator), config, revalidator);
    }

    private AsynchronousValidator createAsynchronousRevalidator(final CacheConfig config) {
        if (config.getAsynchronousWorkersMax() > 0) {
            final SchedulingStrategy configuredSchedulingStrategy = createSchedulingStrategy(config);
            final AsynchronousValidator revalidator = new AsynchronousValidator(
                    configuredSchedulingStrategy);
            addCloseable(revalidator);
            return revalidator;
        }
        return null;
    }

    @SuppressWarnings("resource")
    private SchedulingStrategy createSchedulingStrategy(final CacheConfig config) {
        return schedulingStrategy != null ? schedulingStrategy : new ImmediateSchedulingStrategy(config);
    }

}
