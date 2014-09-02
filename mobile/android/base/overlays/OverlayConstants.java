/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.overlays;

/**
 * Constants used by the share handler service (and clients).
 * The intent API used by the service is defined herein.
 */
public class OverlayConstants {
    /*
     * OverlayIntentHandler service intent actions.
     */

    /*
     * Causes the service to broadcast an intent containing state necessary for proper display of
     * a UI to select a target share method.
     *
     * Intent parameters:
     *
     * None.
     */
    public static final String ACTION_PREPARE_SHARE = "org.mozilla.gecko.overlays.ACTION_PREPARE_SHARE";

    /*
     * Action for sharing a page.
     *
     * Intent parameters:
     *
     * $EXTRA_URL: URL of page to share.    (required)
     * $EXTRA_SHARE_METHOD: Method(s) via which to share this url/title combination. Can be either a
     *                ShareType or a ShareType[]
     * $EXTRA_TITLE: Title of page to share (optional)
     * $EXTRA_PARAMETERS: Parcelable of extra data to pass to the ShareMethod (optional)
     */
    public static final String ACTION_SHARE = "org.mozilla.gecko.overlays.ACTION_SHARE";

    /*
     * OverlayIntentHandler service intent extra field keys.
     */

    // The URL/title of the page being shared
    public static final String EXTRA_URL = "URL";
    public static final String EXTRA_TITLE = "TITLE";

    // The optional extra Parcelable parameters for a ShareMethod.
    public static final String EXTRA_PARAMETERS = "EXTRA";

    // The extra field key used for holding the ShareMethod.Type we wish to use for an operation.
    public static final String EXTRA_SHARE_METHOD = "SHARE_METHOD";

    /*
     * ShareMethod UI event intent constants. Broadcast by ShareMethods using LocalBroadcastManager
     * when state has changed that requires an update of any currently-displayed share UI.
     */

    /*
     * Action for a ShareMethod UI event.
     *
     * Intent parameters:
     *
     * $EXTRA_SHARE_METHOD: The ShareType to which this event relates.
     * ... ShareType-specific parameters as desired... (optional)
     */
    public static final String SHARE_METHOD_UI_EVENT = "org.mozilla.gecko.overlays.ACTION_SHARE_METHOD_UI_EVENT";
}
