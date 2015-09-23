// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

package org.mozilla.gecko.widget;

import org.mozilla.gecko.ActivityHandlerHelper;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;

import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.FragmentManager;
import android.support.v7.app.AlertDialog;
import android.util.Log;

import java.util.List;

/**
 * A DialogFragment to contain a dialog that appears when the user clicks an Intent:// URI during private browsing. The
 * dialog appears to notify the user that a clicked link will open in an external application, potentially leaking their
 * browsing history.
 */
public class ExternalIntentDuringPrivateBrowsingPromptFragment extends DialogFragment {
    private static final String LOGTAG = ExternalIntentDuringPrivateBrowsingPromptFragment.class.getSimpleName();
    private static final String FRAGMENT_TAG = "ExternalIntentPB";

    private static final String KEY_APPLICATION_NAME = "matchingApplicationName";
    private static final String KEY_INTENT = "intent";

    @Override
    public Dialog onCreateDialog(final Bundle savedInstanceState) {
        final Bundle args = getArguments();
        final CharSequence matchingApplicationName = args.getCharSequence(KEY_APPLICATION_NAME);
        final Intent intent = args.getParcelable(KEY_INTENT);

        final Context context = getActivity();
        final String promptMessage = context.getString(R.string.intent_uri_private_browsing_prompt, matchingApplicationName);

        final AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
        builder.setMessage(promptMessage)
                .setTitle(intent.getDataString())
                .setPositiveButton(R.string.button_yes, new DialogInterface.OnClickListener() {
                    public void onClick(final DialogInterface dialog, final int id) {
                        context.startActivity(intent);
                    }
                })
                .setNegativeButton(R.string.button_no, null /* we do nothing if the user rejects */ );
        return builder.create();
    }

    /**
     * @return true if the Activity is started or a dialog is shown. false if the Activity fails to start.
     */
    public static boolean showDialogOrAndroidChooser(final Context context, final FragmentManager fragmentManager,
            final Intent intent) {
        final Tab selectedTab = Tabs.getInstance().getSelectedTab();
        if (selectedTab == null || !selectedTab.isPrivate()) {
            return ActivityHandlerHelper.startIntentAndCatch(LOGTAG, context, intent);
        }

        final PackageManager pm = context.getPackageManager();
        final List<ResolveInfo> matchingActivities = pm.queryIntentActivities(intent, 0);
        if (matchingActivities.size() == 1) {
            final ExternalIntentDuringPrivateBrowsingPromptFragment fragment = new ExternalIntentDuringPrivateBrowsingPromptFragment();

            final Bundle args = new Bundle(2);
            args.putCharSequence(KEY_APPLICATION_NAME, matchingActivities.get(0).loadLabel(pm));
            args.putParcelable(KEY_INTENT, intent);
            fragment.setArguments(args);

            fragment.show(fragmentManager, FRAGMENT_TAG);
            // We don't know the results of the user interaction with the fragment so just return true.
            return true;
        } else if (matchingActivities.size() > 1) {
            // We want to show the Android Intent Chooser. However, we have no way of distinguishing regular tabs from
            // private tabs to the chooser. Thus, if a user chooses "Always" in regular browsing mode, the chooser will
            // not be shown and the URL will be opened. Therefore we explicitly show the chooser (which notably does not
            // have an "Always" option).
            final String androidChooserTitle =
                    context.getResources().getString(R.string.intent_uri_private_browsing_multiple_match_title);
            final Intent chooserIntent = Intent.createChooser(intent, androidChooserTitle);
            return ActivityHandlerHelper.startIntentAndCatch(LOGTAG, context, chooserIntent);
        } else {
            // Normally, we show about:neterror when an Intent does not resolve
            // but we don't have the references here to do that so log instead.
            Log.w(LOGTAG, "showDialogOrAndroidChooser unexpectedly called with Intent that does not resolve");
            return false;
        }
    }
}
