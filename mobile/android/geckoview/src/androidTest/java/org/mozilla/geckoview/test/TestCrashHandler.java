package org.mozilla.geckoview.test;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.support.annotation.Nullable;

public class TestCrashHandler extends Service {
    public TestCrashHandler() {
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // We don't want to do anything, this handler only exists
        // so we produce a crash dump which is picked up by the
        // test harness.
        System.exit(0);
        return Service.START_NOT_STICKY;
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
