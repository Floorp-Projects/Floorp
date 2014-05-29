/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabspanel;

import org.mozilla.gecko.R;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.sync.setup.SyncAccounts;
import org.mozilla.gecko.tabspanel.TabsPanel.PanelView;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

/**
 * This panel, which is a {@link TabsPanel.PanelView}, chooses which underlying
 * PanelView to show based on the current account state, and forwards the appropriate
 * calls to the currently visible panel.
 */
class RemoteTabsPanel extends FrameLayout implements PanelView {
    private enum RemotePanelType {
        SETUP,
        VERIFICATION,
        CONTAINER
    }

    private PanelView currentPanel;
    private RemotePanelType currentPanelType;

    private TabsPanel tabsPanel;

    public RemoteTabsPanel(Context context, AttributeSet attrs) {
        super(context, attrs);
        updateCurrentPanel();
    }

    @Override
    public void setTabsPanel(TabsPanel panel) {
        tabsPanel = panel;
        currentPanel.setTabsPanel(panel);
    }

    @Override
    public void show() {
        updateCurrentPanel();
        currentPanel.show();
        setVisibility(View.VISIBLE);
    }

    @Override
    public void hide() {
        setVisibility(View.GONE);
        currentPanel.hide();
    }

    @Override
    public boolean shouldExpand() {
        return currentPanel.shouldExpand();
    }

    private void updateCurrentPanel() {
        final RemotePanelType newPanelType = getPanelTypeFromAccountState();
        if (newPanelType != currentPanelType) {
            // The current panel should be null the first time this is called.
            if (currentPanel != null) {
                currentPanel.hide();
            }
            removeAllViews();

            currentPanelType = newPanelType;
            currentPanel = inflatePanel(currentPanelType);
            currentPanel.setTabsPanel(tabsPanel);
            addView((View) currentPanel);
        }
    }

    private RemotePanelType getPanelTypeFromAccountState() {
        final Context context = getContext();
        final State accountState = FirefoxAccounts.getFirefoxAccountState(context);
        if (accountState == null) {
            // If old Sync exists, we want to show their synced tabs,
            // rather than the new Sync setup screen.
            if (SyncAccounts.syncAccountsExist(context)) {
                return RemotePanelType.CONTAINER;
            } else {
                return RemotePanelType.SETUP;
            }
        }

        if (accountState.getNeededAction() == State.Action.NeedsVerification) {
            return RemotePanelType.VERIFICATION;
        }

        return RemotePanelType.CONTAINER;
    }

    private PanelView inflatePanel(final RemotePanelType panelType) {
        final LayoutInflater inflater = LayoutInflater.from(getContext());
        final View inflatedView;
        switch (panelType) {
            case SETUP:
                inflatedView = inflater.inflate(R.layout.remote_tabs_setup_panel, null);
                break;

            case VERIFICATION:
                inflatedView = inflater.inflate(R.layout.remote_tabs_verification_panel, null);
                break;

            case CONTAINER:
                inflatedView = inflater.inflate(R.layout.remote_tabs_container_panel, null);
                break;

            default:
                throw new IllegalArgumentException("Unknown panelType, " + panelType);
        }

        return (PanelView) inflatedView;
    }
}
