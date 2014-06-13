/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabspanel;

import org.mozilla.gecko.R;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.tabspanel.TabsPanel.PanelView;

import android.content.Context;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/**
 * A tabs panel which allows a user to get resend the verification email
 * to confirm a Firefox Account. Currently used as one sub-panel in a sequence
 * contained by the {@link RemoteTabsPanel}.
 */
class RemoteTabsVerificationPanel extends ScrollView implements PanelView {
    private static final String LOG_TAG = RemoteTabsVerificationPanel.class.getSimpleName();

    private LinearLayout containingLayout;

    private TabsPanel tabsPanel;

    public RemoteTabsVerificationPanel(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        containingLayout = (LinearLayout) findViewById(R.id.remote_tabs_verification_containing_layout);

        final View resendLink = containingLayout.findViewById(R.id.remote_tabs_confirm_resend);
        resendLink.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                final State accountState = FirefoxAccounts.getFirefoxAccountState(getContext());
                final State.Action neededAction = accountState.getNeededAction();
                if (accountState.getNeededAction() != State.Action.NeedsVerification) {
                    autoHideTabsPanelOnUnexpectedState("Account expected to need verification " +
                            "on resend, but action was " + neededAction + " instead.");
                    return;
                }

                if (!FirefoxAccounts.resendVerificationEmail(getContext())) {
                    autoHideTabsPanelOnUnexpectedState("Account DNE when resending verification email.");
                    return;
                }
            }
        });
    }

    private void refresh() {
        final TextView verificationView =
                (TextView) containingLayout.findViewById(R.id.remote_tabs_confirm_verification);
        final String email = FirefoxAccounts.getFirefoxAccountEmail(getContext());
        if (email == null) {
            autoHideTabsPanelOnUnexpectedState("Account email DNE on View refresh.");
            return;
        }

        final String text = getResources().getString(
                R.string.fxaccount_confirm_account_verification_link, email);
        verificationView.setText(text);
    }

    /**
     * Hides the tabs panel and logs the given String.
     *
     * As the name suggests, this method should be only be used for unexpected states!
     * We hide the tabs panel on unexpected states as the best of several evils - hiding
     * the tabs panel communicates to the user, "Hey, that's a strange bug!" and, if they're
     * curious enough, will reopen the RemoteTabsPanel, refreshing its contents. Since we're
     * in a strange state, we may already be screwed, but it's better than some alternatives like:
     *   * Crashing
     *   * Hiding the resources which allow invalid state (e.g. resend link, email text)
     *   * Attempting to refresh the RemoteTabsPanel, possibly starting an infinite loop.
     *
     * @param log The message to log.
     */
    private void autoHideTabsPanelOnUnexpectedState(final String log) {
        Log.w(LOG_TAG, "Unexpected state: " + log + " Closing the tabs panel.");

        if (tabsPanel != null) {
            tabsPanel.autoHidePanel();
        }
    }

    @Override
    public void setTabsPanel(TabsPanel panel) {
        tabsPanel = panel;
    }

    @Override
    public void show() {
        refresh();
        setVisibility(View.VISIBLE);
    }

    @Override
    public void hide() {
        setVisibility(View.GONE);
    }

    @Override
    public boolean shouldExpand() {
        return containingLayout.getOrientation() == LinearLayout.VERTICAL;
    }
}
