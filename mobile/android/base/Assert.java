package org.mozilla.gecko;

public class Assert
{
    private Assert() {}

    public static void equals(Object a, Object b)
    {
        equals(a, b, null);
    }

    public static void equals(Object a, Object b, String message)
    {
        Assert.isTrue(a.equals(b), message);
    }

    public static void isTrue(boolean a)
    {
        isTrue(a, null);
    }

    public static void isTrue(boolean a, String message)
    {
        if (!AppConstants.DEBUG_BUILD) {
            return;
        }

        if (!a) {
            throw new AssertException(message);
        }
    }

    public static class AssertException extends RuntimeException
    {
        private static final long serialVersionUID = 0L;

        public AssertException(String message) {
            super(message);
        }
    }
}