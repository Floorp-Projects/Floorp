package org.mozilla.gecko;

import org.mozilla.gecko.fxa.activities.FxAccountGetStartedActivity;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
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
                showAccountSetup();
            }
        });

        final Button browserButton = (Button) findViewById(R.id.button_browser);
        browserButton.setOnClickListener(new OnClickListener() {

            @Override
            public void onClick(View v) {
                showBrowser();
            }
        });
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
}
