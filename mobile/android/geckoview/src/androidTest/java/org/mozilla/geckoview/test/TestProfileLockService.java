package org.mozilla.geckoview.test;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.mozilla.geckoview.GeckoRuntime;
import org.mozilla.geckoview.GeckoRuntimeSettings;
import org.mozilla.geckoview.GeckoSession;

public class TestProfileLockService extends Service implements
        GeckoSession.ProgressDelegate,
        GeckoRuntime.Delegate {
    public static final class p0 extends TestProfileLockService {}
    public static final class p1 extends TestProfileLockService {}

    // Used by the client to register themselves
    public static final int MESSAGE_REGISTER = 1;
    // Sent when the first page load completes
    public static final int MESSAGE_PAGE_STOP = 2;
    // Sent when GeckoRuntime exits
    public static final int MESSAGE_QUIT = 3;

    private GeckoRuntime mRuntime;

    private Messenger mClient;

    private class Handler extends android.os.Handler {
        @Override
        public void handleMessage(@NonNull Message msg) {
            switch (msg.what) {
                case MESSAGE_REGISTER:
                    mClient = msg.replyTo;
                    return;
                default:
                    throw new IllegalStateException("Unknown message: " + msg.what);
            }
        }
    }

    final Messenger mMessenger = new Messenger(new Handler());

    @Override
    public void onShutdown() {
        final Message message = Message.obtain(null, MESSAGE_QUIT);
        try {
            mClient.send(message);
        } catch (RemoteException ex) {
            throw new RuntimeException(ex);
        }
    }

    @Override
    public void onPageStop(@NonNull GeckoSession session, boolean success) {
        final Message message = Message.obtain(null, MESSAGE_PAGE_STOP);
        try {
            mClient.send(message);
        } catch (RemoteException ex) {
            throw new RuntimeException(ex);
        }
    }

    @Override
    public void onDestroy() {
        // Sometimes the service doesn't die on it's own so we need to kill it here.
        System.exit(0);
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        // Request to be killed as soon as the client unbinds.
        stopSelf();

        if (mRuntime != null) {
            // We only expect one client
            throw new RuntimeException("Multiple clients !?");
        }

        GeckoRuntimeSettings settings = new GeckoRuntimeSettings.Builder()
                .extras(intent.getExtras())
                .build();
        mRuntime = GeckoRuntime.create(getApplicationContext(), settings);

        mRuntime.setDelegate(this);

        final GeckoSession session = new GeckoSession();
        session.setProgressDelegate(this);
        session.open(mRuntime);

        return mMessenger.getBinder();
    }
}
