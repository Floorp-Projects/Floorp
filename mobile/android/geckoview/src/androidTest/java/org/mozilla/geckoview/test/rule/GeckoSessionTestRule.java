/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test.rule;

import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoSessionSettings;
import org.mozilla.geckoview.test.util.Callbacks;

import static org.hamcrest.Matchers.*;
import static org.junit.Assert.fail;

import org.hamcrest.Matcher;

import org.junit.rules.ErrorCollector;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import android.app.Instrumentation;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.MessageQueue;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.UiThreadTestRule;

import java.lang.annotation.Annotation;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.ParameterizedType;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.regex.Pattern;

import kotlin.jvm.JvmClassMappingKt;
import kotlin.reflect.KClass;

/**
 * TestRule that, for each test, sets up a GeckoSession, runs the test on the UI thread,
 * and tears down the GeckoSession at the end of the test. The rule also provides methods
 * for waiting on particular callbacks to be called, and methods for asserting that
 * callbacks are called in the proper order.
 */
public class GeckoSessionTestRule extends UiThreadTestRule {

    private static final long DEFAULT_TIMEOUT_MILLIS = 10000;
    public static final String APK_URI_PREFIX = "resource://android";

    /**
     * Specify the timeout for any of the wait methods, in milliseconds. Can be used
     * on classes or methods.
     */
    @Target({ElementType.METHOD, ElementType.TYPE})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface TimeoutMillis {
        long value();
    }

    /**
     * Specify a list of GeckoSession settings to be applied to the GeckoSession object
     * under test. Can be used on classes or methods. Note that the settings values must
     * be string literals regardless of the type of the settings.
     * <p>
     * Disable e10s for a particular test:
     * <pre>
     * &#64;Setting.List(&#64;Setting(key = Setting.Key.USE_MULTIPROCESS,
     *                        value = "false"))
     * &#64;Test public void test() { ... }
     * </pre>
     * <p>
     * Use multiple settings:
     * <pre>
     * &#64;Setting.List({&#64;Setting(key = Setting.Key.USE_MULTIPROCESS,
     *                         value = "false"),
     *                &#64;Setting(key = Setting.Key.USE_TRACKING_PROTECTION,
     *                         value = "true")})
     * </pre>
     */
    @Target({ElementType.ANNOTATION_TYPE, ElementType.METHOD, ElementType.TYPE})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface Setting {
        enum Key {
            CHROME_URI,
            DISPLAY_MODE,
            SCREEN_ID,
            USE_MULTIPROCESS,
            USE_PRIVATE_MODE,
            USE_REMOTE_DEBUGGER,
            USE_TRACKING_PROTECTION;

            private final GeckoSessionSettings.Key<?> mKey;
            private final Class<?> mType;

            Key() {
                final Field field;
                try {
                    field = GeckoSessionSettings.class.getField(name());
                    mKey = (GeckoSessionSettings.Key<?>) field.get(null);
                } catch (final NoSuchFieldException | IllegalAccessException e) {
                    throw new RuntimeException(e);
                }

                final ParameterizedType genericType = (ParameterizedType) field.getGenericType();
                mType = (Class<?>) genericType.getActualTypeArguments()[0];
            }

            @SuppressWarnings("unchecked")
            public void set(final GeckoSessionSettings settings, final String value) {
                if (boolean.class.equals(mType) || Boolean.class.equals(mType)) {
                    settings.setBoolean((GeckoSessionSettings.Key<Boolean>) mKey,
                            Boolean.valueOf(value));
                } else if (int.class.equals(mType) || Integer.class.equals(mType)) {
                    try {
                        settings.setInt((GeckoSessionSettings.Key<Integer>) mKey,
                                (Integer) GeckoSessionSettings.class.getField(value)
                                        .get(null));
                        return;
                    } catch (final NoSuchFieldException | IllegalAccessException |
                            ClassCastException e) {
                    }
                    settings.setInt((GeckoSessionSettings.Key<Integer>) mKey,
                            Integer.valueOf(value));
                } else if (String.class.equals(mType)) {
                    settings.setString((GeckoSessionSettings.Key<String>) mKey, value);
                } else {
                    throw new IllegalArgumentException("Unsupported type: " +
                            mType.getSimpleName());
                }
            }
        }

        @Target({ElementType.METHOD, ElementType.TYPE})
        @Retention(RetentionPolicy.RUNTIME)
        @interface List {
            Setting[] value();
        }

        Key key();
        String value();
    }

