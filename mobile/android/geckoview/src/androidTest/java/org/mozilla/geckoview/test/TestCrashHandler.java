package org.mozilla.geckoview.test;

import android.app.Service;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.support.annotation.Nullable;
import android.util.Log;

import org.mozilla.geckoview.GeckoRuntime;
import org.mozilla.geckoview.test.util.UiThreadUtils;

import java.io.File;

public class TestCrashHandler extends Service {
    private static final int MSG_EVAL_NEXT_CRASH_DUMP = 1;
    private static final int MSG_CRASH_DUMP_EVAL_RESULT = 2;
    private static final String LOGTAG = "TestCrashHandler";

    public static final class EvalResult {
        private static final String BUNDLE_KEY_RESULT = "TestCrashHandler.EvalResult.mResult";
        private static final String BUNDLE_KEY_MSG = "TestCrashHandler.EvalResult.mMsg";

        public EvalResult(boolean result, String msg) {
            mResult = result;
            mMsg = msg;
        }

        public EvalResult(Bundle bundle) {
            mResult = bundle.getBoolean(BUNDLE_KEY_RESULT, false);
            mMsg = bundle.getString(BUNDLE_KEY_MSG);
        }

        public Bundle asBundle() {
            final Bundle bundle = new Bundle();
            bundle.putBoolean(BUNDLE_KEY_RESULT, mResult);
            bundle.putString(BUNDLE_KEY_MSG, mMsg);
            return bundle;
        }

        public boolean mResult;
        public String mMsg;
    }

    public static final class Client {
        private static final String LOGTAG = "TestCrashHandler.Client";

        private class Receiver extends Handler {
            public Receiver(final Looper looper) {
                super(looper);
            }

            @Override
            public void handleMessage(Message msg) {
                if (msg.what == MSG_CRASH_DUMP_EVAL_RESULT) {
                    setEvalResult(new EvalResult(msg.getData()));
                    return;
                }

                super.handleMessage(msg);
            }
        }

        private Receiver mReceiver;
        private boolean mDoUnbind = false;
        private Messenger mService = null;
        private Messenger mMessenger;
        private Context mContext;
        private HandlerThread mThread;
        private EvalResult mResult = null;

        private ServiceConnection mConnection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName className, IBinder service) {
                mService = new Messenger(service);
            }

            @Override
            public void onServiceDisconnected(ComponentName className) {
                disconnect();
            }
        };

        public Client(final Context context) {
            mContext = context;
            mThread = new HandlerThread("TestCrashHandler.Client");
            mThread.start();
            mReceiver = new Receiver(mThread.getLooper());
            mMessenger = new Messenger(mReceiver);
        }

        /**
         * Tests should call this to notify the crash handler that the next crash it sees is
         * intentional and that its intent should be checked for correctness.
         *
         * @param expectFatal Whether the incoming crash is expected to be fatal or not.
         */
        public void setEvalNextCrashDump(final boolean expectFatal) {
            setEvalResult(null);
            mReceiver.post(new Runnable() {
                @Override
                public void run() {
                    Message msg = Message.obtain(null, MSG_EVAL_NEXT_CRASH_DUMP,
                                                 expectFatal ? 1 : 0, 0);
                    msg.replyTo = mMessenger;

                    try {
                        mService.send(msg);
                    } catch (RemoteException e) {
                        throw new RuntimeException(e.getMessage());
                    }
                }
            });
        }

        public boolean connect(final long timeoutMillis) {
            Intent intent = new Intent(mContext, TestCrashHandler.class);
            mDoUnbind = mContext.bindService(intent, mConnection, Context.BIND_AUTO_CREATE |
                                                                  Context.BIND_IMPORTANT);
            if (!mDoUnbind) {
                return false;
            }

            UiThreadUtils.waitForCondition(() -> mService != null, timeoutMillis);

            return mService != null;
        }

        public void disconnect() {
            if (mDoUnbind) {
                mContext.unbindService(mConnection);
                mService = null;
                mDoUnbind = false;
            }
            mThread.quitSafely();
        }

        private synchronized void setEvalResult(EvalResult result) {
            mResult = result;
        }

        private synchronized EvalResult getEvalResult() {
            return mResult;
        }

        /**
         * Tests should call this method after initiating the intentional crash to wait for the
         * result from the crash handler.
         *
         * @param timeoutMillis timeout in milliseconds
         * @return EvalResult containing the boolean result of the test and an error message.
         */
        public EvalResult getEvalResult(final long timeoutMillis) {
            UiThreadUtils.waitForCondition(() -> getEvalResult() != null, timeoutMillis);
            return getEvalResult();
        }
    }

    private static final class MessageHandler extends Handler {
        private Messenger mReplyToMessenger;
        private boolean mExpectFatal = false;

        MessageHandler() {
        }

        @Override
        public void handleMessage(Message msg) {
            if (msg.what == MSG_EVAL_NEXT_CRASH_DUMP) {
                mReplyToMessenger = msg.replyTo;
                mExpectFatal = msg.arg1 != 0;
                return;
            }

            super.handleMessage(msg);
        }

        public void reportResult(EvalResult result) {
            if (mReplyToMessenger == null) {
                return;
            }

            Message msg = Message.obtain(null, MSG_CRASH_DUMP_EVAL_RESULT);
            msg.setData(result.asBundle());

            try {
                mReplyToMessenger.send(msg);
            } catch (RemoteException e) {
                throw new RuntimeException(e.getMessage());
            }

            mReplyToMessenger = null;
        }

        public boolean getExpectFatal() {
            return mExpectFatal;
        }
    }

    private Messenger mMessenger;
    private MessageHandler mMsgHandler;

    public TestCrashHandler() {
    }

    private EvalResult evalCrashInfo(final Intent intent) {
        if (!intent.getAction().equals(GeckoRuntime.ACTION_CRASHED)) {
            return new EvalResult(false, "Action should match");
        }

        final File dumpFile = new File(intent.getStringExtra(GeckoRuntime.EXTRA_MINIDUMP_PATH));
        final boolean dumpFileExists = dumpFile.exists();
        dumpFile.delete();

        final File extrasFile = new File(intent.getStringExtra(GeckoRuntime.EXTRA_EXTRAS_PATH));
        final boolean extrasFileExists = extrasFile.exists();
        extrasFile.delete();

        if (!dumpFileExists) {
            return new EvalResult(false, "Dump file should exist");
        }

        if (!extrasFileExists) {
            return new EvalResult(false, "Extras file should exist");
        }

        final boolean expectFatal = mMsgHandler.getExpectFatal();
        if (intent.getBooleanExtra(GeckoRuntime.EXTRA_CRASH_FATAL, !expectFatal) != expectFatal) {
            return new EvalResult(false, "Fatality should match");
        }

        return new EvalResult(true, "Crash Dump OK");
    }

    @Override
    public synchronized int onStartCommand(Intent intent, int flags, int startId) {
        if (mMsgHandler != null) {
            mMsgHandler.reportResult(evalCrashInfo(intent));
            return Service.START_NOT_STICKY;
        }

        // We don't want to do anything, this handler only exists
        // so we produce a crash dump which is picked up by the
        // test harness.
        System.exit(0);
        return Service.START_NOT_STICKY;
    }

    @Override
    public synchronized IBinder onBind(Intent intent) {
        mMsgHandler = new MessageHandler();
        mMessenger = new Messenger(mMsgHandler);
        return mMessenger.getBinder();
    }

    @Override
    public synchronized boolean onUnbind(Intent intent) {
        mMsgHandler = null;
        mMessenger = null;
        return false;
    }
}
