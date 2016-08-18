/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

// Non-default types used in interface.
import org.mozilla.gecko.media.FormatParam;
import org.mozilla.gecko.media.Sample;

interface ICodecCallbacks {
    oneway void onInputExhausted();
    oneway void onOutputFormatChanged(in FormatParam format);
    oneway void onOutput(in Sample sample);
    oneway void onError(boolean fatal);
}