    /**
     * Assert that a method is called or not called, and if called, the order and number
     * of times it is called. The order number is a monotonically increasing integer; if
     * an called method's order number is less than the current order number, an exception
     * is raised for out-of-order call.
     * <p>
     * {@code @AssertCalled} asserts the method must be called at least once.
     * <p>
     * {@code @AssertCalled(false)} asserts the method must not be called.
     * <p>
     * {@code @AssertCalled(order = 2)} asserts the method must be called once and
     * after any other method with order number less than 2.
     * <p>
     * {@code @AssertCalled(order = {2, 4})} asserts order number 2 for first
     * call and order number 4 for any subsequent calls.
     * <p>
     * {@code @AssertCalled(count = 2)} asserts two calls total in any order
     * with respect to other calls.
     * <p>
     * {@code @AssertCalled(count = 2, order = 2)} asserts two calls, both with
     * order number 2.
     * <p>
     * {@code @AssertCalled(count = 2, order = {2, 4, 6})} asserts two calls
     * total: the first with order number 2 and the second with order number 4.
     */
    @Target(ElementType.METHOD)
    @Retention(RetentionPolicy.RUNTIME)
    public @interface AssertCalled {
        /**
         * @return True if the method must be called,
         *         or false if the method must not be called.
         */
        boolean value() default true;

        /**
         * @return If called, the number of calls called, or 0 to allow any number > 0.
         */
        int count() default 0;

        /**
         * @return If called, the order number for each call, or 0 to allow arbitrary
         *         order. If order's length is more than count, extra elements are not used;
         *         if order's length is less than count, the last element is repeated.
         */
        int[] order() default 0;
    }

    public static class CallRequirement {
        public final boolean allowed;
        public final int count;
        public final int[] order;

        public CallRequirement(final boolean allowed, final int count, final int[] order) {
            this.allowed = allowed;
            this.count = count;
            this.order = order;
        }
    }

    public static class CallInfo {
        public final int counter;
        public final int order;

        /* package */ CallInfo(final int counter, final int order) {
            this.counter = counter;
            this.order = order;
        }
    }

    public static class MethodCall {
        public final Method method;
        public final CallRequirement requirement;
        public final Object target;
        private int currentCount;

        public MethodCall(final Method method,
                          final CallRequirement requirement) {
            this(method, requirement, /* target */ null);
        }

        /* package */ MethodCall(final Method method, final AssertCalled annotation,
                                 final Object target) {
            this(method,
                 (annotation != null) ? new CallRequirement(annotation.value(),
                                                            annotation.count(),
                                                            annotation.order())
                                      : null,
                 /* target */ target);
        }

        /* package */ MethodCall(final Method method, final CallRequirement requirement,
                                 final Object target) {
            this.method = method;
            this.requirement = requirement;
            this.target = target;
            currentCount = 0;
        }

        @Override
        public boolean equals(final Object other) {
            if (this == other) {
                return true;
            } else if (other instanceof MethodCall) {
                return methodsEqual(method, ((MethodCall) other).method);
            } else if (other instanceof Method) {
                return methodsEqual(method, (Method) other);
            }
            return false;
        }

        /* package */ int getOrder() {
            if (requirement == null || currentCount == 0) {
                return 0;
            }

            final int[] order = requirement.order;
            if (order == null || order.length == 0) {
                return 0;
            }
            return order[Math.min(currentCount - 1, order.length - 1)];
        }

