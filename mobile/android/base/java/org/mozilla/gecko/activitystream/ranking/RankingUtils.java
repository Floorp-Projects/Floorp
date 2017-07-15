/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.ranking;

import android.database.Cursor;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.List;

/**
 * Some helper methods that make processing lists in a pipeline easier. This wouldn't be needed with
 * Java 8 streams or RxJava. But we haven't anything like that available.
 */
/* package-private */ class RankingUtils {
    /**
     * An action with one argument.
     */
    interface Action1<T> {
        void call(T t);
    }

    /**
     * An action with two arguments.
     */
    interface Action2<T1, T2> {
        void call(T1 t1, T2 t2);
    }

    /**
     * A function with one argument and one result.
     */
    interface Func1<T, R> {
        R call(T t);
    }

    /**
     * A function with two arguments and one result.
     */
    interface Func2<T1, T2, R> {
        R call(T1 t, T2 a);
    }

    /**
     * Filter a list of items. Items for which the provided function returns false are removed from
     * the list. This method will modify the list in place and not create a copy.
     */
    static <T> void filter(List<T> items, Func1<T, Boolean> filter) {
        final Iterator<T> iterator = items.iterator();

        while (iterator.hasNext()) {
            if (!filter.call(iterator.next())) {
                iterator.remove();
            }
        }
    }

    /**
     * Call an action with every item of the list.
     */
    static <T> void apply(List<T> items, Action1<T> action) {
        for (T item : items) {
            action.call(item);
        }
    }

    /**
     * Call an action with every item of the first list paired with the items of the second list.
     *
     * Example:
     *   items1 = 1, 2, 3
     *   items2 = A, B
     *
     * Action will be called with: (1, A)  (2, A)  (3, A)  (1, B)  (2, B)  (3, B)
     */
    static <T1, T2> void apply2D(List<T1> items1, List<T2> items2, Action2<T1, T2> action) {
        for (T1 candidate : items1) {
            for (T2 items : items2) {
                action.call(candidate, items);
            }
        }
    }

    /**
     * Call an action with ordered pairs of items from the list.
     *
     * Example:
     *  items = A, B, C, D
     *
     * Action will be called with: (A, B)  (B, C)  (C, D)
     */
    static <T> void applyInPairs(List<T> items, Action2<T, T> action) {
        if (items.size() < 2) {
            return;
        }

        for (int i = 1; i < items.size(); i++) {
            action.call(items.get(i - 1), items.get(i));
        }
    }

    /**
     * Reduce a list of items to a single value by calling a function that takes an item, the current
     * value and returns a "combined" value.
     *
     * Example:
     *   items = 1, 2, 3
     *   func = (a, b) => a + b
     *   initial = 0
     *
     * The method will return the sum (6) and the function will be called with (1, 0)  (2,  1)  (3,  3).
     */
    static <T, R> R reduce(Collection<T> items, Func2<T, R, R> func, R initial) {
        R result = initial;

        for (T item : items) {
            result = func.call(item, result);
        }

        return result;
    }

    /**
     * Transform a list of items into a list of different items by calling a function for every item
     * to return a new item. The new list will not contain more items than the provided limit.
     *
     * Example:
     *   items = 1, 2, 3, 5
     *   func = (a) => a + 1
     *   limit = 2
     *
     * The method will return a list containing 2 and 3.
     */
    static <T, R> List<R> mapWithLimit(List<T> items, Func1<T, R> func, int limit) {
        List<R> newItems = new ArrayList<>(items.size());

        for (int i = 0; i < items.size() && i < limit; i++) {
            newItems.add(func.call(items.get(i)));
        }

        return newItems;
    }

    /**
     * Transform a cursor of size N into a list of items of size M by calling a function for every
     * cursor position. If `null` is returned, that cursor position is skipped. Otherwise, value is
     * added to the list.
     */
    static <T> List<T> looselyMapCursor(Cursor cursor, Func1<Cursor, T> func) {
        List<T> items = new ArrayList<>(cursor.getCount());

        if (cursor.getCount() == 0) {
            return items;
        }

        cursor.moveToFirst();

        do {
            T item = func.call(cursor);
            if (item != null) {
                items.add(item);
            }
        } while (cursor.moveToNext());

        return items;
    }

    static double normalize(double value, double min, double max) {
        if (max > min) {
            if (value < min || value > max) {
                throw new IllegalArgumentException("value " + value + " not in range [" + min + "," + max + "]");
            }

            double delta = max - min;
            return (value - min) / delta;
        }

        // Assumption: If we do not have min/max values to normalizeFeatureValue then all values are the same
        // or do not exist. So we can just ignore this feature by setting the value to 0.
        return 0;
    }
}
