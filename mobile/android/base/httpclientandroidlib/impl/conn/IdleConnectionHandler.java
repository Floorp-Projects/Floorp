/*
 * ====================================================================
 *
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 */
package ch.boye.httpclientandroidlib.impl.conn;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;
import java.util.concurrent.TimeUnit;

import ch.boye.httpclientandroidlib.androidextra.HttpClientAndroidLog;
/* LogFactory removed by HttpClient for Android script. */
import ch.boye.httpclientandroidlib.HttpConnection;

// Currently only used by AbstractConnPool
/**
 * A helper class for connection managers to track idle connections.
 *
 * <p>This class is not synchronized.</p>
 *
 * @see ch.boye.httpclientandroidlib.conn.ClientConnectionManager#closeIdleConnections
 *
 * @since 4.0
 *
 * @deprecated no longer used
 */
@Deprecated
public class IdleConnectionHandler {

    public HttpClientAndroidLog log = new HttpClientAndroidLog(getClass());

    /** Holds connections and the time they were added. */
    private final Map<HttpConnection,TimeValues> connectionToTimes;


    public IdleConnectionHandler() {
        super();
        connectionToTimes = new HashMap<HttpConnection,TimeValues>();
    }

    /**
     * Registers the given connection with this handler.  The connection will be held until
     * {@link #remove} or {@link #closeIdleConnections} is called.
     *
     * @param connection the connection to add
     *
     * @see #remove
     */
    public void add(HttpConnection connection, long validDuration, TimeUnit unit) {

        long timeAdded = System.currentTimeMillis();

        if (log.isDebugEnabled()) {
            log.debug("Adding connection at: " + timeAdded);
        }

        connectionToTimes.put(connection, new TimeValues(timeAdded, validDuration, unit));
    }

    /**
     * Removes the given connection from the list of connections to be closed when idle.
     * This will return true if the connection is still valid, and false
     * if the connection should be considered expired and not used.
     *
     * @param connection
     * @return True if the connection is still valid.
     */
    public boolean remove(HttpConnection connection) {
        TimeValues times = connectionToTimes.remove(connection);
        if(times == null) {
            log.warn("Removing a connection that never existed!");
            return true;
        } else {
            return System.currentTimeMillis() <= times.timeExpires;
        }
    }

    /**
     * Removes all connections referenced by this handler.
     */
    public void removeAll() {
        this.connectionToTimes.clear();
    }

    /**
     * Closes connections that have been idle for at least the given amount of time.
     *
     * @param idleTime the minimum idle time, in milliseconds, for connections to be closed
     */
    public void closeIdleConnections(long idleTime) {

        // the latest time for which connections will be closed
        long idleTimeout = System.currentTimeMillis() - idleTime;

        if (log.isDebugEnabled()) {
            log.debug("Checking for connections, idle timeout: "  + idleTimeout);
        }

        for (Entry<HttpConnection, TimeValues> entry : connectionToTimes.entrySet()) {
            HttpConnection conn = entry.getKey();
            TimeValues times = entry.getValue();
            long connectionTime = times.timeAdded;
            if (connectionTime <= idleTimeout) {
                if (log.isDebugEnabled()) {
                    log.debug("Closing idle connection, connection time: "  + connectionTime);
                }
                try {
                    conn.close();
                } catch (IOException ex) {
                    log.debug("I/O error closing connection", ex);
                }
            }
        }
    }


    public void closeExpiredConnections() {
        long now = System.currentTimeMillis();
        if (log.isDebugEnabled()) {
            log.debug("Checking for expired connections, now: "  + now);
        }

        for (Entry<HttpConnection, TimeValues> entry : connectionToTimes.entrySet()) {
            HttpConnection conn = entry.getKey();
            TimeValues times = entry.getValue();
            if(times.timeExpires <= now) {
                if (log.isDebugEnabled()) {
                    log.debug("Closing connection, expired @: "  + times.timeExpires);
                }
                try {
                    conn.close();
                } catch (IOException ex) {
                    log.debug("I/O error closing connection", ex);
                }
            }
        }
    }

    private static class TimeValues {
        private final long timeAdded;
        private final long timeExpires;

        /**
         * @param now The current time in milliseconds
         * @param validDuration The duration this connection is valid for
         * @param validUnit The unit of time the duration is specified in.
         */
        TimeValues(long now, long validDuration, TimeUnit validUnit) {
            this.timeAdded = now;
            if(validDuration > 0) {
                this.timeExpires = now + validUnit.toMillis(validDuration);
            } else {
                this.timeExpires = Long.MAX_VALUE;
            }
        }
    }
}
