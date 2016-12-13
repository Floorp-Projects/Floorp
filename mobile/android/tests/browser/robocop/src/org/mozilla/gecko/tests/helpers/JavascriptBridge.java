/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests.helpers;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import junit.framework.AssertionFailedError;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.Actions.EventExpecter;
import org.mozilla.gecko.Assert;
import org.mozilla.gecko.tests.UITestContext;
import org.mozilla.gecko.util.GeckoBundle;

/**
 * Javascript bridge allows calls to and from JavaScript.
 *
 * To establish communication, create an instance of JavascriptBridge in Java and pass in
 * an object that will receive calls from JavaScript. For example:
 *
 *  {@code final JavascriptBridge js = new JavascriptBridge(javaObj);}
 *
 * Next, create an instance of JavaBridge in JavaScript and pass in another object
 * that will receive calls from Java. For example:
 *
 *  {@code let java = new JavaBridge(jsObj);}
 *
 * Once a link is established, calls can be made using the methods syncCall and asyncCall.
 * syncCall waits for the call to finish before returning. For example:
 *
 *  {@code js.syncCall("abc", 1, 2, 3);} will synchronously call the JavaScript method
 *    jsObj.abc and pass in arguments 1, 2, and 3.
 *
 *  {@code java.asyncCall("def", 4, 5, 6);} will asynchronously call the Java method
 *    javaObj.def and pass in arguments 4, 5, and 6.
 *
 * Supported argument types include int, double, boolean, String, and GeckoBundle. Note
 * that only implicit conversion is done, meaning if a floating point argument is passed
 * from JavaScript to Java, the call will fail if the Java method has an int argument.
 *
 * Because JavascriptBridge and JavaBridge use one underlying communication channel,
 * creating multiple instances of them will not create independent links.
 *
 * Note also that because Robocop tests finish as soon as the Java test method returns,
 * the last call to JavaScript from Java must be a synchronous call. Otherwise, the test
 * will finish before the JavaScript method is run. Calls to Java from JavaScript do not
 * have this requirement. Because of these considerations, calls from Java to JavaScript
 * are usually synchronous and calls from JavaScript to Java are usually asynchronous.
 * See testJavascriptBridge.java for examples.
 */
public final class JavascriptBridge {

    private static enum MessageStatus {
        QUEUE_EMPTY, // Did not process a message; queue was empty.
        PROCESSED,   // A message other than sync was processed.
        REPLIED,     // A sync message was processed.
        SAVED,       // An async message was saved; see processMessage().
    };

    @SuppressWarnings("serial")
    public static class CallException extends RuntimeException {
        public CallException() {
            super();
        }

        public CallException(final String msg) {
            super(msg);
        }

        public CallException(final String msg, final Throwable e) {
            super(msg, e);
        }

        public CallException(final Throwable e) {
            super(e);
        }
    }

    public static final String EVENT_TYPE = "Robocop:Java";
    public static final String JS_EVENT_TYPE = "Robocop:JS";

    private static Actions sActions;
    private static Assert sAsserter;

    // Target of JS-to-Java calls
    private final Object mTarget;
    // List of public methods in subclass
    private final Method[] mMethods;
    // Parser for handling xpcshell assertions
    private final JavascriptMessageParser mLogParser;
    // Expecter of our internal Robocop event
    private final EventExpecter mExpecter;
    // Saved async message; see processMessage() for its purpose.
    private GeckoBundle mSavedAsyncMessage;
    // Number of levels in the synchronous call stack
    private int mCallStackDepth;
    // If JavaBridge has been loaded
    private boolean mJavaBridgeLoaded;

    /* package */ static void init(final UITestContext context) {
        sActions = context.getActions();
        sAsserter = context.getAsserter();
    }

    public JavascriptBridge(final Object target) {
        mTarget = target;
        mMethods = target.getClass().getMethods();
        mExpecter = sActions.expectGlobalEvent(Actions.EventType.GECKO, EVENT_TYPE);
        // The JS here is unrelated to a test harness, so we
        // have our message parser end on assertion failure.
        mLogParser = new JavascriptMessageParser(sAsserter, true);
    }

