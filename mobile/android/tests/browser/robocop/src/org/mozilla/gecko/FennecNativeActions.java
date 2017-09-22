/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.ArrayList;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

import org.mozilla.gecko.FennecNativeDriver.LogLevel;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.gfx.LayerView.DrawListener;
import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.sqlite.SQLiteBridge;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

import android.app.Activity;
import android.app.Instrumentation;
import android.database.Cursor;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.KeyEvent;

import com.robotium.solo.Solo;

public class FennecNativeActions implements Actions {
    private static final String LOGTAG = "FennecNativeActions";

    private Solo mSolo;
    private Instrumentation mInstr;
    private Assert mAsserter;

    public FennecNativeActions(Activity activity, Solo robocop, Instrumentation instrumentation, Assert asserter) {
        mSolo = robocop;
        mInstr = instrumentation;
        mAsserter = asserter;

        GeckoLoader.loadSQLiteLibs(activity, activity.getApplication().getPackageResourcePath());
    }

    class GeckoEventExpecter implements RepeatedEventExpecter {
        private static final int MAX_WAIT_MS = 180000;

        private volatile boolean mIsRegistered;

        private final EventDispatcher mDispatcher;
        private final EventType mType;
        private final String mGeckoEvent;
        private final BlockingQueue<Object> mEventDataQueue;
        private final BundleEventListener mBundleListener;

        private volatile boolean mEventEverReceived;
        private Object mEventData;

        GeckoEventExpecter(final EventDispatcher dispatcher, final EventType type,
                           final String geckoEvent) {
            if (TextUtils.isEmpty(geckoEvent)) {
                throw new IllegalArgumentException("geckoEvent must not be empty");
            }

            mDispatcher = dispatcher;
            mType = type;
            mGeckoEvent = geckoEvent;
            mEventDataQueue = new LinkedBlockingQueue<>();
            mIsRegistered = true;

            mBundleListener = new BundleEventListener() {
                @Override
                public void handleMessage(final String event, final GeckoBundle message,
                                          final EventCallback callback) {
                    FennecNativeDriver.log(FennecNativeDriver.LogLevel.DEBUG,
                            "handleMessage called for: " + event + "; expecting: " + mGeckoEvent);
                    mAsserter.is(event, mGeckoEvent,
                            "Given message occurred for registered event: " + message);

                    // Because we cannot put null into the queue, we have to use an empty
                    // bundle if we don't have a message.
                    notifyOfEvent(message != null ? message : new GeckoBundle(0));
                }
            };

            if (type == EventType.GECKO) {
                dispatcher.registerGeckoThreadListener(mBundleListener, geckoEvent);
            } else if (type == EventType.UI) {
                dispatcher.registerUiThreadListener(mBundleListener, geckoEvent);
            } else if (type == EventType.BACKGROUND) {
                dispatcher.registerBackgroundThreadListener(mBundleListener, geckoEvent);
            } else {
                throw new IllegalArgumentException("Unsupported thread type");
            }
        }

        public void blockForEvent() {
            blockForEvent(MAX_WAIT_MS, true);
        }

        public void blockForEvent(long millis, boolean failOnTimeout) {
            if (!mIsRegistered) {
                throw new IllegalStateException("listener not registered");
            }

            try {
                mEventData = mEventDataQueue.poll(millis, TimeUnit.MILLISECONDS);
            } catch (InterruptedException ie) {
                FennecNativeDriver.log(LogLevel.ERROR, ie);
            }
            if (mEventData == null) {
                if (failOnTimeout) {
                    FennecNativeDriver.logAllStackTraces(FennecNativeDriver.LogLevel.ERROR);
                    mAsserter.ok(false, "GeckoEventExpecter",
                        "blockForEvent timeout: "+mGeckoEvent);
                } else {
                    FennecNativeDriver.log(FennecNativeDriver.LogLevel.DEBUG,
                        "blockForEvent timeout: "+mGeckoEvent);
                }
            } else {
                FennecNativeDriver.log(FennecNativeDriver.LogLevel.DEBUG,
                    "unblocked on expecter for " + mGeckoEvent);
            }
        }

