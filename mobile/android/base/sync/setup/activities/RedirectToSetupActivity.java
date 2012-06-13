package org.mozilla.gecko.sync.setup.activities;

import org.mozilla.gecko.R;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.setup.Constants;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;

public class RedirectToSetupActivity extends Activity {
  public static final String LOG_TAG = "RedirectToSetupActivity";
  @Override
  public void onCreate(Bundle savedInstanceState) {
    setTheme(R.style.SyncTheme);
    super.onCreate(savedInstanceState);
    setContentView(R.layout.sync_redirect_to_setup);
  }

  public void redirectToSetupHandler(View view) {
    Logger.info(LOG_TAG, "Setup Sync was clicked.");

    Intent intent = new Intent(this, SetupSyncActivity.class);
    intent.setFlags(Constants.FLAG_ACTIVITY_REORDER_TO_FRONT_NO_ANIMATION);
    startActivity(intent);
  }

  public void cancelClickHandler(View target) {
    setResult(RESULT_CANCELED);
    finish();
  }
}
