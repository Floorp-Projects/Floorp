package org.mozilla.gecko;

import org.mozilla.gecko.fxa.activities.FxAccountGetStartedActivity;

import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.view.GestureDetector;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnTouchListener;
import android.view.ViewGroup;
import android.widget.Button;

public class StartPane extends DialogFragment {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setStyle(DialogFragment.STYLE_NO_TITLE, 0);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle bundle) {
        final View view = inflater.inflate(R.layout.onboard_start_pane, container, false);
        final Button browserButton = (Button) view.findViewById(R.id.button_browser);
        browserButton.setOnClickListener(new OnClickListener() {

            @Override
            public void onClick(View v) {
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, "firstrun-sync");

                // StartPane is on the stack above the browser, so just dismiss this Fragment.
                StartPane.this.dismiss();
            }
        });

        final Button accountButton = (Button) view.findViewById(R.id.button_account);
        accountButton.setOnClickListener(new OnClickListener() {

            @Override
            public void onClick(View v) {
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, "firstrun-browser");

                final Intent intent = new Intent(getActivity(), FxAccountGetStartedActivity.class);
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                startActivity(intent);
                StartPane.this.dismiss();
            }
        });

        addDismissHandler(view);
        return view;
    }

    @Override
    public void onCancel(DialogInterface dialog) {
        // StartPane is closed by touching outside the dialog.
        Telemetry.sendUIEvent(TelemetryContract.Event.CANCEL, TelemetryContract.Method.DIALOG, "firstrun-pane");
        super.onCancel(dialog);
    }

    // Add handler for dismissing the StartPane on a single click.
    private void addDismissHandler(View view) {
        final GestureDetector gestureDetector = new GestureDetector(getActivity(), new GestureDetector.SimpleOnGestureListener() {
            @Override
            public boolean onSingleTapUp(MotionEvent e) {
                Telemetry.sendUIEvent(TelemetryContract.Event.CANCEL, TelemetryContract.Method.DIALOG, "firstrun-pane");
                StartPane.this.dismiss();
                return true;
            }
        });

        view.findViewById(R.id.onboard_content).setOnTouchListener(new OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                return gestureDetector.onTouchEvent(event);
            }
        });
    }
}
