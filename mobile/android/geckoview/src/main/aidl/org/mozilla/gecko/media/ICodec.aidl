/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

// Non-default types used in interface.
import android.os.Bundle;
import org.mozilla.gecko.gfx.GeckoSurface;
import org.mozilla.gecko.media.FormatParam;
import org.mozilla.gecko.media.ICodecCallbacks;
import org.mozilla.gecko.media.Sample;

interface ICodec {
    void setCallbacks(in ICodecCallbacks callbacks);
    boolean configure(in FormatParam format, in GeckoSurface surface, in int flags, in String drmStubId);
    boolean isAdaptivePlaybackSupported();
    void start();
    void stop();
    void flush();
    void release();

    Sample dequeueInput(int size);
    oneway void queueInput(in Sample sample);

    void releaseOutput(in Sample sample, in boolean render);
    oneway void setRates(in int newBitRate);
}
