/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons;

import android.graphics.Bitmap;
import android.support.annotation.NonNull;
import android.util.Log;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.icons.loader.IconLoader;
import org.mozilla.gecko.icons.preparation.Preparer;
import org.mozilla.gecko.icons.processing.Processor;
import org.mozilla.gecko.util.ThreadUtils;

import java.util.List;
import java.util.concurrent.Callable;

/**
 * Task that will be run by the IconRequestExecutor for every icon request.
 */
/* package-private */ class IconTask implements Callable<IconResponse> {
    private static final String LOGTAG = "Gecko/IconTask";
    private static final boolean DEBUG = false;

    private final List<Preparer> preparers;
    private final List<IconLoader> loaders;
    private final List<Processor> processors;
    private final IconLoader generator;
    private final IconRequest request;

    /* package-private */ IconTask(
            @NonNull IconRequest request,
            @NonNull List<Preparer> preparers,
            @NonNull List<IconLoader> loaders,
            @NonNull List<Processor> processors,
            @NonNull IconLoader generator) {
        this.request = request;
        this.preparers = preparers;
        this.loaders = loaders;
        this.processors = processors;
        this.generator = generator;
    }

    @Override
    public IconResponse call() {
        try {
            logRequest(request);

            prepareRequest(request);

            if (request.shouldPrepareOnly()) {
                // This request should only be prepared but not load an actual icon.
                return null;
            }

            final IconResponse response = loadIcon(request);

            if (response != null) {
                processIcon(request, response);
                executeCallback(request, response);

                logResponse(response);

                return response;
            }
        } catch (InterruptedException e) {
            Log.d(LOGTAG, "IconTask was interrupted", e);

            // Clear interrupt thread.
            Thread.interrupted();
        } catch (Throwable e) {
            handleException(e);
        }

        return null;
    }

    /**
     * Check if this thread was interrupted (e.g. this task was cancelled). Throws an InterruptedException
     * to stop executing the task in this case.
     */
    private void ensureNotInterrupted() throws InterruptedException {
        if (Thread.currentThread().isInterrupted()) {
            throw new InterruptedException("Task has been cancelled");
        }
    }

    private void executeCallback(IconRequest request, final IconResponse response) {
        final IconCallback callback = request.getCallback();

        if (callback != null) {
            if (request.shouldRunOnBackgroundThread()) {
                ThreadUtils.postToBackgroundThread(new Runnable() {
                    @Override
                    public void run() {
                        callback.onIconResponse(response);
                    }
                });
            } else {
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        callback.onIconResponse(response);
                    }
                });
            }
        }
    }

    private void prepareRequest(IconRequest request) throws InterruptedException {
        for (Preparer preparer : preparers) {
            ensureNotInterrupted();

            preparer.prepare(request);

            logPreparer(request, preparer);
        }
    }

    private IconResponse loadIcon(IconRequest request) throws InterruptedException {
        while (request.hasIconDescriptors()) {
            for (IconLoader loader : loaders) {
                ensureNotInterrupted();

                IconResponse response = loader.load(request);

                logLoader(request, loader, response);

                if (response != null) {
                    return response;
                }
            }

            request.moveToNextIcon();
        }

        return generator.load(request);
    }

    private void processIcon(IconRequest request, IconResponse response) throws InterruptedException {
        for (Processor processor : processors) {
            ensureNotInterrupted();

            processor.process(request, response);

            logProcessor(processor);
        }
    }

    private void handleException(final Throwable t) {
        if (AppConstants.NIGHTLY_BUILD) {
            // We want to be aware of problems: Let's re-throw the exception on the main thread to
            // force an app crash. However we only do this in Nightly builds. Every other build
            // (especially release builds) should just carry on and log the error.
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    throw new RuntimeException("Icon task thread crashed", t);
                }
            });
        } else {
            Log.e(LOGTAG, "Icon task crashed", t);
        }
    }

    private boolean shouldLog() {
        // Do not log anything if debugging is disabled and never log anything in a non-nightly build.
        return DEBUG && AppConstants.NIGHTLY_BUILD;
    }

    private void logPreparer(IconRequest request, Preparer preparer) {
        if (!shouldLog()) {
            return;
        }

        Log.d(LOGTAG, String.format("  PREPARE %s" + " (%s)",
                preparer.getClass().getSimpleName(),
                request.getIconCount()));
    }

    private void logLoader(IconRequest request, IconLoader loader, IconResponse response) {
        if (!shouldLog()) {
            return;
        }

        Log.d(LOGTAG, String.format("  LOAD [%s] %s : %s",
                response != null ? "X" : " ",
                loader.getClass().getSimpleName(),
                request.getBestIcon().getUrl()));
    }

    private void logProcessor(Processor processor) {
        if (!shouldLog()) {
            return;
        }

        Log.d(LOGTAG, "  PROCESS " + processor.getClass().getSimpleName());
    }

    private void logResponse(IconResponse response) {
        if (!shouldLog()) {
            return;
        }

        final Bitmap bitmap = response.getBitmap();

        Log.d(LOGTAG, String.format("=> ICON: %sx%s", bitmap.getWidth(), bitmap.getHeight()));
    }

    private void logRequest(IconRequest request) {
        if (!shouldLog()) {
            return;
        }

        Log.d(LOGTAG, String.format("REQUEST (%s) %s",
                request.getIconCount(),
                request.getPageUrl()));
    }
}
