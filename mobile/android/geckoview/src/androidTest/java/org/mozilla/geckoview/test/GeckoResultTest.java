package org.mozilla.geckoview.test;

import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.geckoview.GeckoResult;
import org.mozilla.geckoview.test.util.Environment;
import org.mozilla.geckoview.test.util.UiThreadUtils;

import android.os.Handler;
import android.os.Looper;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.MediumTest;
import android.support.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.SynchronousQueue;
import java.util.concurrent.TimeoutException;

import static org.hamcrest.Matchers.*;
import static org.junit.Assert.assertThat;

@RunWith(AndroidJUnit4.class)
@MediumTest
public class GeckoResultTest {
    private static class MockException extends RuntimeException {
    }

    private boolean mDone;

    private final Environment mEnv = new Environment();

    private void waitUntilDone() {
        assertThat("We should not be done", mDone, equalTo(false));
        UiThreadUtils.waitForCondition(() -> mDone, mEnv.getDefaultTimeoutMillis());
    }

    private void done() {
        UiThreadUtils.HANDLER.post(() -> mDone = true);
    }

    @Before
    public void setup() {
        mDone = false;
    }

    @Test
    @UiThreadTest
    public void thenWithResult() {
        GeckoResult.fromValue(42).accept(value -> {
            assertThat("Value should match", value, equalTo(42));
            done();
        });

        waitUntilDone();
    }

    @Test
    @UiThreadTest
    public void thenWithException() {
        final Throwable boom = new Exception("boom");
        GeckoResult.fromException(boom).accept(null, error -> {
            assertThat("Exception should match", error, equalTo(boom));
            done();
        });

        waitUntilDone();
    }

    @Test(expected = IllegalArgumentException.class)
    @UiThreadTest
    public void thenNoListeners() {
        GeckoResult.fromValue(42).then(null, null);
    }

    @Test
    @UiThreadTest
    public void testCopy() {
        final GeckoResult<Integer> result = new GeckoResult<>(GeckoResult.fromValue(42));
        result.accept(value -> {
            assertThat("Value should match", value, equalTo(42));
            done();
        });

        waitUntilDone();
    }

    @Test(expected = IllegalStateException.class)
    @UiThreadTest
    public void completeMultiple() {
        final GeckoResult<Integer> deferred = new GeckoResult<>();
        deferred.complete(42);
        deferred.complete(43);
    }

    @Test(expected = IllegalStateException.class)
    @UiThreadTest
    public void completeMultipleExceptions() {
        final GeckoResult<Integer> deferred = new GeckoResult<>();
        deferred.completeExceptionally(new Exception("boom"));
        deferred.completeExceptionally(new Exception("boom again"));
    }

    @Test(expected = IllegalStateException.class)
    @UiThreadTest
    public void completeMixed() {
        final GeckoResult<Integer> deferred = new GeckoResult<>();
        deferred.complete(42);
        deferred.completeExceptionally(new Exception("boom again"));
    }

    @Test(expected = IllegalArgumentException.class)
    @UiThreadTest
    public void completeExceptionallyNull() {
        new GeckoResult<Integer>().completeExceptionally(null);
    }

    @Test
    @UiThreadTest
    public void completeThreaded() {
        final GeckoResult<Integer> deferred = new GeckoResult<>();
        final Thread thread = new Thread(() -> deferred.complete(42));

        deferred.accept(value -> {
            assertThat("Value should match", value, equalTo(42));
            ThreadUtils.assertOnUiThread();
            done();
        });

        thread.start();
        waitUntilDone();
    }

    @Test
    @UiThreadTest
    public void dispatchOnInitialThread() throws InterruptedException {
        final Thread thread = new Thread(() -> {
            Looper.prepare();
            final Thread dispatchThread = Thread.currentThread();

            GeckoResult.fromValue(42).accept(value -> {
                assertThat("Thread should match", Thread.currentThread(),
                        equalTo(dispatchThread));
                Looper.myLooper().quit();
            });

            Looper.loop();
        });

        thread.start();
        thread.join();
    }

    @Test
    @UiThreadTest
    public void completeExceptionallyThreaded() {
        final GeckoResult<Integer> deferred = new GeckoResult<>();
        final Throwable boom = new Exception("boom");
        final Thread thread = new Thread(() -> deferred.completeExceptionally(boom));

        deferred.exceptionally(error -> {
            assertThat("Exception should match", error, equalTo(boom));
            ThreadUtils.assertOnUiThread();
            done();
            return null;
        });

        thread.start();
        waitUntilDone();
    }

