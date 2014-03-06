/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests.helpers;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.*;

import org.mozilla.gecko.tests.UITestContext;

import java.lang.reflect.Field;

import android.content.Context;
import android.view.View;

/**
 * Provides helper functions for accessing Android framework features
 *
 * This class uses reflection to access framework functionalities that are
 * unavailable through the regular Android API. Using reflection in this
 * case is okay because it does not touch Gecko classes that go through
 * ProGuard.
 */
public final class FrameworkHelper {

    private FrameworkHelper() { /* To disallow instantiation. */ }

    private static Field getClassField(final Class<?> clazz, final String fieldName)
            throws NoSuchFieldException {
        Class<?> cls = clazz;
        do {
            try {
                return cls.getDeclaredField(fieldName);
            } catch (final NoSuchFieldException e) {
                cls = cls.getSuperclass();
            }
        } while (cls != null);
        // We tried getDeclaredField before; now try getField instead.
        // getField behaves differently in that getField traverses the inheritance
        // list, but it only works on public fields. While getField won't get us
        // anything new, it makes code cleaner by throwing an exception for us.
        return clazz.getField(fieldName);
    }

    private static Object getField(final Object obj, final String fieldName) {
        try {
            final Field field = getClassField(obj.getClass(), fieldName);
            final boolean accessible = field.isAccessible();
            field.setAccessible(true);
            final Object ret = field.get(obj);
            field.setAccessible(accessible);
            return ret;
        } catch (final NoSuchFieldException e) {
            // We expect a valid field name; if it's not valid,
            // the caller is doing something wrong and should be fixed.
            fFail("Argument field should be a valid field name: " + e.toString());
        } catch (final IllegalAccessException e) {
            // This should not happen. If it does, setAccessible above is not working.
            fFail("Field should be accessible: " + e.toString());
        }
        throw new IllegalStateException("Should not continue from previous failures");
    }

    private static void setField(final Object obj, final String fieldName, final Object value) {
        try {
            final Field field = getClassField(obj.getClass(), fieldName);
            final boolean accessible = field.isAccessible();
            field.setAccessible(true);
            field.set(obj, value);
            field.setAccessible(accessible);
            return;
        } catch (final NoSuchFieldException e) {
            // We expect a valid field name; if it's not valid,
            // the caller is doing something wrong and should be fixed.
            fFail("Argument field should be a valid field name: " + e.toString());
        } catch (final IllegalAccessException e) {
            // This should not happen. If it does, setAccessible above is not working.
            fFail("Field should be accessible: " + e.toString());
        }
        throw new IllegalStateException("Cannot continue from previous failures");
    }

    public static Context getViewContext(final View v) {
        return (Context) getField(v, "mContext");
    }

    public static void setViewContext(final View v, final Context c) {
        setField(v, "mContext", c);
    }
}