        /* package */ int getCount() {
            return (requirement == null) ? 0 :
                   !requirement.allowed ? -1 : requirement.count;
        }

        /* package */ void incrementCounter() {
            currentCount++;
        }

        /* package */ int getCurrentCount() {
            return currentCount;
        }

        /* package */ boolean allowUnlimitedCalls() {
            return getCount() == 0;
        }

        /* package */ boolean allowMoreCalls() {
            final int count = getCount();
            return count == 0 || count > currentCount;
        }

        /* package */ CallInfo getInfo() {
            return new CallInfo(currentCount, getOrder());
        }

        // Similar to Method.equals, but treat the same method from an interface and an
        // overriding class as the same (e.g. CharSequence.length == String.length).
        private static boolean methodsEqual(final @NonNull Method m1, final @NonNull Method m2) {
            return (m1.getDeclaringClass().isAssignableFrom(m2.getDeclaringClass()) ||
                    m2.getDeclaringClass().isAssignableFrom(m1.getDeclaringClass())) &&
                    m1.getName().equals(m2.getName()) &&
                    m1.getReturnType().equals(m2.getReturnType()) &&
                    Arrays.equals(m1.getParameterTypes(), m2.getParameterTypes());
        }
    }

    protected class CallbackDelegates {
        private final Map<Method, MethodCall> mDelegates = new HashMap<>();
        private int mOrder;

        public void delegate(final Object callback) {
            for (final Class<?> ifce : CALLBACK_CLASSES) {
                if (!ifce.isInstance(callback)) {
                    continue;
                }
                for (final Method method : ifce.getMethods()) {
                    final Method callbackMethod;
                    try {
                        callbackMethod = callback.getClass().getMethod(method.getName(),
                                                                       method.getParameterTypes());
                    } catch (final NoSuchMethodException e) {
                        throw new RuntimeException(e);
                    }
                    final MethodCall call = new MethodCall(
                            callbackMethod, getAssertCalled(callbackMethod, callback), callback);
                    mDelegates.put(method, call);
                }
            }
        }

        public void clear() {
            final Collection<MethodCall> values = mDelegates.values();
            final MethodCall[] valuesArray = values.toArray(new MethodCall[values.size()]);

            mDelegates.clear();
            mOrder = 0;

            for (final MethodCall call : valuesArray) {
                assertMatchesCount(call);
            }
        }

        public MethodCall prepareMethodCall(final Method method) {
            final MethodCall call = mDelegates.get(method);
            if (call == null) {
                return null;
            }

            assertAllowMoreCalls(call);
            call.incrementCounter();
            assertOrder(call, mOrder);
            mOrder = Math.max(call.getOrder(), mOrder);
            return call;
        }
    }

    protected static class CallRecord {
        public final Method method;
        public final MethodCall methodCall;
        public final Object[] args;

        public CallRecord(final Method method, final Object[] args) {
            this.method = method;
            this.methodCall = new MethodCall(method, /* requirement */ null);
            this.args = args;
        }
    }

    /* package */ static AssertCalled getAssertCalled(final Method method, final Object callback) {
        final AssertCalled annotation = method.getAnnotation(AssertCalled.class);
        if (annotation != null) {
            return annotation;
        }

        // Some Kotlin lambdas have an invoke method that carries the annotation,
        // instead of the interface method carrying the annotation.
        try {
            return callback.getClass().getDeclaredMethod(
                    "invoke", method.getParameterTypes()).getAnnotation(AssertCalled.class);
        } catch (final NoSuchMethodException e) {
            return null;
        }
    }

    private static void addCallbackClasses(final List<Class<?>> list, final Class<?> ifce) {
        if (!Callbacks.class.equals(ifce.getDeclaringClass())) {
            list.add(ifce);
            return;
        }
        final Class<?>[] superIfces = ifce.getInterfaces();
        for (final Class<?> superIfce : superIfces) {
            addCallbackClasses(list, superIfce);
        }
    }