    @UiThreadTest
    @Test
    public void resultChaining() {
        assertThat("We're on the UI thread", Thread.currentThread(), equalTo(Looper.getMainLooper().getThread()));

        GeckoResult.fromValue(42).then(value -> {
            assertThat("Value should match", value, equalTo(42));
            return GeckoResult.fromValue("hello");
        }).then(value -> {
            assertThat("Value should match", value, equalTo("hello"));
            return GeckoResult.fromValue(42.0f);
        }).then(value -> {
            assertThat("Value should match", value, equalTo(42.0f));
            return GeckoResult.fromException(new Exception("boom"));
        }).exceptionally(error -> {
            assertThat("Error message should match", error.getMessage(), equalTo("boom"));
            throw new MockException();
        }).accept(null, exception -> {
            assertThat("Exception should be MockException", exception, instanceOf(MockException.class));
            done();
        });

        waitUntilDone();
    }

    @UiThreadTest
    @Test
    public void then_propagatedValue() {
        // The first GeckoResult only has an exception listener, so when the value 42 is
        // propagated to subsequent GeckoResult instances, the propagated value is coerced to null.
        GeckoResult.fromValue(42).exceptionally(error -> null)
        .accept(value -> {
            assertThat("Propagated value is null", value, nullValue());
            done();
        });

        waitUntilDone();
    }

    @UiThreadTest
    @Test(expected = GeckoResult.UncaughtException.class)
    public void then_uncaughtException() {
        GeckoResult.fromValue(42).then(value -> {
            throw new MockException();
        });

        waitUntilDone();
    }

    @UiThreadTest
    @Test(expected = GeckoResult.UncaughtException.class)
    public void then_propagatedUncaughtException() {
        GeckoResult.fromValue(42).then(value -> {
            throw new MockException();
        }).accept(value -> {});

        waitUntilDone();
    }

    @UiThreadTest
    @Test
    public void then_caughtException() {
        GeckoResult.fromValue(42).then(value -> { throw new MockException(); })
            .accept(value -> {})
            .exceptionally(exception -> {
                assertThat("Exception should be expected",
                           exception, instanceOf(MockException.class));
                done();
                return null;
            });

        waitUntilDone();
    }

    @Test(expected = IllegalThreadStateException.class)
    public void noLooperThenThrows() {
        assertThat("We shouldn't have a Looper", Looper.myLooper(), nullValue());
        GeckoResult.fromValue(42).then(value -> null);
    }

    @Test
    public void noLooperPoll() throws Throwable {
        assertThat("We shouldn't have a Looper", Looper.myLooper(), nullValue());
        assertThat("Value should match",
                GeckoResult.fromValue(42).poll(0), equalTo(42));
    }

    @Test
    public void withHandler() {

        final SynchronousQueue<Handler> queue = new SynchronousQueue<>();
        final Thread thread = new Thread(() -> {
            Looper.prepare();

            try {
                queue.put(new Handler());
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }

            Looper.loop();
        });

        thread.start();

        final GeckoResult<Integer> result = GeckoResult.fromValue(42);
        assertThat("We shouldn't have a Looper", result.getLooper(), nullValue());

        try {
            result.withHandler(queue.take()).accept(value -> {
                assertThat("Thread should match", Thread.currentThread(), equalTo(thread));
                assertThat("Value should match", value, equalTo(42));
                Looper.myLooper().quit();
            });

            thread.join();
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    @Test
    public void pollCompleteWithValue() throws Throwable {
        assertThat("Value should match",
                GeckoResult.fromValue(42).poll(0), equalTo(42));
    }

    @Test(expected = MockException.class)
    public void pollCompleteWithError() throws Throwable {
        GeckoResult.fromException(new MockException()).poll(0);
    }


    @Test(expected = TimeoutException.class)
    public void pollTimeout() throws Throwable {
        new GeckoResult<Void>().poll(1);
    }

    @UiThreadTest
    @Test(expected = TimeoutException.class)
    public void pollTimeoutWithLooper() throws Throwable {
        new GeckoResult<Void>().poll(1);
    }

    @UiThreadTest
    @Test(expected = IllegalThreadStateException.class)
    public void pollWithLooper() throws Throwable {
        new GeckoResult<Void>().poll();
    }
}
