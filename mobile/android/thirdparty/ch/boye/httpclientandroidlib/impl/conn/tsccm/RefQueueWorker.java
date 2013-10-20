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

package ch.boye.httpclientandroidlib.impl.conn.tsccm;

import java.lang.ref.Reference;
import java.lang.ref.ReferenceQueue;

/**
 * A worker thread for processing queued references.
 * {@link Reference Reference}s can be
 * {@link ReferenceQueue queued}
 * automatically by the garbage collector.
 * If that feature is used, a daemon thread should be executing
 * this worker. It will pick up the queued references and pass them
 * on to a handler for appropriate processing.
 *
 * @deprecated do not use
 */
@Deprecated
public class RefQueueWorker implements Runnable {

    /** The reference queue to monitor. */
    protected final ReferenceQueue<?> refQueue;

    /** The handler for the references found. */
    protected final RefQueueHandler refHandler;


    /**
     * The thread executing this handler.
     * This attribute is also used as a shutdown indicator.
     */
    protected volatile Thread workerThread;


    /**
     * Instantiates a new worker to listen for lost connections.
     *
     * @param queue     the queue on which to wait for references
     * @param handler   the handler to pass the references to
     */
    public RefQueueWorker(ReferenceQueue<?> queue, RefQueueHandler handler) {
        if (queue == null) {
            throw new IllegalArgumentException("Queue must not be null.");
        }
        if (handler == null) {
            throw new IllegalArgumentException("Handler must not be null.");
        }

        refQueue   = queue;
        refHandler = handler;
    }


    /**
     * The main loop of this worker.
     * If initialization succeeds, this method will only return
     * after {@link #shutdown shutdown()}. Only one thread can
     * execute the main loop at any time.
     */
    public void run() {

        if (this.workerThread == null) {
            this.workerThread = Thread.currentThread();
        }

        while (this.workerThread == Thread.currentThread()) {
            try {
                // remove the next reference and process it
                Reference<?> ref = refQueue.remove();
                refHandler.handleReference(ref);
            } catch (InterruptedException ignore) {
            }
        }
    }


    /**
     * Shuts down this worker.
     * It can be re-started afterwards by another call to {@link #run run()}.
     */
    public void shutdown() {
        Thread wt = this.workerThread;
        if (wt != null) {
            this.workerThread = null; // indicate shutdown
            wt.interrupt();
        }
    }


    /**
     * Obtains a description of this worker.
     *
     * @return  a descriptive string for this worker
     */
    @Override
    public String toString() {
        return "RefQueueWorker::" + this.workerThread;
    }

} // class RefQueueWorker

