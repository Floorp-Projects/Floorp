/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import org.mozilla.gecko.R;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;

import java.util.EnumSet;
import java.util.Locale;

/**
 * A <code>HomeFragment</code> which displays one of a small set of static views
 * in response to different Firefox Account states. When the Firefox Account is
 * healthy and syncing normally, these views should not be shown.
 * <p>
 * This class exists to handle view-specific actions when buttons and links
 * shown by the different static views are clicked. For example, a static view
 * offers to set up a Firefox Account to a user who has no account (Firefox or
 * Sync) on their device.
 * <p>
 * This could be a vanilla <code>Fragment</code>, except it needs to open URLs.
 * To do so, it expects its containing <code>Activity</code> to implement
 * <code>OnUrlOpenListener<code>; to suggest this invariant at compile time, we
 * inherit from <code>HomeFragment</code>.
 */
public class RemoteTabsStaticFragment extends HomeFragment implements OnClickListener {
    @SuppressWarnings("unused")
    private static final String LOGTAG = "GeckoRemoteTabsStatic";

    protected static final String RESOURCE_ID = "resource_id";
    protected static final int DEFAULT_RESOURCE_ID = R.layout.remote_tabs_setup;

    private static final String CONFIRM_ACCOUNT_SUPPORT_URL =
            "https://support.mozilla.org/kb/im-having-problems-confirming-my-firefox-account";

    protected int mLayoutId;

    public static RemoteTabsStaticFragment newInstance(int resourceId) {
        final RemoteTabsStaticFragment fragment = new RemoteTabsStaticFragment();

        final Bundle args = new Bundle();
        args.putInt(RESOURCE_ID, resourceId);
        fragment.setArguments(args);

        return fragment;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final Bundle args = getArguments();
        if (args != null) {
            mLayoutId = args.getInt(RESOURCE_ID, DEFAULT_RESOURCE_ID);
        } else {
            mLayoutId = DEFAULT_RESOURCE_ID;
        }
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(mLayoutId, container, false);
    }

    protected boolean maybeSetOnClickListener(View view, int resourceId) {
        final View button = view.findViewById(resourceId);
        if (button != null) {
            button.setOnClickListener(this);
            return true;
        }
        return false;
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        for (int resourceId : new int[] {
                R.id.remote_tabs_setup_get_started,
                R.id.remote_tabs_setup_old_sync_link,
                R.id.remote_tabs_needs_verification_resend_email,
                R.id.remote_tabs_needs_verification_help,
                R.id.remote_tabs_needs_password_sign_in,
                R.id.remote_tabs_needs_finish_migrating_sign_in, }) {
            maybeSetOnClickListener(view, resourceId);
        }
    }

    @Override
    public void onClick(final View v) {
        final int id = v.getId();
        if (id == R.id.remote_tabs_setup_get_started) {
            // This Activity will redirect to the correct Activity as needed.
            final Intent intent = new Intent(FxAccountConstants.ACTION_FXA_GET_STARTED);
            startActivity(intent);
        } else if (id == R.id.remote_tabs_setup_old_sync_link) {
            final String url = FirefoxAccounts.getOldSyncUpgradeURL(getResources(), Locale.getDefault());
            // Don't allow switch-to-tab.
            final EnumSet<OnUrlOpenListener.Flags> flags = EnumSet.noneOf(OnUrlOpenListener.Flags.class);
            mUrlOpenListener.onUrlOpen(url, flags);
        } else if (id == R.id.remote_tabs_needs_verification_resend_email) {
            // Send a fresh email; this displays a toast, so the user gets feedback.
            FirefoxAccounts.resendVerificationEmail(getActivity());
        } else if (id == R.id.remote_tabs_needs_verification_help) {
            // Don't allow switch-to-tab.
            final EnumSet<OnUrlOpenListener.Flags> flags = EnumSet.noneOf(OnUrlOpenListener.Flags.class);
            mUrlOpenListener.onUrlOpen(CONFIRM_ACCOUNT_SUPPORT_URL, flags);
        } else if (id == R.id.remote_tabs_needs_password_sign_in) {
            final Intent intent = new Intent(FxAccountConstants.ACTION_FXA_UPDATE_CREDENTIALS);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(intent);
        } else if (id == R.id.remote_tabs_needs_finish_migrating_sign_in) {
            final Intent intent = new Intent(FxAccountConstants.ACTION_FXA_FINISH_MIGRATING);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(intent);
        }
    }

    @Override
    protected void load() {
        // We're static, so nothing to do here!
    }
}