    /**
     * Synchronously calls a method in Javascript.
     *
     * @param method Name of the method to call
     * @param args Arguments to pass to the Javascript method; must be a list of
     *             values allowed by GeckoBundle.
     */
    public void syncCall(final String method, final Object... args) {
        mCallStackDepth++;

        sendMessage("sync-call", method, args);
        try {
            while (processPendingMessage() != MessageStatus.REPLIED) {
            }
        } catch (final AssertionFailedError e) {
            // Most likely an event expecter time out
            throw new CallException("Cannot call " + method, e);
        }

        // If syncCall was called reentrantly from processPendingMessage(), mCallStackDepth
        // will be greater than 1 here. In that case we don't have to wait for pending calls
        // because the outermost syncCall will do it for us.
        if (mCallStackDepth == 1) {
            // We want to wait for all asynchronous calls to finish,
            // because the test may end immediately after this method returns.
            finishPendingCalls();
        }
        mCallStackDepth--;
    }

    /**
     * Asynchronously calls a method in Javascript.
     *
     * @param method Name of the method to call
     * @param args Arguments to pass to the Javascript method; must be a list of
     *             values allowed by GeckoBundle.
     */
    public void asyncCall(final String method, final Object... args) {
        sendMessage("async-call", method, args);
    }

    /**
     * Disconnect the bridge.
     */
    public void disconnect() {
        mExpecter.unregisterListener();
    }

    /**
     * Process a new message; wait for new message if necessary.
     *
     * @return MessageStatus value to indicate result of processing the message
     */
    private MessageStatus processPendingMessage() {
        // We're on the test thread.
        // We clear mSavedAsyncMessage in maybeProcessPendingMessage() but not here,
        // because we always have a new message for processing here, so we never
        // get a chance to clear mSavedAsyncMessage.
        return processMessage(mExpecter.blockForBundle());
    }

    /**
     * Process a message if a new or saved message is available.
     *
     * @return MessageStatus value to indicate result of processing the message
     */
    private MessageStatus maybeProcessPendingMessage() {
        // We're on the test thread.
        final GeckoBundle message = mExpecter.blockForBundleWithTimeout(0);
        if (message != null) {
            return processMessage(message);
        }
        if (mSavedAsyncMessage != null) {
            // processMessage clears mSavedAsyncMessage.
            return processMessage(mSavedAsyncMessage);
        }
        return MessageStatus.QUEUE_EMPTY;
    }

    /**
     * Wait for all asynchronous messages from Javascript to be processed.
     */
    private void finishPendingCalls() {
        MessageStatus result;
        do {
            result = maybeProcessPendingMessage();
            if (result == MessageStatus.REPLIED) {
                throw new IllegalStateException("Sync reply was unexpected");
            }
        } while (result != MessageStatus.QUEUE_EMPTY);
    }

    private void ensureJavaBridgeLoaded() {
        while (!mJavaBridgeLoaded) {
            processPendingMessage();
        }
    }

    private void sendMessage(final String innerType, final String method, final Object[] args) {
        ensureJavaBridgeLoaded();

        // Call from Java to Javascript
        final GeckoBundle message = new GeckoBundle();
        final GeckoBundle bundleArgs = new GeckoBundle();
        if (args == null) {
            bundleArgs.putInt("length", 0);
        } else {
            for (int i = 0; i < args.length; i++) {
                putInBundle(bundleArgs, String.valueOf(i), args[i]);
            }
            bundleArgs.putInt("length", args.length);
        }
        message.putString("type", JS_EVENT_TYPE);
        message.putString("innerType", innerType);
        message.putString("method", method);
        message.putBundle("args", bundleArgs);

        sActions.sendGlobalEvent(JS_EVENT_TYPE, message);
    }

