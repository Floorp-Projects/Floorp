/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.activitystream.ranking;

import android.database.Cursor;
import android.database.MatrixCursor;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import static org.mozilla.gecko.activitystream.ranking.RankingUtils.Action1;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.Action2;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.Func1;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.Func2;

@RunWith(TestRunner.class)
public class TestRankingUtils {
    @Test
    public void testFilter() {
        final List<Integer> numbers = new ArrayList<>(Arrays.asList(5, 7, 3, 2, 1, 9, 0, 10, 4));

        final Func1<Integer, Boolean> func = new Func1<Integer, Boolean>() {
            @Override
            public Boolean call(Integer integer) {
                return integer > 3;
            }
        };

        Assert.assertEquals(9, numbers.size());

        RankingUtils.filter(numbers, func);

        Assert.assertEquals(5, numbers.size());

        Assert.assertTrue(numbers.contains(5));
        Assert.assertTrue(numbers.contains(7));
        Assert.assertTrue(numbers.contains(9));
        Assert.assertTrue(numbers.contains(10));
        Assert.assertTrue(numbers.contains(4));

        Assert.assertFalse(numbers.contains(3));
        Assert.assertFalse(numbers.contains(2));
        Assert.assertFalse(numbers.contains(1));
        Assert.assertFalse(numbers.contains(0));
    }

    @Test
    public void testApply() {
        // We are using an integer array here because we can't modify an integer in-place. However
        // numbers are most straight-forward to use as test values.
        final List<Integer[]> numbers = Arrays.asList(
                new Integer[] { 5 },
                new Integer[] { 7 },
                new Integer[] { 3 },
                new Integer[] { 2 });

        final Action1<Integer[]> action = new Action1<Integer[]>() {
            @Override
            public void call(Integer[] number) {
                number[0] = number[0] + 2;
            }
        };

        RankingUtils.apply(numbers, action);

        Assert.assertEquals(4, numbers.size());

        Assert.assertEquals(7, numbers.get(0)[0].intValue());
        Assert.assertEquals(9, numbers.get(1)[0].intValue());
        Assert.assertEquals(5, numbers.get(2)[0].intValue());
        Assert.assertEquals(4, numbers.get(3)[0].intValue());
    }

    @Test
    public void testApply2D() {
        final List<Integer[]> numbers1 = Arrays.asList(
                new Integer[] { 5 },
                new Integer[] { 7 },
                new Integer[] { 3 },
                new Integer[] { 2 });

        final List<Integer[]> numbers2 = Arrays.asList(
                new Integer[] { 4 },
                new Integer[] { 2 });

        final Action2<Integer[], Integer[]> action = new Action2<Integer[], Integer[]>() {
            @Override
            public void call(Integer[] left, Integer[] right) {
                left[0] = left[0] + right[0];
            }
        };

        RankingUtils.apply2D(numbers1, numbers2, action);

        Assert.assertEquals(4, numbers1.size());

        Assert.assertEquals(11, numbers1.get(0)[0].intValue());
        Assert.assertEquals(13, numbers1.get(1)[0].intValue());
        Assert.assertEquals(9, numbers1.get(2)[0].intValue());
        Assert.assertEquals(8, numbers1.get(3)[0].intValue());
    }

    @Test
    public void testApplyInPairs() {
        final List<Integer[]> numbers = Arrays.asList(
                new Integer[] { 5 },
                new Integer[] { 7 },
                new Integer[] { 3 },
                new Integer[] { 2 });

        final Action2<Integer[], Integer[]> action = new Action2<Integer[], Integer[]>() {
            @Override
            public void call(Integer[] previous, Integer[] next) {
                previous[0] = previous[0] + next[0];
            }
        };

        RankingUtils.applyInPairs(numbers, action);

        Assert.assertEquals(4, numbers.size());

        Assert.assertEquals(12, numbers.get(0)[0].intValue());
        Assert.assertEquals(10, numbers.get(1)[0].intValue());
        Assert.assertEquals(5, numbers.get(2)[0].intValue());

        // The last one is untouched because action was called with pairs (5,7) (7,3) (3,2)
        Assert.assertEquals(2, numbers.get(3)[0].intValue());
    }