        public void blockUntilClear(long millis) {
            if (!mIsRegistered) {
                throw new IllegalStateException("listener not registered");
            }
            if (millis <= 0) {
                throw new IllegalArgumentException("millis must be > 0");
            }

            // wait for at least one event
            try {
                mEventData = mEventDataQueue.poll(MAX_WAIT_MS, TimeUnit.MILLISECONDS);
            } catch (InterruptedException ie) {
                FennecNativeDriver.log(LogLevel.ERROR, ie);
            }
            if (mEventData == null) {
                FennecNativeDriver.logAllStackTraces(FennecNativeDriver.LogLevel.ERROR);
                mAsserter.ok(false, "GeckoEventExpecter", "blockUntilClear timeout");
                return;
            }
            // now wait for a period of millis where we don't get an event
            while (true) {
                try {
                    mEventData = mEventDataQueue.poll(millis, TimeUnit.MILLISECONDS);
                } catch (InterruptedException ie) {
                    FennecNativeDriver.log(LogLevel.INFO, ie);
                }
                if (mEventData == null) {
                    // success
                    break;
                }
            }
            FennecNativeDriver.log(FennecNativeDriver.LogLevel.DEBUG,
                "unblocked on expecter for " + mGeckoEvent);
        }

        public String blockForEventData() {
            blockForEvent();
            return (String) mEventData;
        }

        public String blockForEventDataWithTimeout(long millis) {
            blockForEvent(millis, false);
            return (String) mEventData;
        }

        public GeckoBundle blockForBundle() {
            blockForEvent();
            return (GeckoBundle) mEventData;
        }

        public GeckoBundle blockForBundleWithTimeout(long millis) {
            blockForEvent(millis, false);
            return (GeckoBundle) mEventData;
        }

        public void unregisterListener() {
            if (!mIsRegistered) {
                throw new IllegalStateException("listener not registered");
            }

            FennecNativeDriver.log(LogLevel.INFO,
                    "EventExpecter: no longer listening for " + mGeckoEvent);

            if (mType == EventType.GECKO) {
                mDispatcher.unregisterGeckoThreadListener(mBundleListener, mGeckoEvent);
            } else if (mType == EventType.UI) {
                mDispatcher.unregisterUiThreadListener(mBundleListener, mGeckoEvent);
            } else if (mType == EventType.BACKGROUND) {
                mDispatcher.unregisterBackgroundThreadListener(mBundleListener, mGeckoEvent);
            } else {
                throw new IllegalArgumentException("Unsupported thread type");
            }
            mIsRegistered = false;
        }

        public boolean eventReceived() {
            return mEventEverReceived;
        }

        /* package */ void notifyOfEvent(final Object data) {
            FennecNativeDriver.log(FennecNativeDriver.LogLevel.DEBUG,
                    "received event " + mGeckoEvent);

            mEventEverReceived = true;

            try {
                mEventDataQueue.put(data);
            } catch (InterruptedException e) {
                FennecNativeDriver.log(LogLevel.ERROR,
                    "EventExpecter dropped event: " + data, e);
            }
        }
    }

    public RepeatedEventExpecter expectGlobalEvent(final EventType type, final String geckoEvent) {
        FennecNativeDriver.log(FennecNativeDriver.LogLevel.DEBUG, "waiting for " + geckoEvent);
        return new GeckoEventExpecter(EventDispatcher.getInstance(), type, geckoEvent);
    }

    public RepeatedEventExpecter expectWindowEvent(final EventType type, final String geckoEvent) {
        FennecNativeDriver.log(FennecNativeDriver.LogLevel.DEBUG, "waiting for " + geckoEvent);
        return new GeckoEventExpecter(
                ((GeckoApp) mSolo.getCurrentActivity()).getAppEventDispatcher(),
                type, geckoEvent);
    }

    public void sendGlobalEvent(final String event, final GeckoBundle data) {
        EventDispatcher.getInstance().dispatch(event, data);
    }

    public void sendWindowEvent(final String event, final GeckoBundle data) {
        ((GeckoApp) mSolo.getCurrentActivity()).getAppEventDispatcher().dispatch(event, data);
    }

    public static final class PrefProxy implements PrefsHelper.PrefHandler, PrefWaiter {
        public static final int MAX_WAIT_MS = 180000;

        /* package */ final PrefHandlerBase target;
        private final String[] expectedPrefs;
        private final ArrayList<String> seenPrefs = new ArrayList<>();
        private boolean finished = false;

        /* package */ PrefProxy(PrefHandlerBase target, String[] expectedPrefs, Assert asserter) {
            this.target = target;
            this.expectedPrefs = expectedPrefs;
            target.asserter = asserter;
        }

