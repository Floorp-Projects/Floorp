/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sqlite;

import org.mozilla.gecko.mozglue.generatorannotations.WrapEntireClassForJNI;

@WrapEntireClassForJNI
public class SQLiteBridgeException extends RuntimeException {
    static final long serialVersionUID = 1L;

    public SQLiteBridgeException() {}
    public SQLiteBridgeException(String msg) {
        super(msg);
    }
}
