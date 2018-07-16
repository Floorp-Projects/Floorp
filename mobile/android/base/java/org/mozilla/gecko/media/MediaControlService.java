package org.mozilla.gecko.media;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

import org.mozilla.gecko.R;

public class MediaControlService extends Service {
    private static final String LOGTAG = "MediaControlService";

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.d(LOGTAG, "onStartCommand");
        startForeground(R.id.mediaControlNotification, GeckoMediaControlAgent.getInstance().getCurrentNotification());

        if (intent != null && intent.getAction() != null) {
            GeckoMediaControlAgent.getInstance().handleAction(intent.getAction());
        }

        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