    private static Class<?>[] getCallbackClasses() {
        final Class<?>[] ifces = Callbacks.class.getDeclaredClasses();
        final List<Class<?>> list = new ArrayList<>(ifces.length);

        for (final Class<?> ifce : ifces) {
            addCallbackClasses(list, ifce);
        }

        final HashSet<Class<?>> set = new HashSet<>(list);
        return set.toArray(new Class<?>[set.size()]);
    }

    private static final List<Class<?>> CALLBACK_CLASSES = Arrays.asList(getCallbackClasses());

    protected final Instrumentation mInstrumentation =
            InstrumentationRegistry.getInstrumentation();
    protected final GeckoSessionSettings mDefaultSettings;

    protected ErrorCollector mErrorCollector;
    protected GeckoSession mSession;
    protected Object mCallbackProxy;
    protected List<CallRecord> mCallRecords;
    protected CallbackDelegates mWaitScopeDelegates;
    protected CallbackDelegates mTestScopeDelegates;
    protected int mLastWaitStart;
    protected int mLastWaitEnd;
    protected MethodCall mCurrentMethodCall;
    protected long mTimeoutMillis = DEFAULT_TIMEOUT_MILLIS;

    public GeckoSessionTestRule() {
        mDefaultSettings = new GeckoSessionSettings();
    }

    /**
     * Set an ErrorCollector for assertion errors, or null to not use one.
     *
     * @param ec ErrorCollector or null.
     */
    public void setErrorCollector(final @Nullable ErrorCollector ec) {
        mErrorCollector = ec;
    }

    /**
     * Get the current ErrorCollector, or null if not using one.
     *
     * @return ErrorCollector or null.
     */
    public @Nullable ErrorCollector getErrorCollector() {
        return mErrorCollector;
    }

    private <T> void assertThat(final String reason, final T value, final Matcher<T> matcher) {
        if (mErrorCollector != null) {
            mErrorCollector.checkThat(reason, value, matcher);
        } else {
            org.junit.Assert.assertThat(reason, value, matcher);
        }
    }

    private void assertAllowMoreCalls(final MethodCall call) {
        final int count = call.getCount();
        if (count != 0) {
            assertThat(call.method.getName() + " call count should be within limit",
                       call.getCurrentCount(), lessThan(Math.max(0, count)));
        }
    }

    private void assertOrder(final MethodCall call, final int order) {
        final int newOrder = call.getOrder();
        if (newOrder != 0) {
            assertThat(call.method.getName() + " should be in order",
                       newOrder, greaterThanOrEqualTo(order));
        }
    }

    private void assertMatchesCount(final MethodCall call) {
        if (call.requirement == null) {
            return;
        }
        final int count = call.getCount();
        if (count < 0) {
            assertThat(call.method.getName() + " should not be called",
                       call.getCurrentCount(), equalTo(0));
        } else if (count == 0) {
            assertThat(call.method.getName() + " should be called",
                       call.getCurrentCount(), greaterThan(0));
        } else {
            assertThat(call.method.getName() +
                       " should be called specified number of times",
                       call.getCurrentCount(), equalTo(count));
        }
    }

    /**
     * Get the session set up for the current test.
     *
     * @return GeckoSession object.
     */
    public @NonNull GeckoSession getSession() {
        return mSession;
    }

    protected static Method getCallbackSetter(final @NonNull Class<?> cls)
            throws NoSuchMethodException {
        return GeckoSession.class.getMethod("set" + cls.getSimpleName(), cls);
    }

    protected static Method getCallbackGetter(final @NonNull Class<?> cls)
            throws NoSuchMethodException {
        return GeckoSession.class.getMethod("get" + cls.getSimpleName());
    }

