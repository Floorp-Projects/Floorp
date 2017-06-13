/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.videoengine;

import org.mozilla.gecko.annotation.WebRTCJNITarget;

@WebRTCJNITarget
public class CaptureCapabilityAndroid {
    public String name;
    public int width[];
    public int height[];
    public int minMilliFPS;
    public int maxMilliFPS;
    public boolean frontFacing;
    public int orientation;
}
