/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.Arrays;

/**
 * A simple open hash table, specialied for primitive long values.
 *
 * There is no support for removing individual values but clearing the entire
 * set is supported.
 *
 * The implementation uses linear probing.
 */

public final class SimpleLongOpenHashSet {
    private long[] elems = new long[0];
    private int size;
    private static final int FILL_PERCENTAGE = 70;
    /** Use a known value to indicate that an element is not present. */
    private static final long SENTINEL = ~0L;
    private static final int MINIMUM_SIZE = 16;

    public SimpleLongOpenHashSet() {
        this(MINIMUM_SIZE);
    }

    public SimpleLongOpenHashSet(int expectedSize) {
        if (expectedSize <= MINIMUM_SIZE) {
            expectedSize = MINIMUM_SIZE;
        }
        resize(expectedSize);
    }

    /** @return true if element exists in set */
    public boolean contains(long elem) {
        if (elem == SENTINEL) {
            ++elem;
        }
        int index = index(elem);
        // loop guaranteed to terminate since set has at least one SENTINEL
        while (true) {
            if (elems[index] == elem) {
                return true;
            } else if (elems[index] == -1) {
                return false;
            } else {
                ++index;
                if (index >= elems.length) {
                    index = 0;
                }
            }
        }
    }

    /**
     * Add an element to the hash set.
     *
     * @return true if element did not previously exist
     */
    public boolean add(long elem) {
        if ((size + 1) * 100 / elems.length > FILL_PERCENTAGE) {
            resize(2 * elems.length);
        }
        if (elem == SENTINEL) {
            ++elem;
        }
        int index = index(elem);
        // loop guaranteed to terminate since set has at least one SENTINEL
        while (true) {
            if (elems[index] == elem) {
                return false;
            } else if (elems[index] == -1) {
                elems[index] = elem;
                ++size;
                return true;
            } else {
                ++index;
                if (index >= elems.length) {
                    index = 0;
                }
            }
        }
    }

    /** Remove all elements from the set. */
    public void clear() {
        Arrays.fill(elems, SENTINEL);
        size = 0;
    }

    /** @return number of elements in the set. */
    public int size() {
        return size;
    }

    private void resize(int size) {
        long[] oldElems = elems;
        elems = new long[size];
        clear();
        for (long elem : oldElems) {
            add(elem);
        }
    }

    /** @return valid index for element */
    private int index(long elem) {
        // Collapse upper 32-bits into lower 32-bits like Long.hashCode.
        int temp = (int) (elem ^ (elem >>> 32));
        // Remove sign bit since modulus preserves it.
        return (temp & Integer.MAX_VALUE) % elems.length;
    }
}
