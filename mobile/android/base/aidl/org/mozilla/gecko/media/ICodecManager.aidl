/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

// Non-default types used in interface.
import org.mozilla.gecko.media.ICodec;

interface ICodecManager {
    /** Creates a remote ICodec object. */
    ICodec createCodec();
}
