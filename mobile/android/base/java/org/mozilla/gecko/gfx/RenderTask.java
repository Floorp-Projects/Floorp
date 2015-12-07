/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

/**
 * A class used to schedule a callback to occur when the next frame is drawn.
 * Subclasses must redefine the internalRun method, not the run method.
 */
public abstract class RenderTask {
    /**
     * Whether to run the task after the render, or before.
     */
    public final boolean runAfter;

    /**
     * Time when this task has first run, in ns. Useful for tasks which run for a specific duration.
     */
    private long mStartTime;

    /**
     * Whether we should initialise mStartTime on the next frame run.
     */
    private boolean mResetStartTime = true;

    /**
     * The callback to run on each frame. timeDelta is the time elapsed since
     * the last call, in nanoseconds. Returns true if it should continue
     * running, or false if it should be removed from the task queue. Returning
     * true implicitly schedules a redraw.
     *
     * This method first initializes the start time if resetStartTime has been invoked,
     * then calls internalRun.
     *
     * Note : subclasses should override internalRun.
     *
     * @param timeDelta the time between the beginning of last frame and the beginning of this frame, in ns.
     * @param currentFrameStartTime the startTime of the current frame, in ns.
     * @return true if animation should be run at the next frame, false otherwise
     * @see RenderTask#internalRun(long, long)
     */
    public final boolean run(long timeDelta, long currentFrameStartTime) {
        if (mResetStartTime) {
            mStartTime = currentFrameStartTime;
            mResetStartTime = false;
        }
        return internalRun(timeDelta, currentFrameStartTime);
    }

    /**
     * Abstract method to be overridden by subclasses.
     * @param timeDelta the time between the beginning of last frame and the beginning of this frame, in ns
     * @param currentFrameStartTime the startTime of the current frame, in ns.
     * @return true if animation should be run at the next frame, false otherwise
     */
    protected abstract boolean internalRun(long timeDelta, long currentFrameStartTime);

    public RenderTask(boolean aRunAfter) {
        runAfter = aRunAfter;
    }

    /**
     * Get the start time of this task.
     * It is the start time of the first frame this task was run on.
     * @return the start time in ns
     */
    public long getStartTime() {
        return mStartTime;
    }

    /**
     * Schedule a reset of the recorded start time next time {@link RenderTask#run(long, long)} is run.
     * @see RenderTask#getStartTime()
     */
    public void resetStartTime() {
        mResetStartTime = true;
    }
}
