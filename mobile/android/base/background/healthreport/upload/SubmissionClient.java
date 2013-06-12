/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport.upload;

public interface SubmissionClient {
  public interface Delegate {
    /**
     * Called in the event of a temporary failure; we should try again soon.
     *
     * @param localTime milliseconds since the epoch.
     * @param id if known; may be null.
     * @param reason for failure.
     * @param e if there was an exception; may be null.
     */
    public void onSoftFailure(long localTime, String id, String reason, Exception e);

    /**
     * Called in the event of a failure; we should try again, but not today.
     *
     * @param localTime milliseconds since the epoch.
     * @param id if known; may be null.
     * @param reason for failure.
     * @param e if there was an exception; may be null.
     */
    public void onHardFailure(long localTime, String id, String reason, Exception e);

    /**
     * Success!
     *
     * @param localTime milliseconds since the epoch.
     * @param id is always known; not null.
     */
    public void onSuccess(long localTime, String id);
  }

  public void upload(long localTime, Delegate delegate);
  public void delete(long localTime, String id, Delegate delegate);
}
