/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import java.util.List;

public class JavaUtil {
    /**
     * Move the item at position {@code from} to the position {@code to}, shifting the elements in
     * between.
     * Callers are responsible for synchronizing for {@code list} if needed.
     */
    public static <T> void moveInList(List<T> list, int from, int to) {
        final T movedTab = list.get(from);
        final int step = (from < to) ? 1 : -1;
        for (int i = from; i != to; i += step) {
            list.set(i, list.get(i + step));
        }
        list.set(to, movedTab);
    }
}
