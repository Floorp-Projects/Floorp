/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public final class SimpleLongOpenHashSetTest {
    @Test
    public void testEmpty() {
        SimpleLongOpenHashSet set = new SimpleLongOpenHashSet();

        assertFalse(set.contains(42));
        assertFalse(set.contains(0));
        assertFalse(set.contains(-1));
        assertTrue(set.size() == 0);
    }

    @Test
    public void testOneElement() {
        SimpleLongOpenHashSet set = new SimpleLongOpenHashSet();
        assertTrue(set.add(42));
        assertFalse(set.add(42));

        assertTrue(set.contains(42));
        assertFalse(set.contains(1));
        assertFalse(set.contains(0));
        assertFalse(set.contains(-1));
        assertTrue(set.size() == 1);
    }

    @Test
    public void testTwoElementsSameBucket() {
        SimpleLongOpenHashSet set = new SimpleLongOpenHashSet(4);
        assertTrue(set.add(0));
        assertTrue(set.add(4));

        assertTrue(set.contains(0));
        assertTrue(set.contains(4));
        assertFalse(set.contains(8));
        assertTrue(set.size() == 2);
    }

    @Test
    public void testManyElementsResize() {
        SimpleLongOpenHashSet set = new SimpleLongOpenHashSet(4);
        int count = 16;
        for (int i = 0; i < count; ++i) {
            assertTrue(set.add(i));
        }

        for (int i = 0; i < count; ++i) {
            assertTrue(set.contains(i));
        }
        assertTrue(set.size() == count);
    }

    @Test
    public void testConstructorEdgeCases() {
        for (int i = 0; i < 3; ++i) {
            SimpleLongOpenHashSet set = new SimpleLongOpenHashSet(i);
            assertTrue(set.add(42));
            assertFalse(set.add(42));

            assertTrue(set.contains(42));
            assertFalse(set.contains(1));
            assertFalse(set.contains(0));
            assertFalse(set.contains(-1));
            assertTrue(set.size() == 1);
        }
    }
}