        @Override // PrefsHelper.PrefHandler
        public void prefValue(String pref, boolean value) {
            target.prefValue(pref, value);
            seenPrefs.add(pref);
        }

        @Override // PrefsHelper.PrefHandler
        public void prefValue(String pref, int value) {
            target.prefValue(pref, value);
            seenPrefs.add(pref);
        }

        @Override // PrefsHelper.PrefHandler
        public void prefValue(String pref, String value) {
            target.prefValue(pref, value);
            seenPrefs.add(pref);
        }

        @Override // PrefsHelper.PrefHandler
        public synchronized void finish() {
            target.finish();

            for (String pref : expectedPrefs) {
                target.asserter.ok(seenPrefs.remove(pref), "Checking pref was seen", pref);
            }
            target.asserter.ok(seenPrefs.isEmpty(), "Checking unexpected prefs",
                               TextUtils.join(", ", seenPrefs));

            finished = true;
            this.notifyAll();
        }

        @Override // PrefWaiter
        public synchronized boolean isFinished() {
            return finished;
        }

        @Override // PrefWaiter
        public void waitForFinish() {
            waitForFinish(MAX_WAIT_MS, /* failOnTimeout */ true);
        }

        @Override // PrefWaiter
        public synchronized void waitForFinish(long timeoutMillis, boolean failOnTimeout) {
            final long startTime = System.nanoTime();
            while (!finished) {
                if (System.nanoTime() - startTime
                        >= timeoutMillis * 1e6 /* ns per ms */) {
                    final String prefsLog = "expected " +
                            TextUtils.join(", ", expectedPrefs) + "; got " +
                            TextUtils.join(", ", seenPrefs.toArray()) + ".";
                    if (failOnTimeout) {
                        FennecNativeDriver.logAllStackTraces(FennecNativeDriver.LogLevel.ERROR);
                        target.asserter.ok(false, "Timeout waiting for pref", prefsLog);
                    } else {
                        FennecNativeDriver.log(FennecNativeDriver.LogLevel.DEBUG,
                                               "Pref timeout (" + prefsLog + ")");
                    }
                    break;
                }
                try {
                    this.wait(1000); // Wait for 1 second at a time.
                } catch (final InterruptedException e) {
                    // Attempt waiting again.
                }
            }
            finished = false;
        }
    }

    @Override // Actions
    public PrefWaiter getPrefs(String[] prefNames, PrefHandlerBase handler) {
        final PrefProxy proxy = new PrefProxy(handler, prefNames, mAsserter);
        PrefsHelper.getPrefs(prefNames, proxy);
        return proxy;
    }

    @Override // Actions
    public void setPref(String pref, Object value, boolean flush) {
        PrefsHelper.setPref(pref, value, flush);
    }

    @Override // Actions
    public PrefWaiter addPrefsObserver(String[] prefNames, PrefHandlerBase handler) {
        final PrefProxy proxy = new PrefProxy(handler, prefNames, mAsserter);
        PrefsHelper.addObserver(prefNames, proxy);
        return proxy;
    }

    @Override // Actions
    public void removePrefsObserver(PrefWaiter proxy) {
        PrefsHelper.removeObserver((PrefProxy) proxy);
    }

    class PaintExpecter implements RepeatedEventExpecter {
        private static final int MAX_WAIT_MS = 90000;

        private boolean mPaintDone;
        private boolean mListening;

        private final LayerView mLayerView;
        private final DrawListener mDrawListener;

        PaintExpecter() {
            final PaintExpecter expecter = this;
            mLayerView = GeckoAppShell.getLayerView();
            mDrawListener = new DrawListener() {
                @Override
                public void drawFinished() {
                    FennecNativeDriver.log(FennecNativeDriver.LogLevel.DEBUG,
                            "Received drawFinished notification");
                    expecter.notifyOfEvent();
                }
            };
            mLayerView.addDrawListener(mDrawListener);
            mListening = true;
        }

        private synchronized void notifyOfEvent() {
            mPaintDone = true;
            this.notifyAll();
        }

