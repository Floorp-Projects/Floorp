package org.mozilla.geckoview.test;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.geckoview.GeckoResult;
import org.mozilla.geckoview.GeckoResult.OnExceptionListener;
import org.mozilla.geckoview.GeckoResult.OnValueListener;
import org.mozilla.geckoview.test.util.UiThreadUtils;

import android.os.Looper;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.MediumTest;
import android.support.test.rule.UiThreadTestRule;
import android.support.test.runner.AndroidJUnit4;

import static org.hamcrest.Matchers.equalTo;
import static org.junit.Assert.assertThat;

@RunWith(AndroidJUnit4.class)
@MediumTest
public class GeckoResultTest {
    private static final long DEFAULT_TIMEOUT = 5000;

    @Rule
    public UiThreadTestRule mUiThreadTestRule = new UiThreadTestRule();

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
    public void testEquals() {
        final GeckoResult<Integer> result = GeckoResult.fromValue(42);
        final GeckoResult<Integer> result2 = new GeckoResult<>(result);

        assertThat("Results should be equal", result, equalTo(result2));
        assertThat("Hashcode should be equal", result.hashCode(), equalTo(result2.hashCode()));
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
    public void completeExceptionallyThreaded() {
        final GeckoResult<Integer> deferred = new GeckoResult<>();
        final Throwable boom = new Exception("boom");
        final Thread thread = new Thread() {
            @Override
            public void run() {
                deferred.completeExceptionally(boom);
            }
        };

        deferred.then(new OnExceptionListener<Void>() {
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
        }).then(new OnExceptionListener<Void>() {
            @Override
            public GeckoResult<Void> onException(Throwable error) {
                assertThat("Error message should match", error.getMessage(), equalTo("boom"));
                throw new MockException();
            }
        }).then(new OnExceptionListener<Void>() {
            @Override
            public GeckoResult<Void> onException(Throwable exception) {
                assertThat("Exception should be MockException", exception instanceof MockException, equalTo(true));
                done();
                return null;
            }
        });

        waitUntilDone();
    }

    private static class MockException extends RuntimeException {
    }
}
