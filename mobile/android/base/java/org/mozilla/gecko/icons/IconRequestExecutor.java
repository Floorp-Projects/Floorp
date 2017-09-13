/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons;

import android.support.annotation.NonNull;

import org.mozilla.gecko.icons.loader.ContentProviderLoader;
import org.mozilla.gecko.icons.loader.DataUriLoader;
import org.mozilla.gecko.icons.loader.DiskLoader;
import org.mozilla.gecko.icons.loader.IconDownloader;
import org.mozilla.gecko.icons.loader.IconGenerator;
import org.mozilla.gecko.icons.loader.IconLoader;
import org.mozilla.gecko.icons.loader.JarLoader;
import org.mozilla.gecko.icons.loader.LegacyLoader;
import org.mozilla.gecko.icons.loader.MemoryLoader;
import org.mozilla.gecko.icons.loader.SuggestedSiteLoader;
import org.mozilla.gecko.icons.preparation.AboutPagesPreparer;
import org.mozilla.gecko.icons.preparation.AddDefaultIconUrl;
import org.mozilla.gecko.icons.preparation.FilterKnownFailureUrls;
import org.mozilla.gecko.icons.preparation.FilterMimeTypes;
import org.mozilla.gecko.icons.preparation.FilterPrivilegedUrls;
import org.mozilla.gecko.icons.preparation.LookupIconUrl;
import org.mozilla.gecko.icons.preparation.Preparer;
import org.mozilla.gecko.icons.preparation.SuggestedSitePreparer;
import org.mozilla.gecko.icons.processing.ColorProcessor;
import org.mozilla.gecko.icons.processing.DiskProcessor;
import org.mozilla.gecko.icons.processing.MemoryProcessor;
import org.mozilla.gecko.icons.processing.Processor;
import org.mozilla.gecko.icons.processing.ResizingProcessor;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Future;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

/**
 * Executor for icon requests.
 */
/* package-private */ class IconRequestExecutor {
    /**
     * Loader implementation that generates an icon if none could be loaded.
     */
    private static final IconLoader GENERATOR = new IconGenerator();

    /**
     * Ordered list of prepares that run before any icon is loaded.
     */
    private static final List<Preparer> PREPARERS = Arrays.asList(
            // First we look into our memory and disk caches if there are some known icon URLs for
            // the page URL of the request.
            new LookupIconUrl(),

            // For all icons with MIME type we filter entries with unknown MIME type that we probably
            // cannot decode anyways.
            new FilterMimeTypes(),

            // If this is not a request that is allowed to load icons from privileged locations (omni.jar)
            // then filter such icon URLs.
            new FilterPrivilegedUrls(),

            // This preparer adds an icon URL for about pages. It's added after the filter for privileged
            // URLs. We always want to be able to load those specific icons.
            new AboutPagesPreparer(),

            // Suggested sites have icons bundled in the app - we should use them until the user has
            // visited a specified page (after which the standard icon lookup will generally provide
            // an update icon.
            new SuggestedSitePreparer(),

            // Add the default favicon URL (*/favicon.ico) to the list of icon URLs; with a low priority,
            // this icon URL should be tried last.
            new AddDefaultIconUrl(),

            // Finally we filter all URLs that failed to load recently (4xx / 5xx errors).
            new FilterKnownFailureUrls()
    );

    /**
     * Ordered list of loaders. If a loader returns a response object then subsequent loaders are not run.
     */
    private static final List<IconLoader> LOADERS = Arrays.asList(
            // First we try to load an icon that is already in the memory. That's cheap.
            new MemoryLoader(),

            // Try to decode the icon if it is a data: URI.
            new DataUriLoader(),

            // Try to load the icon from the omni.ha if it's a jar:jar URI.
            new JarLoader(),

            // Try to load the icon from a content provider (if applicable).
            new ContentProviderLoader(),

            // Try to load the icon from the disk cache.
            new DiskLoader(),

            // Try to load from the suggested site tile builder
            new SuggestedSiteLoader(),

            // If the icon is not in any of our cashes and can't be decoded then look into the
            // database (legacy). Maybe this icon was loaded before the new code was deployed.
            new LegacyLoader(),

            // Download the icon from the web.
            new IconDownloader()
    );

    /**
     * Ordered list of processors that run after an icon has been loaded.
     */
    private static final List<Processor> PROCESSORS = Arrays.asList(
            // Store the icon (and mapping) in the disk cache if needed
            new DiskProcessor(),

            // Resize the icon to match the target size (if possible)
            new ResizingProcessor(),

            // Extract the dominant color from the icon
            new ColorProcessor(),

            // Store the icon in the memory cache
            new MemoryProcessor()
    );

    private static final ExecutorService EXECUTOR;
    static {
        final ThreadFactory factory = new ThreadFactory() {
            @Override
            public Thread newThread(@NonNull Runnable runnable) {
                Thread thread = new Thread(runnable, "GeckoIconTask");
                thread.setDaemon(false);
                thread.setPriority(Thread.NORM_PRIORITY);
                return thread;
            }
        };

        // Single thread executor
        EXECUTOR = new ThreadPoolExecutor(
                1, /* corePoolSize */
                1, /* maximumPoolSize */
                0L, /* keepAliveTime */
                TimeUnit.MILLISECONDS,
                new LinkedBlockingQueue<Runnable>(),
                factory);
    }

    /**
     * Submit the request for execution.
     */
    /* package-private */ static Future<IconResponse> submit(IconRequest request) {
        return EXECUTOR.submit(
                new IconTask(request, PREPARERS, LOADERS, PROCESSORS, GENERATOR)
        );
    }
}