        public synchronized void blockForEvent(long millis, boolean failOnTimeout) {
            if (!mListening) {
                throw new IllegalStateException("draw listener not registered");
            }
            long startTime = SystemClock.uptimeMillis();
            long endTime = 0;
            while (!mPaintDone) {
                try {
                    this.wait(millis);
                } catch (InterruptedException ie) {
                    FennecNativeDriver.log(LogLevel.ERROR, ie);
                    break;
                }
                endTime = SystemClock.uptimeMillis();
                if (!mPaintDone && (endTime - startTime >= millis)) {
                    if (failOnTimeout) {
                        FennecNativeDriver.logAllStackTraces(FennecNativeDriver.LogLevel.ERROR);
                        mAsserter.ok(false, "PaintExpecter", "blockForEvent timeout");
                    }
                    return;
                }
            }
        }

        public synchronized void blockForEvent() {
            blockForEvent(MAX_WAIT_MS, true);
        }

        public synchronized String blockForEventData() {
            blockForEvent();
            return null;
        }

        public synchronized String blockForEventDataWithTimeout(long millis) {
            blockForEvent(millis, false);
            return null;
        }

        public GeckoBundle blockForBundle() {
            throw new UnsupportedOperationException();
        }

        public GeckoBundle blockForBundleWithTimeout(long millis) {
            throw new UnsupportedOperationException();
        }

        public synchronized boolean eventReceived() {
            return mPaintDone;
        }

        public synchronized void blockUntilClear(long millis) {
            if (!mListening) {
                throw new IllegalStateException("draw listener not registered");
            }
            if (millis <= 0) {
                throw new IllegalArgumentException("millis must be > 0");
            }
            // wait for at least one event
            long startTime = SystemClock.uptimeMillis();
            long endTime = 0;
            while (!mPaintDone) {
                try {
                    this.wait(MAX_WAIT_MS);
                } catch (InterruptedException ie) {
                    FennecNativeDriver.log(LogLevel.ERROR, ie);
                    break;
                }
                endTime = SystemClock.uptimeMillis();
                if (!mPaintDone && (endTime - startTime >= MAX_WAIT_MS)) {
                    FennecNativeDriver.logAllStackTraces(FennecNativeDriver.LogLevel.ERROR);
                    mAsserter.ok(false, "PaintExpecter", "blockUtilClear timeout");
                    return;
                }
            }
            // now wait for a period of millis where we don't get an event
            startTime = SystemClock.uptimeMillis();
            while (true) {
                try {
                    this.wait(millis);
                } catch (InterruptedException ie) {
                    FennecNativeDriver.log(LogLevel.ERROR, ie);
                    break;
                }
                endTime = SystemClock.uptimeMillis();
                if (endTime - startTime >= millis) {
                    // success
                    break;
                }

                // we got a notify() before we could wait long enough, so we need to start over
                // Note, moving the goal post might have us race against a "drawFinished" flood
                startTime = endTime;
            }
        }

        public synchronized void unregisterListener() {
            if (!mListening) {
                throw new IllegalStateException("listener not registered");
            }

            FennecNativeDriver.log(LogLevel.INFO,
                    "PaintExpecter: no longer listening for events");
            mLayerView.removeDrawListener(mDrawListener);
            mListening = false;
        }
    }

    public RepeatedEventExpecter expectPaint() {
        return new PaintExpecter();
    }

    public void sendSpecialKey(SpecialKey button) {
        switch(button) {
            case DOWN:
                sendKeyCode(Solo.DOWN);
                break;
            case UP:
                sendKeyCode(Solo.UP);
                break;
            case LEFT:
                sendKeyCode(Solo.LEFT);
                break;
            case RIGHT:
                sendKeyCode(Solo.RIGHT);
                break;
            case ENTER:
                sendKeyCode(Solo.ENTER);
                break;
            case MENU:
                sendKeyCode(Solo.MENU);
                break;
            case DELETE:
                sendKeyCode(Solo.DELETE);
                break;
            default:
                mAsserter.ok(false, "sendSpecialKey", "Unknown SpecialKey " + button);
                break;
        }
    }

    public void sendKeyCode(int keyCode) {
        if (keyCode <= 0 || keyCode > KeyEvent.getMaxKeyCode()) {
            mAsserter.ok(false, "sendKeyCode", "Unknown keyCode " + keyCode);
        }
        mSolo.sendKey(keyCode);
    }

    @Override
    public void sendKeys(String input) {
        mInstr.sendStringSync(input);
    }

    public void drag(int startingX, int endingX, int startingY, int endingY) {
        mSolo.drag(startingX, endingX, startingY, endingY, 10);
    }

    public Cursor querySql(final String dbPath, final String sql) {
        return new SQLiteBridge(dbPath).rawQuery(sql, null);
    }
}
