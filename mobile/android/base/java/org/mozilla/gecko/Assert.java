package org.mozilla.gecko;

/**
 * Static helper class to provide debug assertions for Java.
 *
 * Used in preference to JSR 41 assertions due to their difficulty of use on Android and their
 * poor behaviour w.r.t bytecode bloat (and to a lesser extent, runtime performance when disabled)
 *
 * Calls to methods in this class will be stripped by Proguard for release builds, so may be used
 * arbitrarily liberally at zero cost.
 * Under no circumstances should the argument expressions to methods in this class have side effects
 * relevant to the correctness of execution of the program. Such side effects shall not be checked
 * for when stripping assertions.
 */
public class Assert {
    // Static helper class.
    private Assert() {}

    /**
     * Verify that two objects are equal according to their equals method.
     */
    public static void equal(Object a, Object b) {
        equal(a, b, "Assertion failure: !" + a + ".equals(" + b + ')');
    }
    public static void equal(Object a, Object b, String message) {
        isTrue(a.equals(b), message);
    }

    /**
     * Verify that an arbitrary boolean expression is true.
     */
    public static void isTrue(boolean a) {
        isTrue(a, null);
    }
    public static void isTrue(boolean a, String message)  {
        if (!a) {
            throw new AssertionError(message);
        }
    }

    /**
     * Verify that an arbitrary boolean expression is false.
     */
    public static void isFalse(boolean a) {
        isTrue(a, null);
    }
    public static void isFalse(boolean a, String message)  {
        if (a) {
            throw new AssertionError(message);
        }
    }

    /**
     * Verify that a given object is null.
     */
    public static void isNull(Object o) {
        isNull(o, "Assertion failure: " + o + " must be null!");
    }
    public static void isNull(Object o, String message) {
        isTrue(o == null, message);
    }

    /**
     * Verify that a given object is non-null.
     */
    public static void isNotNull(Object o) {
        isNotNull(o, "Assertion failure: " + o + " cannot be null!");
    }
    public static void isNotNull(Object o, String message) {
        isTrue(o != null, message);
    }

    /**
     * Fail. Should be used whenever an impossible state is encountered (such as the default branch
     * of a switch over all possible values of an enum: such an assertion may save future developers
     * time when they try to add new states)
     */
    public static void fail() {
        isTrue(false);
    }
    public static void fail(String message) {
        isTrue(false, message);
    }
}
