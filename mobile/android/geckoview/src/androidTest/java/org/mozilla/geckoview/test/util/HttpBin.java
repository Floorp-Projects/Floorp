/*
 * Copyright 2015-2016 Bounce Storage, Inc. <info@bouncestorage.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.mozilla.geckoview.test.util;

import android.content.Context;
import android.os.StrictMode;

import android.support.annotation.NonNull;

import android.util.Log;

import java.net.URI;

import org.eclipse.jetty.server.Server;

import org.eclipse.jetty.util.log.AbstractLogger;
import org.eclipse.jetty.util.log.Logger;
import org.eclipse.jetty.util.thread.ExecutorThreadPool;

/**
 * Reimplementation of HttpBin http://httpbin.org/ suitable for offline unit
 * tests.
 */
public final class HttpBin {
    private static final String LOGTAG = "HttpBin";
    private final Server mServer;

    static {
        org.eclipse.jetty.util.log.Log.setLog(new AndroidLogger());
    }

    public HttpBin(@NonNull Context context, @NonNull URI endpoint) {
        this(endpoint, new HttpBinHandler(context));
    }

    public HttpBin(@NonNull URI endpoint, @NonNull HttpBinHandler handler) {
        StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder(
                StrictMode.getThreadPolicy())
                .permitNetwork()
                .build());

        mServer = new Server(endpoint.getPort());
        mServer.setHandler(handler);
    }

    public void start() throws Exception {
        mServer.start();
    }

    public void stop() throws Exception {
        mServer.stop();
    }

    public int getPort() {
        return mServer.getConnectors()[0].getLocalPort();
    }

    // We need this because the default Logger tries to use some things
    // that don't exist in older versions of Android
    private static class AndroidLogger extends AbstractLogger {
        private boolean mDebugEnabled;

        @Override
        protected Logger newLogger(String fullname) {
            return this;
        }

        @Override
        public String getName() {
            return null;
        }

        @Override
        public void warn(String msg, Object... args) {
            Log.w(LOGTAG, String.format(msg, args));
        }

        @Override
        public void warn(Throwable thrown) {
            Log.w(LOGTAG, thrown);
        }

        @Override
        public void warn(String msg, Throwable thrown) {
            Log.w(LOGTAG, msg, thrown);
        }

        @Override
        public void info(String msg, Object... args) {
            Log.i(LOGTAG, String.format(msg, args));
        }

        @Override
        public void info(Throwable thrown) {
            Log.i(LOGTAG, thrown.getMessage(), thrown);
        }

        @Override
        public void info(String msg, Throwable thrown) {
            Log.i(LOGTAG, msg, thrown);
        }

        @Override
        public boolean isDebugEnabled() {
            return mDebugEnabled;
        }

        @Override
        public void setDebugEnabled(boolean enabled) {
            mDebugEnabled = enabled;
        }

        @Override
        public void debug(String msg, Object... args) {
            if (mDebugEnabled) {
                Log.d(LOGTAG, String.format(msg, args));
            }
        }

        @Override
        public void debug(Throwable thrown) {
            if (mDebugEnabled) {
                Log.d(LOGTAG, thrown.getMessage(), thrown);
            }
        }

        @Override
        public void debug(String msg, Throwable thrown) {
            if (mDebugEnabled) {
                Log.d(LOGTAG, msg, thrown);
            }
        }

        @Override
        public void ignore(Throwable ignored) {
            // This is pretty spammy
            if (mDebugEnabled) {
                Log.w(LOGTAG, "Ignored Exception: ", ignored);
            }
        }
    };
}