    protected void applyAnnotations(final Collection<Annotation> annotations,
                                    final GeckoSessionSettings settings) {
        for (final Annotation annotation : annotations) {
            if (TimeoutMillis.class.equals(annotation.annotationType())) {
                mTimeoutMillis = Math.max(((TimeoutMillis) annotation).value(), 100);
            } else if (Setting.class.equals(annotation.annotationType())) {
                ((Setting) annotation).key().set(settings, ((Setting) annotation).value());
            } else if (Setting.List.class.equals(annotation.annotationType())) {
                for (final Setting setting : ((Setting.List) annotation).value()) {
                    setting.key().set(settings, setting.value());
                }
            }
        }
    }

    protected void prepareSession(final Description description) throws Throwable {
        final GeckoSessionSettings settings = new GeckoSessionSettings(mDefaultSettings);

        applyAnnotations(Arrays.asList(description.getTestClass().getAnnotations()), settings);
        applyAnnotations(description.getAnnotations(), settings);

        final List<CallRecord> records = new ArrayList<>();
        final CallbackDelegates waitDelegates = new CallbackDelegates();
        final CallbackDelegates testDelegates = new CallbackDelegates();
        mCallRecords = records;
        mWaitScopeDelegates = waitDelegates;
        mTestScopeDelegates = testDelegates;
        mLastWaitStart = 0;
        mLastWaitEnd = 0;

        final InvocationHandler recorder = new InvocationHandler() {
            @Override
            public Object invoke(final Object proxy, final Method method,
                                 final Object[] args) {
                assertThat("Callbacks must be on UI thread",
                           Looper.myLooper(), equalTo(Looper.getMainLooper()));

                records.add(new CallRecord(method, args));

                MethodCall call = waitDelegates.prepareMethodCall(method);
                if (call == null) {
                    call = testDelegates.prepareMethodCall(method);
                }

                try {
                    mCurrentMethodCall = call;
                    return method.invoke((call != null) ? call.target
                                                        : Callbacks.Default.INSTANCE, args);
                } catch (final IllegalAccessException | InvocationTargetException e) {
                    throw new RuntimeException(e.getCause() != null ? e.getCause() : e);
                } finally {
                    mCurrentMethodCall = null;
                }
            }
        };

        final Class<?>[] classes = CALLBACK_CLASSES.toArray(new Class<?>[CALLBACK_CLASSES.size()]);
        mCallbackProxy = Proxy.newProxyInstance(GeckoSession.class.getClassLoader(),
                                                classes, recorder);

        mSession = new GeckoSession(settings);

        for (final Class<?> cls : CALLBACK_CLASSES) {
            if (cls != null) {
                getCallbackSetter(cls).invoke(mSession, mCallbackProxy);
            }
        }

        mSession.openWindow(mInstrumentation.getTargetContext());

        if (settings.getBoolean(GeckoSessionSettings.USE_MULTIPROCESS)) {
            // Under e10s, we receive an initial about:blank load; don't expose that to the test.
            waitForPageStop();
        }
    }

    /**
     * Internal method to perform callback checks at the end of a test.
     */
    public void performTestEndCheck() {
        mWaitScopeDelegates.clear();
        mTestScopeDelegates.clear();
    }

    protected void cleanupSession() throws Throwable {
        if (mSession.isOpen()) {
            mSession.closeWindow();
        }
        mSession = null;
        mCallbackProxy = null;
        mCallRecords = null;
        mWaitScopeDelegates = null;
        mTestScopeDelegates = null;
        mLastWaitStart = 0;
        mLastWaitEnd = 0;
        mTimeoutMillis = DEFAULT_TIMEOUT_MILLIS;
    }