    @Test
    public void testReduce() {
        final List<Integer> numbers = Arrays.asList(5, 7, 3, 2);

        final Func2<Integer,Integer,Integer> func = new Func2<Integer, Integer, Integer>() {
            @Override
            public Integer call(Integer number, Integer accumulator) {
                return accumulator + number;
            }
        };

        double value = RankingUtils.reduce(numbers, func, 5);

        Assert.assertEquals(22, value, 1e-6);
    }

    @Test
    public void testMapWithLimit() {
        final List<Integer> numbers = Arrays.asList(5, 7, 3, 2);

        final Func1<Integer, Integer> func = new Func1<Integer, Integer>() {
            @Override
            public Integer call(Integer number) {
                return number - 2;
            }
        };

        final List<Integer> result = RankingUtils.mapWithLimit(numbers, func, 2);

        Assert.assertEquals(2, result.size());

        Assert.assertEquals(3, result.get(0), 1e-6);
        Assert.assertEquals(5, result.get(1), 1e-6);
    }

    @Test
    public void testMapWithLimitAndLargeLimit() {
        final List<Integer> numbers = Arrays.asList(5, 7, 3, 2);

        final Func1<Integer, Integer> func = new Func1<Integer, Integer>() {
            @Override
            public Integer call(Integer number) {
                return number * 2;
            }
        };

        final List<Integer> result = RankingUtils.mapWithLimit(numbers, func, 100);

        Assert.assertEquals(4, result.size());

        Assert.assertEquals(10, result.get(0), 1e-6);
        Assert.assertEquals(14, result.get(1), 1e-6);
        Assert.assertEquals(6, result.get(2), 1e-6);
        Assert.assertEquals(4, result.get(3), 1e-6);
    }

    @Test
    public void testLooselyMapCursor() {
        final MatrixCursor cursor = new MatrixCursor(new String[] {
                "column1", "column2"
        });

        cursor.addRow(new Object[] { 1, 3 });
        cursor.addRow(new Object[] { 5, 7 });
        cursor.addRow(new Object[] { 2, 9 });

        final Func1<Cursor, Integer> func = new Func1<Cursor, Integer>() {
            @Override
            public Integer call(Cursor cursor) {
                // -1 is our "fail this cursor entry" sentinel.
                final int col1 = cursor.getInt(cursor.getColumnIndexOrThrow("column1"));
                if (col1 == -1) {
                    return null;
                }
                return col1 + cursor.getInt(cursor.getColumnIndexOrThrow("column2"));
            }
        };

        List<Integer> result = RankingUtils.looselyMapCursor(cursor, func);

        Assert.assertEquals(3, result.size());

        Assert.assertEquals(4, result.get(0).intValue());
        Assert.assertEquals(12, result.get(1).intValue());
        Assert.assertEquals(11, result.get(2).intValue());

        // Test that cursor entries for which func returns null are ignored.
        cursor.addRow(new Object[] {-1, 6});
        cursor.addRow(new Object[] {3, 3});

        result = RankingUtils.looselyMapCursor(cursor, func);
        Assert.assertEquals(4, result.size());

        Assert.assertEquals(4, result.get(0).intValue());
        Assert.assertEquals(12, result.get(1).intValue());
        Assert.assertEquals(11, result.get(2).intValue());
        Assert.assertEquals(6, result.get(3).intValue());
    }

    @Test
    public void testNormalize() {
        Assert.assertEquals(0, RankingUtils.normalize(0, 0, 100), 1e-6);
        Assert.assertEquals(0.5, RankingUtils.normalize(0, -100, 100), 1e-6);
        Assert.assertEquals(1, RankingUtils.normalize(1, 0, 1), 1e-6);
        Assert.assertEquals(0, RankingUtils.normalize(50, 50, 100), 1e-6);

        Assert.assertEquals(0, RankingUtils.normalize(100, 100, 100), 1e-6);
        Assert.assertEquals(0, RankingUtils.normalize(0, 0, 0), 1e-6);

        Assert.assertEquals(0.6666666, RankingUtils.normalize(50, 0, 75), 1e-6);
        Assert.assertEquals(.768, RankingUtils.normalize(768, 0, 1000), 1e-6);
        Assert.assertEquals(0.625, RankingUtils.normalize(-5, -10, -2), 1e-6);

        // min > max
        Assert.assertEquals(0, RankingUtils.normalize(75, 100, 50), 1e-6);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testNormalizeWithOutOfBoundsValue() {
        RankingUtils.normalize(50, 0, 10);
    }
}
