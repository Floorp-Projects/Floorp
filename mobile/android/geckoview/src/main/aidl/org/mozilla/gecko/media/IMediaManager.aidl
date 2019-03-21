/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

// Non-default types used in interface.
import org.mozilla.gecko.media.ICodec;
import org.mozilla.gecko.media.IMediaDrmBridge;

interface IMediaManager {
    /** Creates a remote ICodec object. */
    ICodec createCodec();

    /** Creates a remote IMediaDrmBridge object. */
    IMediaDrmBridge createRemoteMediaDrmBridge(in String keySystem,
                                               in String stubId);

    /** Called by client to indicate it no longer needs a requested codec or DRM bridge. */
    oneway void endRequest();
}
