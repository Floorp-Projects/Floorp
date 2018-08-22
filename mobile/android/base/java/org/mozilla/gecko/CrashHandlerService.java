package org.mozilla.gecko;

import android.app.IntentService;
import android.content.Intent;
import android.support.annotation.Nullable;


public class CrashHandlerService extends IntentService {

    public CrashHandlerService() {
        super("Crash Handler");
    }

    @Override
    protected void onHandleIntent(@Nullable Intent intent) {
        intent.setClass(this, CrashReporterActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(intent);

        // Avoid ANR due to background limitations on Oreo+
        System.exit(0);
    }
}