    private MessageStatus processMessage(GeckoBundle message) {
        final String type;
        final String methodName;
        final GeckoBundle argsArray;
        final Object[] args;

        type = message.getString("innerType");

        switch (type) {
            case "progress":
                // Javascript harness message
                mLogParser.logMessage(message.getString("message"));
                return MessageStatus.PROCESSED;

            case "notify-loaded":
                mJavaBridgeLoaded = true;
                return MessageStatus.PROCESSED;

            case "sync-reply":
                // Reply to Java-to-Javascript sync call
                return MessageStatus.REPLIED;

            case "sync-call":
            case "async-call":

                if ("async-call".equals(type)) {
                    // Save this async message until another async message arrives, then we
                    // process the saved message and save the new one. This is done as a
                    // form of tail call optimization, by making sync-replies come before
                    // async-calls. On the other hand, if (message == mSavedAsyncMessage),
                    // it means we're currently processing the saved message and should clear
                    // mSavedAsyncMessage.
                    final GeckoBundle newSavedMessage =
                            (message != mSavedAsyncMessage ? message : null);
                    message = mSavedAsyncMessage;
                    mSavedAsyncMessage = newSavedMessage;
                    if (message == null) {
                        // Saved current message and there wasn't an already saved one.
                        return MessageStatus.SAVED;
                    }
                }

                methodName = message.getString("method");
                argsArray = message.getBundle("args");
                args = new Object[argsArray.getInt("length")];
                for (int i = 0; i < args.length; i++) {
                    args[i] = argsArray.get(String.valueOf(i));
                }
                invokeMethod(methodName, args);

                if ("sync-call".equals(type)) {
                    // Reply for sync messages
                    sendMessage("sync-reply", methodName, null);
                }
                return MessageStatus.PROCESSED;
        }

        throw new IllegalStateException("Message type is unexpected");
    }

    /**
     * Given a method name and a list of arguments,
     * call the most suitable method in the subclass.
     */
    private Object invokeMethod(final String methodName, final Object[] args) {
        final Class<?>[] argTypes = new Class<?>[args.length];
        for (int i = 0; i < argTypes.length; i++) {
            if (args[i] == null) {
                argTypes[i] = Object.class;
            } else {
                argTypes[i] = args[i].getClass();
            }
        }

        // Try using argument types directly without casting.
        try {
            return invokeMethod(mTarget.getClass().getMethod(methodName, argTypes), args);
        } catch (final NoSuchMethodException e) {
            // getMethod() failed; try fallback below.
        }

        // One scenario for getMethod() to fail above is that we don't have the exact
        // argument types in argTypes (e.g. JS gave us an int but we're using a double,
        // or JS gave us a null and we don't know its intended type), or the number of
        // arguments is incorrect. Now we find all the methods with the given name and
        // try calling them one-by-one. If one call fails, we move to the next call.
        // Java will try to convert our arguments to the right types.
        Throwable lastException = null;
        for (final Method method : mMethods) {
            if (!method.getName().equals(methodName)) {
                continue;
            }
            try {
                return invokeMethod(method, args);
            } catch (final IllegalArgumentException e) {
                lastException = e;
                // Try the next method
            } catch (final UnsupportedOperationException e) {
                // "Cannot access method" exception below, see if there are other public methods
                lastException = e;
                // Try the next method
            }
        }
        // Now we're out of options
        throw new UnsupportedOperationException(
            "Cannot call method " + methodName + " (not public? wrong argument types?)",
            lastException);
    }

    private Object invokeMethod(final Method method, final Object[] args) {
        try {
            return method.invoke(mTarget, args);
        } catch (final IllegalAccessException e) {
            throw new UnsupportedOperationException(
                "Cannot access method " + method.getName(), e);
        } catch (final InvocationTargetException e) {
            final Throwable cause = e.getCause();
            if (cause instanceof CallException) {
                // Don't wrap CallExceptions; this can happen if a call is nested on top
                // of existing sync calls, and the nested call throws a CallException
                throw (CallException) cause;
            }
            throw new CallException("Failed to invoke " + method.getName(), cause);
        }
    }

    private void putInBundle(final GeckoBundle bundle, final String key, final Object value) {
        if (value == null) {
            bundle.putBundle(key, null);
        } else if (value instanceof Boolean) {
            bundle.putBoolean(key, (Boolean) value);
        } else if (value instanceof boolean[]) {
            bundle.putBooleanArray(key, (boolean[]) value);
        } else if (value instanceof Double) {
            bundle.putDouble(key, (Double) value);
        } else if (value instanceof double[]) {
            bundle.putDoubleArray(key, (double[]) value);
        } else if (value instanceof Integer) {
            bundle.putInt(key, (Integer) value);
        } else if (value instanceof int[]) {
            bundle.putIntArray(key, (int[]) value);
        } else if (value instanceof CharSequence) {
            bundle.putString(key, value.toString());
        } else if (value instanceof String[]) {
            bundle.putStringArray(key, (String[]) value);
        } else if (value instanceof GeckoBundle) {
            bundle.putBundle(key, (GeckoBundle) value);
        } else if (value instanceof GeckoBundle[]) {
            bundle.putBundleArray(key, (GeckoBundle[]) value);
        } else {
            throw new UnsupportedOperationException();
        }
    }
}
