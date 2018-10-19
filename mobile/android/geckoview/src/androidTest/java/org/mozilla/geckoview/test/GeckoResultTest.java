package org.mozilla.geckoview.test;

import org.mozilla.geckoview.GeckoResult;
import org.mozilla.geckoview.GeckoResult.OnExceptionListener;
import org.mozilla.geckoview.GeckoResult.OnValueListener;
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
    private static final long DEFAULT_TIMEOUT = 5000;

    private static class MockException extends RuntimeException {
    }

    private boolean mDone;

    private void waitUntilDone() {
        assertThat("We should not be done", mDone, equalTo(false));

        while (!mDone) {
            UiThreadUtils.loopUntilIdle(DEFAULT_TIMEOUT);
        }
    }

    private void done() {
        UiThreadUtils.HANDLER.post(new Runnable() {
            @Override
            public void run() {
                mDone = true;
            }
        });
    }

    @Before
    public void setup() {
        mDone = false;
    }

    @Test
    @UiThreadTest
    public void thenWithResult() {
        GeckoResult.fromValue(42).then(new OnValueListener<Integer, Void>() {
            @Override
            public GeckoResult<Void> onValue(Integer value) {
                assertThat("Value should match", value, equalTo(42));
                done();
                return null;
            }
        });

        waitUntilDone();
    }

    @Test
    @UiThreadTest
    public void thenWithException() {
        final Throwable boom = new Exception("boom");
        GeckoResult.fromException(boom).then(null, new OnExceptionListener<Void>() {
            @Override
            public GeckoResult<Void> onException(Throwable error) {
                assertThat("Exception should match", error, equalTo(boom));
                done();
                return null;
            }
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
        result.then(new OnValueListener<Integer, Void>() {
            @Override
            public GeckoResult<Void> onValue(Integer value) throws Throwable {
                assertThat("Value should match", value, equalTo(42));
                done();
                return null;
            }
        });

        waitUntilDone();
    }

    @Test(expected = IllegalStateException.class)
    @UiThreadTest
    public void completeMultiple() {
        final GeckoResult<Integer> deferred = new GeckoResult<Integer>();
        deferred.complete(42);
        deferred.complete(43);
    }

    @Test(expected = IllegalStateException.class)
    @UiThreadTest
    public void completeMultipleExceptions() {
        final GeckoResult<Integer> deferred = new GeckoResult<Integer>();
        deferred.completeExceptionally(new Exception("boom"));
        deferred.completeExceptionally(new Exception("boom again"));
    }

    @Test(expected = IllegalStateException.class)
    @UiThreadTest
    public void completeMixed() {
        final GeckoResult<Integer> deferred = new GeckoResult<Integer>();
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
        final Thread thread = new Thread() {
            @Override
            public void run() {
                deferred.complete(42);
            }
        };

        deferred.then(new OnValueListener<Integer, Void>() {
            @Override
            public GeckoResult<Void> onValue(Integer value) {
                assertThat("Value should match", value, equalTo(42));
                assertThat("Thread should match", Thread.currentThread(),
                        equalTo(Looper.getMainLooper().getThread()));
                done();
                return null;
            }
        });

        thread.start();
        waitUntilDone();
    }

    @Test
    @UiThreadTest
    public void dispatchOnInitialThread() throws InterruptedException {
        final Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {
                Looper.prepare();
                final Thread dispatchThread = Thread.currentThread();

                GeckoResult.fromValue(42).then(new OnValueListener<Integer, Void>() {
                    @Override
                    public GeckoResult<Void> onValue(Integer value) throws Throwable {
                        assertThat("Thread should match", Thread.currentThread(),
                                equalTo(dispatchThread));
                        Looper.myLooper().quit();
                        return null;
                    }
                });

                Looper.loop();
            }
        });

        thread.start();
        thread.join();
    }

    @Test
    @UiThreadTest
    public void completeExceptionallyThreaded() {
        final GeckoResult<Integer> deferred = new GeckoResult<>();
        final Throwable boom = new Exception("boom");
        final Thread thread = new Thread() {
            @Override
            public void run() {
                deferred.completeExceptionally(boom);
            }
        };

        deferred.exceptionally(new OnExceptionListener<Void>() {
            @Override
            public GeckoResult<Void> onException(Throwable error) {
                assertThat("Exception should match", error, equalTo(boom));
                assertThat("Thread should match", Thread.currentThread(),
                        equalTo(Looper.getMainLooper().getThread()));
                done();
                return null;
            }
        });

        thread.start();
        waitUntilDone();
    }

    @UiThreadTest
    @Test
    public void resultChaining() {
        assertThat("We're on the UI thread", Thread.currentThread(), equalTo(Looper.getMainLooper().getThread()));

        GeckoResult.fromValue(42).then(new OnValueListener<Integer, String>() {
            @Override
            public GeckoResult<String> onValue(Integer value) {
                assertThat("Value should match", value, equalTo(42));
                return GeckoResult.fromValue("hello");
            }
        }).then(new OnValueListener<String, Float>() {
            @Override
            public GeckoResult<Float> onValue(String value) {
                assertThat("Value should match", value, equalTo("hello"));
                return GeckoResult.fromValue(42.0f);
            }
        }).then(new OnValueListener<Float, Float>() {
            @Override
            public GeckoResult<Float> onValue(Float value) {
                assertThat("Value should match", value, equalTo(42.0f));
                return GeckoResult.fromException(new Exception("boom"));
            }
        }).exceptionally(new OnExceptionListener<Void>() {
            @Override
            public GeckoResult<Void> onException(Throwable error) {
                assertThat("Error message should match", error.getMessage(), equalTo("boom"));
                throw new MockException();
            }
        }).exceptionally(new OnExceptionListener<Void>() {
            @Override
            public GeckoResult<Void> onException(Throwable exception) {
                assertThat("Exception should be MockException", exception, instanceOf(MockException.class));
                done();
                return null;
            }
        });

        waitUntilDone();
    }

    @UiThreadTest
    @Test
    public void then_propagatedValue() {
        // The first GeckoResult only has an exception listener, so when the value 42 is
        // propagated to subsequent GeckoResult instances, the propagated value is coerced to null.
        GeckoResult.fromValue(42).exceptionally(new OnExceptionListener<String>() {
            @Override
            public GeckoResult<String> onException(Throwable exception) throws Throwable {
                return null;
            }
        }).then(new OnValueListener<String, Void>() {
            @Override
            public GeckoResult<Void> onValue(String value) throws Throwable {
                assertThat("Propagated value is null", value, nullValue());
                done();
                return null;
            }
        });

        waitUntilDone();
    }

    @UiThreadTest
    @Test(expected = GeckoResult.UncaughtException.class)
    public void then_uncaughtException() {
        GeckoResult.fromValue(42).then(new OnValueListener<Integer, String>() {
            @Override
            public GeckoResult<String> onValue(Integer value) {
                throw new MockException();
            }
        });

        waitUntilDone();
    }

    @UiThreadTest
    @Test(expected = GeckoResult.UncaughtException.class)
    public void then_propagatedUncaughtException() {
        GeckoResult.fromValue(42).then(new OnValueListener<Integer, String>() {
            @Override
            public GeckoResult<String> onValue(Integer value) {
                throw new MockException();
            }
        }).then(new OnValueListener<String, Void>() {
            @Override
            public GeckoResult<Void> onValue(String value) throws Throwable {
                return null;
            }
        });

        waitUntilDone();
    }

    @UiThreadTest
    @Test
    public void then_caughtException() {
        GeckoResult.fromValue(42).then(new OnValueListener<Integer, String>() {
            @Override
            public GeckoResult<String> onValue(Integer value) throws Exception {
                throw new MockException();
            }
        }).then(new OnValueListener<String, Void>() {
            @Override
            public GeckoResult<Void> onValue(String value) throws Throwable {
                return null;
            }
        }).exceptionally(new OnExceptionListener<Void>() {
            @Override
            public GeckoResult<Void> onException(Throwable exception) throws Throwable {
                assertThat("Exception should be expected",
                           exception, instanceOf(MockException.class));
                done();
                return null;
            }
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
            result.withHandler(queue.take()).then(value -> {
                assertThat("Thread should match", Thread.currentThread(), equalTo(thread));
                assertThat("Value should match", value, equalTo(42));
                Looper.myLooper().quit();
                return null;
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

    @Test
    public void pollIncompleteWithValue() throws Throwable {
        final GeckoResult<Integer> result = new GeckoResult<>();

        final Thread thread = new Thread(() -> result.complete(42));

        thread.start();
        assertThat("Value should match", result.poll(), equalTo(42));
    }

    @Test(expected = MockException.class)
    public void pollIncompleteWithError() throws Throwable {
        final GeckoResult<Void> result = new GeckoResult<>();

        final Thread thread = new Thread(() -> result.completeExceptionally(new MockException()));

        thread.start();
        result.poll();
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
