/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test.rule;

import org.mozilla.gecko.gfx.GeckoDisplay;
import org.mozilla.geckoview.BuildConfig;
import org.mozilla.geckoview.GeckoRuntime;
import org.mozilla.geckoview.GeckoRuntimeSettings;
import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoSessionSettings;
import org.mozilla.geckoview.test.rdp.RDPConnection;
import org.mozilla.geckoview.test.rdp.Tab;
import org.mozilla.geckoview.test.util.Callbacks;

import static org.hamcrest.Matchers.*;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.fail;

import org.hamcrest.Matcher;

import org.junit.rules.ErrorCollector;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import android.app.Instrumentation;
import android.graphics.Point;
import android.graphics.SurfaceTexture;
import android.net.LocalSocketAddress;
import android.os.Build;
import android.os.Bundle;
import android.os.Debug;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.MessageQueue;
import android.os.SystemClock;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.UiThreadTestRule;
import android.util.Log;
import android.util.Pair;
import android.view.MotionEvent;
import android.view.Surface;

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
import java.util.Set;
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
    private static final String LOGTAG = "GeckoSessionTestRule";

    private static final long DEFAULT_TIMEOUT_MILLIS = 10000;
    private static final long DEFAULT_ARM_DEVICE_TIMEOUT_MILLIS = 30000;
    private static final long DEFAULT_ARM_EMULATOR_TIMEOUT_MILLIS = 120000;
    private static final long DEFAULT_X86_DEVICE_TIMEOUT_MILLIS = 30000;
    private static final long DEFAULT_X86_EMULATOR_TIMEOUT_MILLIS = 5000;
    private static final long DEFAULT_IDE_DEBUG_TIMEOUT_MILLIS = 86400000;

    public static final String APK_URI_PREFIX = "resource://android/";

    private static final Method sOnLocationChange;
    private static final Method sOnPageStop;

    static {
        try {
            sOnLocationChange = GeckoSession.NavigationDelegate.class.getMethod(
                    "onLocationChange", GeckoSession.class, String.class);
            sOnPageStop = GeckoSession.ProgressDelegate.class.getMethod(
                    "onPageStop", GeckoSession.class, boolean.class);
        } catch (final NoSuchMethodException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Specify the timeout for any of the wait methods, in milliseconds, relative to
     * {@link #DEFAULT_TIMEOUT_MILLIS}. When the default timeout scales to account
     * for differences in the device under test, the timeout value here will be
     * scaled as well. Can be used on classes or methods.
     */
    @Target({ElementType.METHOD, ElementType.TYPE})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface TimeoutMillis {
        long value();
    }

    /**
     * Specify the display size for the GeckoSession in device pixels
     */
    @Target({ElementType.METHOD, ElementType.TYPE})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface WithDisplay {
        int width();
        int height();
    }

    /**
     * Specify that the main session should not be opened at the start of the test.
     */
    @Target({ElementType.METHOD, ElementType.TYPE})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface ClosedSessionAtStart {
        boolean value() default true;
    }

    /**
     * Specify that the test will set a delegate to null when creating a session, rather
     * than setting the delegate to a proxy. The test cannot wait on any delegates that
     * are set to null.
     */
    @Target({ElementType.METHOD, ElementType.TYPE})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface NullDelegate {
        Class<?> value();

        @Target({ElementType.METHOD, ElementType.TYPE})
        @Retention(RetentionPolicy.RUNTIME)
        @interface List {
            NullDelegate[] value();
        }
    }

    /**
     * Specify that the test uses DevTools-enabled APIs, such as {@link #evaluateJS}.
     */
    @Target({ElementType.METHOD, ElementType.TYPE})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface WithDevToolsAPI {
        boolean value() default true;
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
                    } catch (final NoSuchFieldException | IllegalAccessException |
                            ClassCastException e) {
                        settings.setInt((GeckoSessionSettings.Key<Integer>) mKey,
                                        Integer.valueOf(value));
                    }
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
         * @return True if the method must be called if count != 0,
         *         or false if the method must not be called.
         */
        boolean value() default true;

        /**
         * @return The number of calls allowed. Specify -1 to allow any number > 0. Specify 0 to
         *         assert the method is not called, even if value() is true.
         */
        int count() default -1;

        /**
         * @return If called, the order number for each call, or 0 to allow arbitrary
         *         order. If order's length is more than count, extra elements are not used;
         *         if order's length is less than count, the last element is repeated.
         */
        int[] order() default 0;
    }

    public static class TimeoutException extends RuntimeException {
        public TimeoutException(final String detailMessage) {
            super(detailMessage);
        }
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
        public final GeckoSession session;
        public final Method method;
        public final CallRequirement requirement;
        public final Object target;
        private int currentCount;

        public MethodCall(final GeckoSession session, final Method method,
                          final CallRequirement requirement) {
            this(session, method, requirement, /* target */ null);
        }

        /* package */ MethodCall(final GeckoSession session, final Method method,
                                 final AssertCalled annotation, final Object target) {
            this(session, method,
                 (annotation != null) ? new CallRequirement(annotation.value(),
                                                            annotation.count(),
                                                            annotation.order())
                                      : null,
                 /* target */ target);
        }

        /* package */ MethodCall(final GeckoSession session, final Method method,
                                 final CallRequirement requirement, final Object target) {
            this.session = session;
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
                final MethodCall otherCall = (MethodCall) other;
                return (session == null || otherCall.session == null ||
                        session == otherCall.session) &&
                        methodsEqual(method, ((MethodCall) other).method);
            } else if (other instanceof Method) {
                return methodsEqual(method, (Method) other);
            }
            return false;
        }

        @Override
        public int hashCode() {
            return method.hashCode();
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
            return (requirement == null) ? -1 :
                   requirement.allowed ? requirement.count : 0;
        }

        /* package */ void incrementCounter() {
            currentCount++;
        }

        /* package */ int getCurrentCount() {
            return currentCount;
        }

        /* package */ boolean allowUnlimitedCalls() {
            return getCount() == -1;
        }

        /* package */ boolean allowMoreCalls() {
            final int count = getCount();
            return count == -1 || count > currentCount;
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

    protected static class CallRecord {
        public final Method method;
        public final MethodCall methodCall;
        public final Object[] args;

        public CallRecord(final GeckoSession session, final Method method, final Object[] args) {
            this.method = method;
            this.methodCall = new MethodCall(session, method, /* requirement */ null);
            this.args = args;
        }
    }

    protected interface CallRecordHandler {
        boolean handleCall(Method method, Object[] args);
    }

    public class Environment {
        /* package */ Environment() {
        }

        private String getEnvVar(final String name) {
            final int nameLen = name.length();
            final Bundle args = InstrumentationRegistry.getArguments();
            String env = args.getString("env0", null);
            for (int i = 1; env != null; i++) {
                if (env.length() >= nameLen + 1 &&
                        env.startsWith(name) &&
                        env.charAt(nameLen) == '=') {
                    return env.substring(nameLen + 1);
                }
                env = args.getString("env" + i, null);
            }
            return "";
        }

        public boolean isAutomation() {
            return !getEnvVar("MOZ_IN_AUTOMATION").isEmpty();
        }

        public boolean isMultiprocess() {
            return Boolean.valueOf(InstrumentationRegistry.getArguments()
                                                          .getString("use_multiprocess",
                                                                     "true"));
        }

        public boolean isDebugging() {
            return Debug.isDebuggerConnected();
        }

        public boolean isEmulator() {
            return "generic".equals(Build.DEVICE) || Build.DEVICE.startsWith("generic_");
        }

        public boolean isDebugBuild() {
            return BuildConfig.DEBUG_BUILD;
        }

        public String getCPUArch() {
            return BuildConfig.ANDROID_CPU_ARCH;
        }
    }

    protected class CallbackDelegates {
        private final Map<Pair<GeckoSession, Method>, MethodCall> mDelegates = new HashMap<>();
        private int mOrder;

        public void delegate(final @Nullable GeckoSession session,
                             final @NonNull Object callback) {
            for (final Class<?> ifce : CALLBACK_CLASSES) {
                if (!ifce.isInstance(callback)) {
                    continue;
                }
                assertThat("Cannot delegate null-delegate callbacks",
                           ifce, not(isIn(mNullDelegates)));

                for (final Method method : ifce.getMethods()) {
                    final Method callbackMethod;
                    try {
                        callbackMethod = callback.getClass().getMethod(method.getName(),
                                                                       method.getParameterTypes());
                    } catch (final NoSuchMethodException e) {
                        throw new RuntimeException(e);
                    }
                    final MethodCall call = new MethodCall(
                            session, callbackMethod,
                            getAssertCalled(callbackMethod, callback), callback);
                    mDelegates.put(new Pair<>(session, method), call);
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

        public MethodCall prepareMethodCall(final GeckoSession session, final Method method) {
            MethodCall call = mDelegates.get(new Pair<>(session, method));
            if (call == null && session != null) {
                call = mDelegates.get(new Pair<>((GeckoSession) null, method));
            }
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

    private static GeckoRuntime sRuntime;
    private static RDPConnection sRDPConnection;
    private static long sLongestWait;

    public final Environment env = new Environment();

    protected final Instrumentation mInstrumentation =
            InstrumentationRegistry.getInstrumentation();
    protected final GeckoSessionSettings mDefaultSettings;
    protected final Set<GeckoSession> mSubSessions = new HashSet<>();

    protected ErrorCollector mErrorCollector;
    protected GeckoSession mMainSession;
    protected Object mCallbackProxy;
    protected Set<Class<?>> mNullDelegates;
    protected List<CallRecord> mCallRecords;
    protected CallRecordHandler mCallRecordHandler;
    protected CallbackDelegates mWaitScopeDelegates;
    protected CallbackDelegates mTestScopeDelegates;
    protected int mLastWaitStart;
    protected int mLastWaitEnd;
    protected MethodCall mCurrentMethodCall;
    protected long mTimeoutMillis;
    protected Point mDisplaySize;
    protected SurfaceTexture mDisplayTexture;
    protected Surface mDisplaySurface;
    protected GeckoDisplay mDisplay;
    protected boolean mClosedSession;
    protected boolean mWithDevTools;
    protected Map<GeckoSession, Tab> mRDPTabs;

    public GeckoSessionTestRule() {
        mDefaultSettings = new GeckoSessionSettings();
        mDefaultSettings.setBoolean(GeckoSessionSettings.USE_MULTIPROCESS, env.isMultiprocess());
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

    /**
     * Assert a condition with junit.Assert or an error collector.
     *
     * @param reason Reason string
     * @param value Value to check
     * @param matcher Matcher for checking the value
     */
    public <T> void checkThat(final String reason, final T value, final Matcher<? super T> matcher) {
        if (mErrorCollector != null) {
            mErrorCollector.checkThat(reason, value, matcher);
        } else {
            assertThat(reason, value, matcher);
        }
    }

    private void assertAllowMoreCalls(final MethodCall call) {
        final int count = call.getCount();
        if (count != -1) {
            checkThat(call.method.getName() + " call count should be within limit",
                      call.getCurrentCount() + 1, lessThanOrEqualTo(count));
        }
    }

    private void assertOrder(final MethodCall call, final int order) {
        final int newOrder = call.getOrder();
        if (newOrder != 0) {
            checkThat(call.method.getName() + " should be in order",
                      newOrder, greaterThanOrEqualTo(order));
        }
    }

    private void assertMatchesCount(final MethodCall call) {
        if (call.requirement == null) {
            return;
        }
        final int count = call.getCount();
        if (count == 0) {
            checkThat(call.method.getName() + " should not be called",
                      call.getCurrentCount(), equalTo(0));
        } else if (count == -1) {
            checkThat(call.method.getName() + " should be called",
                      call.getCurrentCount(), greaterThan(0));
        } else {
            checkThat(call.method.getName() + " should be called specified number of times",
                      call.getCurrentCount(), equalTo(count));
        }
    }

    /**
     * Get the session set up for the current test.
     *
     * @return GeckoSession object.
     */
    public @NonNull GeckoSession getSession() {
        return mMainSession;
    }

    /**
     * Get the runtime set up for the current test.
     *
     * @return GeckoRuntime object.
     */
    public @NonNull GeckoRuntime getRuntime() {
        return sRuntime;
    }

    protected static Method getCallbackSetter(final @NonNull Class<?> cls)
            throws NoSuchMethodException {
        return GeckoSession.class.getMethod("set" + cls.getSimpleName(), cls);
    }

    protected static Method getCallbackGetter(final @NonNull Class<?> cls)
            throws NoSuchMethodException {
        return GeckoSession.class.getMethod("get" + cls.getSimpleName());
    }

    private void addNullDelegate(final Class<?> delegate) {
        if (!Callbacks.class.equals(delegate.getDeclaringClass())) {
            assertThat("Null-delegate must be valid interface class",
                       delegate, isIn(CALLBACK_CLASSES));
            mNullDelegates.add(delegate);
            return;
        }
        for (final Class<?> ifce : delegate.getInterfaces()) {
            addNullDelegate(ifce);
        }
    }

    protected void applyAnnotations(final Collection<Annotation> annotations,
                                    final GeckoSessionSettings settings) {
        for (final Annotation annotation : annotations) {
            if (TimeoutMillis.class.equals(annotation.annotationType())) {
                // Scale timeout based on the default timeout to account for the device under test.
                final long value = ((TimeoutMillis) annotation).value();
                final long timeout = value * getScaledTimeoutMillis() / DEFAULT_TIMEOUT_MILLIS;
                mTimeoutMillis = Math.max(timeout, 1000);
            } else if (Setting.class.equals(annotation.annotationType())) {
                ((Setting) annotation).key().set(settings, ((Setting) annotation).value());
            } else if (Setting.List.class.equals(annotation.annotationType())) {
                for (final Setting setting : ((Setting.List) annotation).value()) {
                    setting.key().set(settings, setting.value());
                }
            } else if (NullDelegate.class.equals(annotation.annotationType())) {
                addNullDelegate(((NullDelegate) annotation).value());
            } else if (NullDelegate.List.class.equals(annotation.annotationType())) {
                for (final NullDelegate nullDelegate : ((NullDelegate.List) annotation).value()) {
                    addNullDelegate(nullDelegate.value());
                }
            } else if (WithDisplay.class.equals(annotation.annotationType())) {
                final WithDisplay displaySize = (WithDisplay)annotation;
                mDisplaySize = new Point(displaySize.width(), displaySize.height());
            } else if (ClosedSessionAtStart.class.equals(annotation.annotationType())) {
                mClosedSession = ((ClosedSessionAtStart) annotation).value();
            } else if (WithDevToolsAPI.class.equals(annotation.annotationType())) {
                mWithDevTools = ((WithDevToolsAPI) annotation).value();
            }
        }
    }

    private static RuntimeException unwrapRuntimeException(final Throwable e) {
        final Throwable cause = e.getCause();
        if (cause != null && cause instanceof RuntimeException) {
            return (RuntimeException) cause;
        } else if (e instanceof RuntimeException) {
            return (RuntimeException) e;
        }

        return new RuntimeException(cause != null ? cause : e);
    }

    private long getScaledTimeoutMillis() {
        if ("x86".equals(env.getCPUArch())) {
            return env.isEmulator() ? DEFAULT_X86_EMULATOR_TIMEOUT_MILLIS
                                    : DEFAULT_X86_DEVICE_TIMEOUT_MILLIS;
        }
        return env.isEmulator() ? DEFAULT_ARM_EMULATOR_TIMEOUT_MILLIS
                                : DEFAULT_ARM_DEVICE_TIMEOUT_MILLIS;
    }

    private long getDefaultTimeoutMillis() {
        return env.isDebugging() ? DEFAULT_IDE_DEBUG_TIMEOUT_MILLIS
                                 : getScaledTimeoutMillis();
    }

    protected void prepareStatement(final Description description) throws Throwable {
        final GeckoSessionSettings settings = new GeckoSessionSettings(mDefaultSettings);
        mTimeoutMillis = getDefaultTimeoutMillis();
        mNullDelegates = new HashSet<>();
        mClosedSession = false;
        mWithDevTools = false;

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
        if (mWithDevTools) {
            mRDPTabs = new HashMap<>();
        }

        final InvocationHandler recorder = new InvocationHandler() {
            @Override
            public Object invoke(final Object proxy, final Method method,
                                 final Object[] args) {
                boolean ignore = false;
                MethodCall call = null;

                if (Object.class.equals(method.getDeclaringClass())) {
                    ignore = true;
                } else if (mCallRecordHandler != null) {
                    ignore = mCallRecordHandler.handleCall(method, args);
                }

                if (!ignore) {
                    assertThat("Callbacks must be on UI thread",
                               Looper.myLooper(), equalTo(Looper.getMainLooper()));
                    assertThat("Callback first argument must be session object",
                               args, arrayWithSize(greaterThan(0)));
                    assertThat("Callback first argument must be session object",
                               args[0], instanceOf(GeckoSession.class));

                    final GeckoSession session = (GeckoSession) args[0];
                    records.add(new CallRecord(session, method, args));

                    call = waitDelegates.prepareMethodCall(session, method);
                    if (call == null) {
                        call = testDelegates.prepareMethodCall(session, method);
                    }
                }

                try {
                    mCurrentMethodCall = call;
                    return method.invoke((call != null) ? call.target
                                                        : Callbacks.Default.INSTANCE, args);
                } catch (final IllegalAccessException | InvocationTargetException e) {
                    throw unwrapRuntimeException(e);
                } finally {
                    mCurrentMethodCall = null;
                }
            }
        };

        final Class<?>[] classes = CALLBACK_CLASSES.toArray(new Class<?>[CALLBACK_CLASSES.size()]);
        mCallbackProxy = Proxy.newProxyInstance(GeckoSession.class.getClassLoader(),
                                                classes, recorder);

        if (sRuntime == null) {
            final GeckoRuntimeSettings.Builder runtimeSettingsBuilder =
                new GeckoRuntimeSettings.Builder();
            runtimeSettingsBuilder.arguments(new String[] { "-purgecaches" })
                    .extras(InstrumentationRegistry.getArguments())
                    .nativeCrashReportingEnabled(true)
                    .javaCrashReportingEnabled(true);

            sRuntime = GeckoRuntime.create(
                InstrumentationRegistry.getTargetContext(),
                runtimeSettingsBuilder.build());
        }

        sRuntime.getSettings().setRemoteDebuggingEnabled(mWithDevTools);

        mMainSession = new GeckoSession(settings);
        prepareSession(mMainSession);

        if (mDisplaySize != null) {
            mDisplayTexture = new SurfaceTexture(0);
            mDisplaySurface = new Surface(mDisplayTexture);
            mDisplay = mMainSession.acquireDisplay();
            mDisplay.surfaceChanged(mDisplaySurface, mDisplaySize.x, mDisplaySize.y);
        }

        if (!mClosedSession) {
            openSession(mMainSession);
        }
    }

    protected void prepareSession(final GeckoSession session) throws Throwable {
        for (final Class<?> cls : CALLBACK_CLASSES) {
            if (!mNullDelegates.contains(cls)) {
                getCallbackSetter(cls).invoke(session, mCallbackProxy);
            }
        }
    }

    /**
     * Call open() on a session, and ensure it's ready for use by the test. In particular,
     * remove any extra calls recorded as part of opening the session.
     *
     * @param session Session to open.
     */
    public void openSession(final GeckoSession session) {
        session.open(sRuntime);
        waitForInitialLoad(session);

        if (mWithDevTools) {
            if (sRDPConnection == null) {
                final String dataDir = InstrumentationRegistry.getTargetContext()
                                                              .getApplicationInfo().dataDir;
                final LocalSocketAddress address = new LocalSocketAddress(
                        dataDir + "/firefox-debugger-socket",
                        LocalSocketAddress.Namespace.FILESYSTEM);
                sRDPConnection = new RDPConnection(address);
                sRDPConnection.setTimeout((int) mTimeoutMillis);
            }
            final Tab tab = sRDPConnection.getMostRecentTab();
            tab.attach();
            mRDPTabs.put(session, tab);
        }
    }

    private void waitForInitialLoad(final GeckoSession session) {
        // We receive an initial about:blank load; don't expose that to the test.
        // The about:blank load is bounded by onLocationChange and onPageStop calls,
        // so find the first about:blank onLocationChange, then the next onPageStop,
        // and ignore everything in-between from that session.

        try {
            // We cannot detect initial page load without progress delegate.
            assertThat("ProgressDelegate cannot be null-delegate when opening session",
                       GeckoSession.ProgressDelegate.class, not(isIn(mNullDelegates)));

            // If navigation delegate is a null-delegate, instead of looking for
            // onLocationChange(), start with the first call that targets this session.
            final boolean nullNavigation = mNullDelegates.contains(
                    GeckoSession.NavigationDelegate.class);

            mCallRecordHandler = new CallRecordHandler() {
                private boolean mFoundStart = false;

                @Override
                public boolean handleCall(final Method method, final Object[] args) {
                    if (!mFoundStart && session.equals(args[0]) && (nullNavigation ||
                            (sOnLocationChange.equals(method) && "about:blank".equals(args[1])))) {
                        mFoundStart = true;
                        return true;
                    } else if (mFoundStart && session.equals(args[0])) {
                        if (sOnPageStop.equals(method)) {
                            mCallRecordHandler = null;
                        }
                        return true;
                    }
                    return false;
                }
            };

            do {
                loopUntilIdle(getDefaultTimeoutMillis());
            } while (mCallRecordHandler != null);

        } finally {
            mCallRecordHandler = null;
        }
    }

    /**
     * Internal method to perform callback checks at the end of a test.
     */
    public void performTestEndCheck() {
        mWaitScopeDelegates.clear();
        mTestScopeDelegates.clear();
    }

    protected void cleanupSession(final GeckoSession session) {
        final Tab tab = (mRDPTabs != null) ? mRDPTabs.get(session) : null;
        if (tab != null) {
            tab.detach();
            mRDPTabs.remove(session);
        }
        if (session.isOpen()) {
            session.close();
        }
    }

    protected void cleanupStatement() throws Throwable {
        for (final GeckoSession session : mSubSessions) {
            cleanupSession(session);
        }
        cleanupSession(mMainSession);

        if (mDisplay != null) {
            mDisplay.surfaceDestroyed();
            mMainSession.releaseDisplay(mDisplay);
            mDisplay = null;
            mDisplaySurface.release();
            mDisplaySurface = null;
            mDisplayTexture.release();
            mDisplayTexture = null;
        }

        mMainSession = null;
        mCallbackProxy = null;
        mNullDelegates = null;
        mCallRecords = null;
        mWaitScopeDelegates = null;
        mTestScopeDelegates = null;
        mLastWaitStart = 0;
        mLastWaitEnd = 0;
        mTimeoutMillis = 0;
        mRDPTabs = null;
    }

    @Override
    public Statement apply(final Statement base, final Description description) {
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                try {
                    prepareStatement(description);
                    base.evaluate();
                    performTestEndCheck();
                } finally {
                    cleanupStatement();
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
                throw new TimeoutException("Timed out after " + timeout + "ms");
            }
        };
        if (timeout > 0) {
            handler.postDelayed(timeoutRunnable, timeout);
        } else {
            queue.addIdleHandler(idleHandler);
        }

        final long startTime = SystemClock.uptimeMillis();
        try {
            while (true) {
                final Message msg;
                try {
                    msg = (Message) getNextMessage.invoke(queue);
                } catch (final IllegalAccessException | InvocationTargetException e) {
                    throw unwrapRuntimeException(e);
                }
                if (msg.getTarget() == handler && msg.obj == handler) {
                    // Our idle signal.
                    break;
                } else if (msg.getTarget() == null) {
                    looper.quit();
                    return;
                }
                msg.getTarget().dispatchMessage(msg);

                if (timeout > 0) {
                    handler.removeCallbacks(timeoutRunnable);
                    queue.addIdleHandler(idleHandler);
                }
            }

            final long waitDuration = SystemClock.uptimeMillis() - startTime;
            if (waitDuration > sLongestWait) {
                sLongestWait = waitDuration;
                Log.i(LOGTAG, "New longest wait: " + waitDuration + "ms");
            }
        } finally {
            if (timeout > 0) {
                handler.removeCallbacks(timeoutRunnable);
            }
        }
    }

    /**
     * Wait until a page load has finished on any session. A session must have started a
     * page load since the last wait, or this method will wait indefinitely.
     */
    public void waitForPageStop() {
        waitForPageStop(/* session */ null);
    }

    /**
     * Wait until a page load has finished. The session must have started a page load since
     * the last wait, or this method will wait indefinitely.
     *
     * @param session Session to wait on, or null to wait on any session.
     */
    public void waitForPageStop(final GeckoSession session) {
        waitForPageStops(session, /* count */ 1);
    }

    /**
     * Wait until a page load has finished on any session. A session must have started a
     * page load since the last wait, or this method will wait indefinitely.
     *
     * @param count Number of page loads to wait for.
     */
    public void waitForPageStops(final int count) {
        waitForPageStops(/* session */ null, count);
    }

    /**
     * Wait until a page load has finished. The session must have started a page load since
     * the last wait, or this method will wait indefinitely.
     *
     * @param session Session to wait on, or null to wait on any session.
     * @param count Number of page loads to wait for.
     */
    public void waitForPageStops(final GeckoSession session, final int count) {
        final List<MethodCall> methodCalls = new ArrayList<>(1);
        methodCalls.add(new MethodCall(session, sOnPageStop,
                new CallRequirement(/* allowed */ true, count, null)));

        waitUntilCalled(session, GeckoSession.ProgressDelegate.class, methodCalls);
    }

    /**
     * Wait until the specified methods have been called on the specified callback
     * interface for any session. If no methods are specified, wait until any method has
     * been called.
     *
     * @param callback Target callback interface; must be an interface under GeckoSession.
     * @param methods List of methods to wait on; use empty or null or wait on any method.
     */
    public void waitUntilCalled(final @NonNull KClass<?> callback,
                                final @Nullable String... methods) {
        waitUntilCalled(/* session */ null, callback, methods);
    }

    /**
     * Wait until the specified methods have been called on the specified callback
     * interface. If no methods are specified, wait until any method has been called.
     *
     * @param session Session to wait on, or null to wait on any session.
     * @param callback Target callback interface; must be an interface under GeckoSession.
     * @param methods List of methods to wait on; use empty or null or wait on any method.
     */
    public void waitUntilCalled(final @Nullable GeckoSession session,
                                final @NonNull KClass<?> callback,
                                final @Nullable String... methods) {
        waitUntilCalled(session, JvmClassMappingKt.getJavaClass(callback), methods);
    }

    /**
     * Wait until the specified methods have been called on the specified callback
     * interface for any session. If no methods are specified, wait until any method has
     * been called.
     *
     * @param callback Target callback interface; must be an interface under GeckoSession.
     * @param methods List of methods to wait on; use empty or null or wait on any method.
     */
    public void waitUntilCalled(final @NonNull Class<?> callback,
                                final @Nullable String... methods) {
        waitUntilCalled(/* session */ null, callback, methods);
    }

    /**
     * Wait until the specified methods have been called on the specified callback
     * interface. If no methods are specified, wait until any method has been called.
     *
     * @param session Session to wait on, or null to wait on any session.
     * @param callback Target callback interface; must be an interface under GeckoSession.
     * @param methods List of methods to wait on; use empty or null or wait on any method.
     */
    public void waitUntilCalled(final @Nullable GeckoSession session,
                                final @NonNull Class<?> callback,
                                final @Nullable String... methods) {
        final int length = (methods != null) ? methods.length : 0;
        final Pattern[] patterns = new Pattern[length];
        for (int i = 0; i < length; i++) {
            patterns[i] = Pattern.compile(methods[i]);
        }

        final List<MethodCall> waitMethods = new ArrayList<>();
        boolean isSessionCallback = false;

        for (final Class<?> ifce : CALLBACK_CLASSES) {
            if (!ifce.isAssignableFrom(callback)) {
                continue;
            }
            for (final Method method : ifce.getMethods()) {
                for (final Pattern pattern : patterns) {
                    if (!pattern.matcher(method.getName()).matches()) {
                        continue;
                    }
                    waitMethods.add(new MethodCall(session, method,
                                                   /* requirement */ null));
                    break;
                }
            }
            isSessionCallback = true;
        }

        assertThat("Class should be a GeckoSession interface",
                   isSessionCallback, equalTo(true));

        waitUntilCalled(session, callback, waitMethods);
    }

    /**
     * Wait until the specified methods have been called on the specified object for any
     * session, as specified by any {@link AssertCalled @AssertCalled} annotations. If no
     * {@link AssertCalled @AssertCalled} annotations are found, wait until any method has
     * been called. Only methods belonging to a GeckoSession callback are supported.
     *
     * @param callback Target callback object; must implement an interface under GeckoSession.
     */
    public void waitUntilCalled(final @NonNull Object callback) {
        waitUntilCalled(/* session */ null, callback);
    }

    /**
     * Wait until the specified methods have been called on the specified object,
     * as specified by any {@link AssertCalled @AssertCalled} annotations. If no
     * {@link AssertCalled @AssertCalled} annotations are found, wait until any method
     * has been called. Only methods belonging to a GeckoSession callback are supported.
     *
     * @param session Session to wait on, or null to wait on any session.
     * @param callback Target callback object; must implement an interface under GeckoSession.
     */
    public void waitUntilCalled(final @Nullable GeckoSession session,
                                final @NonNull Object callback) {
        if (callback instanceof Class<?>) {
            waitUntilCalled(session, (Class<?>) callback, (String[]) null);
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
                    methodCalls.add(new MethodCall(session, method,
                                                   ac, /* target */ null));
                }
            }
        }

        waitUntilCalled(session, callback.getClass(), methodCalls);
        forCallbacksDuringWait(session, callback);
    }

    protected void waitUntilCalled(final @Nullable GeckoSession session,
                                   final @NonNull Class<?> delegate,
                                   final @NonNull List<MethodCall> methodCalls) {
        if (session != null && !session.equals(mMainSession)) {
            assertThat("Session should be wrapped through wrapSession",
                       session, isIn(mSubSessions));
        }

        // Make sure all handlers are set though #delegateUntilTestEnd or #delegateDuringNextWait,
        // instead of through GeckoSession directly, so that we can still record calls even with
        // custom handlers set.
        for (final Class<?> ifce : CALLBACK_CLASSES) {
            final Object callback;
            try {
                callback = getCallbackGetter(ifce).invoke(session == null ? mMainSession : session);
            } catch (final NoSuchMethodException | IllegalAccessException |
                    InvocationTargetException e) {
                throw unwrapRuntimeException(e);
            }
            if (mNullDelegates.contains(ifce)) {
                // Null-delegates are initially null but are allowed to be any value.
                continue;
            }
            assertThat(ifce.getSimpleName() + " callbacks should be " +
                       "accessed through GeckoSessionTestRule delegate methods",
                       callback, sameInstance(mCallbackProxy));
        }

        if (methodCalls.isEmpty()) {
            // Waiting for any call on `delegate`; make sure it doesn't contain any null-delegates.
            for (final Class<?> ifce : mNullDelegates) {
                assertThat("Cannot wait on null-delegate callbacks",
                           delegate, not(typeCompatibleWith(ifce)));
            }
        } else {
            // Waiting for particular calls; make sure those calls aren't from a null-delegate.
            for (final MethodCall call : methodCalls) {
                assertThat("Cannot wait on null-delegate callbacks",
                           call.method.getDeclaringClass(), not(isIn(mNullDelegates)));
            }
        }

        boolean calledAny = false;
        int index = mLastWaitStart = mLastWaitEnd;

        while (!calledAny || !methodCalls.isEmpty()) {
            while (index >= mCallRecords.size()) {
                loopUntilIdle(mTimeoutMillis);
            }

            final MethodCall recorded = mCallRecords.get(index).methodCall;
            calledAny |= recorded.method.getDeclaringClass().isAssignableFrom(delegate);
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
     * Playback callbacks that were made on all sessions during the previous wait. For any
     * methods annotated with {@link AssertCalled @AssertCalled}, assert that the
     * callbacks satisfy the specified requirements. If no {@link AssertCalled
     * @AssertCalled} annotations are found, assert any method has been called. Only
     * methods belonging to a GeckoSession callback are supported.
     *
     * @param callback Target callback object; must implement one or more interfaces
     *                 under GeckoSession.
     */
    public void forCallbacksDuringWait(final @NonNull Object callback) {
        forCallbacksDuringWait(/* session */ null, callback);
    }

    /**
     * Playback callbacks that were made during the previous wait. For any methods
     * annotated with {@link AssertCalled @AssertCalled}, assert that the callbacks
     * satisfy the specified requirements. If no {@link AssertCalled @AssertCalled}
     * annotations are found, assert any method has been called. Only methods belonging
     * to a GeckoSession callback are supported.
     *
     * @param session  Target session object, or null to playback all sessions.
     * @param callback Target callback object; must implement one or more interfaces
     *                 under GeckoSession.
     */
    public void forCallbacksDuringWait(final @Nullable GeckoSession session,
                                       final @NonNull Object callback) {
        final Method[] declaredMethods = callback.getClass().getDeclaredMethods();
        final List<MethodCall> methodCalls = new ArrayList<>(declaredMethods.length);
        boolean assertingAnyCall = true;
        Class<?> foundNullDelegate = null;

        for (final Class<?> ifce : CALLBACK_CLASSES) {
            if (!ifce.isInstance(callback)) {
                continue;
            }
            if (mNullDelegates.contains(ifce)) {
                foundNullDelegate = ifce;
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
                        session, callbackMethod, getAssertCalled(callbackMethod, callback),
                        /* target */ null);
                methodCalls.add(call);

                if (call.requirement != null) {
                    if (foundNullDelegate == ifce) {
                        fail("Cannot assert on null-delegate " + ifce.getSimpleName());
                    }
                    assertingAnyCall = false;
                }
            }
        }

        if (assertingAnyCall && foundNullDelegate != null) {
            fail("Cannot assert on null-delegate " + foundNullDelegate.getSimpleName());
        }

        int order = 0;
        boolean calledAny = false;

        for (int index = mLastWaitStart; index < mLastWaitEnd; index++) {
            final CallRecord record = mCallRecords.get(index);
            if (!record.method.getDeclaringClass().isInstance(callback) ||
                    (session != null && record.args[0] != session)) {
                continue;
            }

            final int i = methodCalls.indexOf(record.methodCall);
            checkThat(record.method.getName() + " should be found",
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
                throw unwrapRuntimeException(e);
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

        checkThat("Should have called one of " +
                  Arrays.toString(callback.getClass().getInterfaces()),
                  calledAny, equalTo(true));
    }

    /**
     * Get information about the current call. Only valid during a {@link
     * #forCallbacksDuringWait}, {@link #delegateDuringNextWait}, or {@link
     * #delegateUntilTestEnd} callback.
     *
     * @return Call information
     */
    public @NonNull CallInfo getCurrentCall() {
        assertThat("Should be in a method call", mCurrentMethodCall, notNullValue());
        return mCurrentMethodCall.getInfo();
    }

    /**
     * Delegate implemented interfaces to the specified callback object for all sessions,
     * for the rest of the test.  Only GeckoSession callback interfaces are supported.
     * Delegates for {@code delegateUntilTestEnd} can be temporarily overridden by
     * delegates for {@link #delegateDuringNextWait}.
     *
     * @param callback Callback object, or null to clear all previously-set delegates.
     */
    public void delegateUntilTestEnd(final @NonNull Object callback) {
        delegateUntilTestEnd(/* session */ null, callback);
    }

    /**
     * Delegate implemented interfaces to the specified callback object, for the rest of the test.
     * Only GeckoSession callback interfaces are supported. Delegates for {@link
     * #delegateUntilTestEnd} can be temporarily overridden by delegates for {@link
     * #delegateDuringNextWait}.
     *
     * @param session Session to target, or null to target all sessions.
     * @param callback Callback object, or null to clear all previously-set delegates.
     */
    public void delegateUntilTestEnd(final @Nullable GeckoSession session,
                                     final @NonNull Object callback) {
        mTestScopeDelegates.delegate(session, callback);
    }

    /**
     * Delegate implemented interfaces to the specified callback object for all sessions,
     * during the next wait.  Only GeckoSession callback interfaces are supported.
     * Delegates for {@code delegateDuringNextWait} can temporarily take precedence over
     * delegates for {@link #delegateUntilTestEnd}.
     *
     * @param callback Callback object, or null to clear all previously-set delegates.
     */
    public void delegateDuringNextWait(final @NonNull Object callback) {
        delegateDuringNextWait(/* session */ null, callback);
    }

    /**
     * Delegate implemented interfaces to the specified callback object, during the next wait.
     * Only GeckoSession callback interfaces are supported. Delegates for {@link
     * #delegateDuringNextWait} can temporarily take precedence over delegates for
     * {@link #delegateUntilTestEnd}.
     *
     * @param session Session to target, or null to target all sessions.
     * @param callback Callback object, or null to clear all previously-set delegates.
     */
    public void delegateDuringNextWait(final @Nullable GeckoSession session,
                                       final @NonNull Object callback) {
        mWaitScopeDelegates.delegate(session, callback);
    }

    /**
     * Synthesize a tap event at the specified location using the main session.
     * The session must have been created with a display.
     *
     * @param session Target session
     * @param x X coordinate
     * @param y Y coordinate
     */
    public void synthesizeTap(final @NonNull GeckoSession session,
                              final int x, final int y) {
        final long downTime = SystemClock.uptimeMillis();
        final MotionEvent down = MotionEvent.obtain(
                downTime, SystemClock.uptimeMillis(), MotionEvent.ACTION_DOWN, x, y, 0);
        session.getPanZoomController().onTouchEvent(down);

        final MotionEvent up = MotionEvent.obtain(
                downTime, SystemClock.uptimeMillis(), MotionEvent.ACTION_UP, x, y, 0);
        session.getPanZoomController().onTouchEvent(up);
    }

    /**
     * Initialize and keep track of the specified session within the test rule. The
     * session is automatically cleaned up at the end of the test.
     *
     * @param session Session to keep track of.
     * @return Same session
     */
    public GeckoSession wrapSession(final GeckoSession session) {
        try {
            mSubSessions.add(session);
            prepareSession(session);
        } catch (final Throwable e) {
            throw unwrapRuntimeException(e);
        }
        return session;
    }

    private GeckoSession createSession(final GeckoSessionSettings settings,
                                       final boolean open) {
        final GeckoSession session = wrapSession(new GeckoSession(settings));
        if (open) {
            openSession(session);
        }
        return session;
    }

    /**
     * Create a new, opened session using the main session settings.
     *
     * @return New session.
     */
    public GeckoSession createOpenSession() {
        return createSession(mMainSession.getSettings(), /* open */ true);
    }

    /**
     * Create a new, opened session using the specified settings.
     *
     * @param settings Settings for the new session.
     * @return New session.
     */
    public GeckoSession createOpenSession(final GeckoSessionSettings settings) {
        return createSession(settings, /* open */ true);
    }

    /**
     * Create a new, closed session using the specified settings.
     *
     * @return New session.
     */
    public GeckoSession createClosedSession() {
        return createSession(mMainSession.getSettings(), /* open */ false);
    }

    /**
     * Create a new, closed session using the specified settings.
     *
     * @param settings Settings for the new session.
     * @return New session.
     */
    public GeckoSession createClosedSession(final GeckoSessionSettings settings) {
        return createSession(settings, /* open */ false);
    }

    /**
     * Return a value from the given array indexed by the current call counter. Only valid
     * during a {@link #forCallbacksDuringWait}, {@link #delegateDuringNextWait}, or
     * {@link #delegateUntilTestEnd} callback.
     * <p><p>
     * Asserts that {@code foo} is equal to {@code "bar"} during the first call and {@code
     * "baz"} during the second call:
     * <pre>{@code assertThat("Foo should match", foo, equalTo(forEachCall("bar",
     * "baz")));}</pre>
     *
     * @param values Input array
     * @return Value from input array indexed by the current call counter.
     */
    @SafeVarargs
    public final <T> T forEachCall(T... values) {
        assertThat("Should be in a method call", mCurrentMethodCall, notNullValue());
        return values[Math.min(mCurrentMethodCall.getCurrentCount(), values.length) - 1];
    }

    /**
     * Evaluate a JavaScript expression in the context of the target page and return the result.
     * RDP must be enabled first using the {@link WithDevToolsAPI} annotation. String, number, and
     * boolean results are converted to Java values. Undefined and null results are returned as
     * null. Objects are returned as Map instances. Arrays are returned as Object[] instances.
     *
     * @param session Session containing the target page.
     * @param js JavaScript expression.
     * @return Result of evaluating the expression.
     */
    public Object evaluateJS(final @NonNull GeckoSession session, final @NonNull String js) {
        assertThat("Must enable RDP using @WithDevToolsAPI", mRDPTabs, notNullValue());

        final Tab tab = mRDPTabs.get(session);
        assertThat("Session should have tab object", tab, notNullValue());
        return tab.getConsole().evaluateJS(js);
    }
}