    @Override
    public Statement apply(final Statement base, final Description description) {
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                try {
                    prepareSession(description);
                    base.evaluate();
                    performTestEndCheck();
                } finally {
                    cleanupSession();
                }
            }
        }, description);
    }

    @Override
    protected boolean shouldRunOnUiThread(final Description description) {
        return true;
    }

    /**
     * Loop the current thread until the message queue is idle. If loop is already idle and
     * timeout is not specified, return immediately. If loop is already idle and timeout is
     * specified, wait for a message to arrive first; an exception is thrown if timeout
     * expires during the wait.
     *
     * @param timeout Wait timeout in milliseconds or 0 to not wait.
     */
    protected static void loopUntilIdle(final long timeout) {
        // Adapted from GeckoThread.pumpMessageLoop.
        final Looper looper = Looper.myLooper();
        final MessageQueue queue = looper.getQueue();
        final Handler handler = new Handler(looper);
        final MessageQueue.IdleHandler idleHandler = new MessageQueue.IdleHandler() {
            @Override
            public boolean queueIdle() {
                final Message msg = Message.obtain(handler);
                msg.obj = handler;
                handler.sendMessageAtFrontOfQueue(msg);
                return false; // Remove this idle handler.
            }
        };

        final Method getNextMessage;
        try {
            getNextMessage = queue.getClass().getDeclaredMethod("next");
        } catch (final NoSuchMethodException e) {
            throw new RuntimeException(e);
        }
        getNextMessage.setAccessible(true);

        final Runnable timeoutRunnable = new Runnable() {
            @Override
            public void run() {
                fail("Timed out after " + timeout + "ms");
            }
        };
        if (timeout > 0) {
            handler.postDelayed(timeoutRunnable, timeout);
        } else {
            queue.addIdleHandler(idleHandler);
        }

        try {
            while (true) {
                final Message msg;
                try {
                    msg = (Message) getNextMessage.invoke(queue);
                } catch (final IllegalAccessException | InvocationTargetException e) {
                    throw new RuntimeException(e.getCause() != null ? e.getCause() : e);
                }
                if (msg.getTarget() == handler && msg.obj == handler) {
                    // Our idle signal.
                    break;
                } else if (msg.getTarget() == null) {
                    looper.quit();
                    break;
                }
                msg.getTarget().dispatchMessage(msg);

                if (timeout > 0) {
                    handler.removeCallbacks(timeoutRunnable);
                    queue.addIdleHandler(idleHandler);
                }
            }
        } finally {
            if (timeout > 0) {
                handler.removeCallbacks(timeoutRunnable);
            }
        }
    }

    /**
     * Wait until a page load has finished. The session must have started a page load since
     * the last wait, or this method will wait indefinitely.
     */
    public void waitForPageStop() {
        waitForPageStops(/* count */ 1);
    }

    /**
     * Wait until a page load has finished. The session must have started a page load since
     * the last wait, or this method will wait indefinitely.
     *
     * @param count Number of page loads to wait for.
     */
    public void waitForPageStops(final int count) {
        final Method onPageStop;
        try {
            onPageStop = GeckoSession.ProgressListener.class.getMethod(
                    "onPageStop", GeckoSession.class, boolean.class);
        } catch (final NoSuchMethodException e) {
            throw new RuntimeException(e);
        }

        final List<MethodCall> methodCalls = new ArrayList<>(1);
        methodCalls.add(new MethodCall(onPageStop,
                new CallRequirement(/* allowed */ true, count, null)));

        waitUntilCalled(GeckoSession.ProgressListener.class, methodCalls);
    }

    /**
     * Wait until the specified methods have been called on the specified callback
     * interface. If no methods are specified, wait until any method has been called.
     *
     * @param callback Target callback interface; must be an interface under GeckoSession.
     * @param methods List of methods to wait on; use empty or null or wait on any method.
     */
    public void waitUntilCalled(final @NonNull KClass<?> callback,
                                final @Nullable String... methods) {
        waitUntilCalled(JvmClassMappingKt.getJavaClass(callback), methods);
    }

    /**
     * Wait until the specified methods have been called on the specified callback
     * interface. If no methods are specified, wait until any method has been called.
     *
     * @param callback Target callback interface; must be an interface under GeckoSession.
     * @param methods List of methods to wait on; use empty or null or wait on any method.
     */
    public void waitUntilCalled(final @NonNull Class<?> callback,
                                final @Nullable String... methods) {
        assertThat("Class should be a GeckoSession interface",
                   callback, isIn(CALLBACK_CLASSES));

        final int length = (methods != null) ? methods.length : 0;
        final Pattern[] patterns = new Pattern[length];
        for (int i = 0; i < length; i++) {
            patterns[i] = Pattern.compile(methods[i]);
        }

        final List<MethodCall> waitMethods = new ArrayList<>();

        for (final Method method : callback.getDeclaredMethods()) {
            for (final Pattern pattern : patterns) {
                if (pattern.matcher(method.getName()).matches()) {
                    waitMethods.add(new MethodCall(method, /* requirement */ null));
                }
            }
        }

        waitUntilCalled(callback, waitMethods);
    }

    /**
     * Wait until the specified methods have been called on the specified object,
     * as specified by any {@link AssertCalled @AssertCalled} annotations. If no
     * {@link AssertCalled @AssertCalled} annotations are found, wait until any method
     * has been called. Only methods belonging to a GeckoSession callback are supported.
     *
     * @param callback Target callback object; must implement an interface under GeckoSession.
     */
    public void waitUntilCalled(final @NonNull Object callback) {
        if (callback instanceof Class<?>) {
            waitUntilCalled((Class<?>) callback, (String[]) null);
            return;
        }

        final List<MethodCall> methodCalls = new ArrayList<>();
        for (final Class<?> ifce : CALLBACK_CLASSES) {
            if (!ifce.isInstance(callback)) {
                continue;
            }
            for (final Method method : ifce.getMethods()) {
                final Method callbackMethod;
                try {
                    callbackMethod = callback.getClass().getMethod(method.getName(),
                                                                   method.getParameterTypes());
                } catch (final NoSuchMethodException e) {
                    throw new RuntimeException(e);
                }
                final AssertCalled ac = getAssertCalled(callbackMethod, callback);
                if (ac != null && ac.value()) {
                    methodCalls.add(new MethodCall(callbackMethod, ac, /* target */ null));
                }
            }
        }

        waitUntilCalled(callback.getClass(), methodCalls);
        forCallbacksDuringWait(callback);
    }

    protected void waitUntilCalled(final @NonNull Class<?> listener,
                                   final @NonNull List<MethodCall> methodCalls) {
        // Make sure all handlers are set though #delegateUntilTestEnd or #delegateDuringNextWait,
        // instead of through GeckoSession directly, so that we can still record calls even with
        // custom handlers set.
        for (final Class<?> ifce : CALLBACK_CLASSES) {
            try {
                assertThat("Callbacks should be set through" +
                           " GeckoSessionTestRule delegate methods",
                           getCallbackGetter(ifce).invoke(mSession), sameInstance(mCallbackProxy));
            } catch (final NoSuchMethodException | IllegalAccessException |
                           InvocationTargetException e) {
                throw new RuntimeException(e.getCause() != null ? e.getCause() : e);
            }
        }

        boolean calledAny = false;
        int index = mLastWaitStart = mLastWaitEnd;

        while (!calledAny || !methodCalls.isEmpty()) {
            while (index >= mCallRecords.size()) {
                loopUntilIdle(mTimeoutMillis);
            }

            final MethodCall recorded = mCallRecords.get(index).methodCall;
            calledAny |= recorded.method.getDeclaringClass().isAssignableFrom(listener);
            index++;

            final int i = methodCalls.indexOf(recorded);
            if (i < 0) {
                continue;
            }

            final MethodCall methodCall = methodCalls.get(i);
            methodCall.incrementCounter();
            if (methodCall.allowUnlimitedCalls() || !methodCall.allowMoreCalls()) {
                methodCalls.remove(i);
            }
        }

        mLastWaitEnd = index;
        mWaitScopeDelegates.clear();
    }

    /**
     * Playback callbacks that were made during the previous wait. For any methods
     * annotated with {@link AssertCalled @AssertCalled}, assert that the callbacks
     * satisfy the specified requirements. If no {@link AssertCalled @AssertCalled}
     * annotations are found, assert any method has been called. Only methods belonging
     * to a GeckoSession callback are supported.
     *
     * @param callback Target callback object; must implement one or more interfaces
     *                 under GeckoSession.
     */
    public void forCallbacksDuringWait(final @NonNull Object callback) {
        final Method[] declaredMethods = callback.getClass().getDeclaredMethods();
        final List<MethodCall> methodCalls = new ArrayList<>(declaredMethods.length);
        for (final Class<?> ifce : CALLBACK_CLASSES) {
            if (!ifce.isInstance(callback)) {
                continue;
            }
            for (final Method method : ifce.getMethods()) {
                final Method callbackMethod;
                try {
                    callbackMethod = callback.getClass().getMethod(method.getName(),
                                                                   method.getParameterTypes());
                } catch (final NoSuchMethodException e) {
                    throw new RuntimeException(e);
                }
                methodCalls.add(new MethodCall(
                        callbackMethod, getAssertCalled(callbackMethod, callback),
                        /* target */ null));
            }
        }

        int order = 0;
        boolean calledAny = false;

        for (int index = mLastWaitStart; index < mLastWaitEnd; index++) {
            final CallRecord record = mCallRecords.get(index);
            if (!record.method.getDeclaringClass().isInstance(callback)) {
                continue;
            }

            final int i = methodCalls.indexOf(record.methodCall);
            assertThat(record.method.getName() + " should be found",
                       i, greaterThanOrEqualTo(0));

            final MethodCall methodCall = methodCalls.get(i);
            assertAllowMoreCalls(methodCall);
            methodCall.incrementCounter();
            assertOrder(methodCall, order);
            order = Math.max(methodCall.getOrder(), order);

            try {
                mCurrentMethodCall = methodCall;
                record.method.invoke(callback, record.args);
            } catch (final IllegalAccessException | InvocationTargetException e) {
                throw new RuntimeException(e.getCause() != null ? e.getCause() : e);
            } finally {
                mCurrentMethodCall = null;
            }
            calledAny = true;
        }

        for (final MethodCall methodCall : methodCalls) {
            assertMatchesCount(methodCall);
            if (methodCall.requirement != null) {
                calledAny = true;
            }
        }

        assertThat("Should have called one of " +
                   Arrays.toString(callback.getClass().getInterfaces()),
                   calledAny, equalTo(true));
    }

    /**
     * Get information about the current call. Only valid during a {@link #forCallbacksDuringWait}
     * callback.
     *
     * @return Call information
     */
    public @NonNull CallInfo getCurrentCall() {
        assertThat("Should be in a method call", mCurrentMethodCall, notNullValue());
        return mCurrentMethodCall.getInfo();
    }

    /**
     * Delegate implemented interfaces to the specified callback object, for the rest of the test.
     * Only GeckoSession callback interfaces are supported. Delegates for {@link
     * #delegateUntilTestEnd} can be temporarily overridden by delegates for {@link
     * #delegateDuringNextWait}.
     *
     * @param callback Callback object, or null to clear all previously-set delegates.
     */
    public void delegateUntilTestEnd(final Object callback) {
        mTestScopeDelegates.delegate(callback);
    }

    /**
     * Delegate implemented interfaces to the specified callback object, during the next wait.
     * Only GeckoSession callback interfaces are supported. Delegates for {@link
     * #delegateDuringNextWait} can temporarily take precedence over delegates for
     * {@link #delegateUntilTestEnd}.
     *
     * @param callback Callback object, or null to clear all previously-set delegates.
     */
    public void delegateDuringNextWait(final Object callback) {
        mWaitScopeDelegates.delegate(callback);
    }
}
