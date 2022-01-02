/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import androidx.annotation.AnyThread;
import androidx.annotation.NonNull;
import org.mozilla.gecko.annotation.WrapForJNI;

public interface WebNotificationDelegate {
  /**
   * This is called when a new notification is created.
   *
   * @param notification The WebNotification received.
   */
  @AnyThread
  @WrapForJNI
  default void onShowNotification(@NonNull final WebNotification notification) {}

  /**
   * This is called when an existing notification is closed.
   *
   * @param notification The WebNotification received.
   */
  @AnyThread
  @WrapForJNI
  default void onCloseNotification(@NonNull final WebNotification notification) {}
}
