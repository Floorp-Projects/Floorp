/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
package org.mozilla.gecko.overlays.ui;

/**
 * Interface for classes that wish to listen for the selection of an element from a SendTabList.
 */
public interface SendTabTargetSelectedListener {
    /**
     * Called when a row in the SendTabList is clicked.
     *
     * @param targetGUID The GUID of the ClientRecord the element represents (if any, otherwise null)
     */
    public void onSendTabTargetSelected(String targetGUID);

    /**
     * Called when the overall Send Tab item is clicked.
     *
     * This implies that the clients list was unavailable.
     */
    public void onSendTabActionSelected();
}
