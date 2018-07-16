package com.leanplum.callbacks;

/**
 * Callback that gets run when forceContentUpdate was called.
 *
 * @author Anna Orlova
 */
public abstract class InboxSyncedCallback implements Runnable {
    private boolean success;

    public void setSuccess(boolean success) {
        this.success = success;
    }

    public void run() {
        this.onForceContentUpdate(success);
    }

    /**
     * Call when forceContentUpdate was called.
     *
     * @param success True if syncing was successful.
     */
    public abstract void onForceContentUpdate(boolean success);
}