package org.mozilla.gecko;

import org.mozilla.gecko.fxa.activities.FxAccountGetStartedActivity;
import org.mozilla.gecko.util.HardwareUtils;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnTouchListener;
import android.widget.Button;

public class StartPane extends Activity {

    @Override
    public void onCreate(Bundle bundle) {
        super.onCreate(bundle);
        setContentView(R.layout.onboard_start_pane);

        final Button accountButton = (Button) findViewById(R.id.button_account);
        accountButton.setOnClickListener(new OnClickListener() {

            @Override
            public void onClick(View v) {
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, "firstrun-sync");
                showAccountSetup();
            }
        });

        final Button browserButton = (Button) findViewById(R.id.button_browser);
        browserButton.setOnClickListener(new OnClickListener() {

            @Override
            public void onClick(View v) {
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, "firstrun-browser");
                showBrowser();
            }
        });

        if (!HardwareUtils.isTablet() && !HardwareUtils.isTelevision()) {
            addDismissHandler();
        }
    }

    private void showBrowser() {
        // StartPane is on the stack above the browser, so just kill this activity.
        finish();
    }

    private void showAccountSetup() {
        final Intent intent = new Intent(this, FxAccountGetStartedActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(intent);
        finish();
    }

    // Add handler for dismissing the StartPane on a single click.
    private void addDismissHandler() {
        final GestureDetector gestureDetector = new GestureDetector(this, new GestureDetector.SimpleOnGestureListener() {
            @Override
            public boolean onSingleTapUp(MotionEvent e) {
                StartPane.this.finish();
                return true;
            }
        });

        findViewById(R.id.onboard_content).setOnTouchListener(new OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                return gestureDetector.onTouchEvent(event);
            }
        });
    }
}
