//#filter substitution
/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mma;

import android.app.Activity;
import android.app.Application;
import android.content.Context;


public interface MmaInterface {
    void init(Activity Activity);

    void start(Context context);

    void track(String mmaEvent);

    void track(String mmaEvent, double value);

    void stop();
}
