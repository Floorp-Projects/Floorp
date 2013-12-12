/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.widget.ArrowPopup;
import org.mozilla.gecko.widget.DoorHanger;
import org.mozilla.gecko.prompts.PromptInput;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.os.Build;
import android.util.Log;
import android.view.View;
import android.widget.CheckBox;

import java.util.HashSet;
import java.util.List;

public class DoorHangerPopup extends ArrowPopup
                             implements GeckoEventListener,
                                        Tabs.OnTabsChangedListener,
                                        DoorHanger.OnButtonClickListener {
    private static final String LOGTAG = "GeckoDoorHangerPopup";

    // Stores a set of all active DoorHanger notifications. A DoorHanger is
    // uniquely identified by its tabId and value.
    private HashSet<DoorHanger> mDoorHangers;

    // Whether or not the doorhanger popup is disabled.
    private boolean mDisabled;

    DoorHangerPopup(GeckoApp activity) {
        super(activity);

        mDoorHangers = new HashSet<DoorHanger>();

        registerEventListener("Doorhanger:Add");
        registerEventListener("Doorhanger:Remove");
        Tabs.registerOnTabsChangedListener(this);
    }

    void destroy() {
        unregisterEventListener("Doorhanger:Add");
        unregisterEventListener("Doorhanger:Remove");
        Tabs.unregisterOnTabsChangedListener(this);
    }

    /**
     * Temporarily disables the doorhanger popup. If the popup is disabled,
     * it will not be shown to the user, but it will continue to process
     * calls to add/remove doorhanger notifications.
     */
    void disable() {
        mDisabled = true;
        updatePopup();
    }

    /**
     * Re-enables the doorhanger popup.
     */
    void enable() {
        mDisabled = false;
        updatePopup();
    }

    @Override
    public void handleMessage(String event, JSONObject geckoObject) {
        try {
            if (event.equals("Doorhanger:Add")) {
                final int tabId = geckoObject.getInt("tabID");
                final String value = geckoObject.getString("value");
                final String message = geckoObject.getString("message");
                final JSONArray buttons = geckoObject.getJSONArray("buttons");
                final JSONObject options = geckoObject.getJSONObject("options");

                mActivity.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        addDoorHanger(tabId, value, message, buttons, options);
                    }
                });
            } else if (event.equals("Doorhanger:Remove")) {
                final int tabId = geckoObject.getInt("tabID");
                final String value = geckoObject.getString("value");

                mActivity.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        DoorHanger doorHanger = getDoorHanger(tabId, value);
                        if (doorHanger == null)
                            return;

                        removeDoorHanger(doorHanger);
                        updatePopup();
                    }
                });
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    // This callback is automatically executed on the UI thread.
    @Override
    public void onTabChanged(final Tab tab, final Tabs.TabEvents msg, final Object data) {
        switch(msg) {
            case CLOSED:
                // Remove any doorhangers for a tab when it's closed (make
                // a temporary set to avoid a ConcurrentModificationException)
                HashSet<DoorHanger> doorHangersToRemove = new HashSet<DoorHanger>();
                for (DoorHanger dh : mDoorHangers) {
                    if (dh.getTabId() == tab.getId())
                        doorHangersToRemove.add(dh);
                }
                for (DoorHanger dh : doorHangersToRemove) {
                    removeDoorHanger(dh);
                }
                break;

            case LOCATION_CHANGE:
                // Only remove doorhangers if the popup is hidden or if we're navigating to a new URL
                if (!isShowing() || !data.equals(tab.getURL()))
                    removeTransientDoorHangers(tab.getId());

                // Update the popup if the location change was on the current tab
                if (Tabs.getInstance().isSelectedTab(tab))
                    updatePopup();
                break;

            case SELECTED:
                // Always update the popup when a new tab is selected. This will cover cases
                // where a different tab was closed, since we always need to select a new tab.
                updatePopup();
                break;
        }
    }

    /**
     * Adds a doorhanger.
     *
     * This method must be called on the UI thread.
     */
    void addDoorHanger(final int tabId, final String value, final String message,
                       final JSONArray buttons, final JSONObject options) {
        // Don't add a doorhanger for a tab that doesn't exist
        if (Tabs.getInstance().getTab(tabId) == null) {
            return;
        }

        // Replace the doorhanger if it already exists
        DoorHanger oldDoorHanger = getDoorHanger(tabId, value);
        if (oldDoorHanger != null) {
            removeDoorHanger(oldDoorHanger);
        }

        if (!mInflated) {
            init();
        }

        final DoorHanger newDoorHanger = new DoorHanger(mActivity, tabId, value);
        newDoorHanger.setMessage(message);
        newDoorHanger.setOptions(options);

        for (int i = 0; i < buttons.length(); i++) {
            try {
                JSONObject buttonObject = buttons.getJSONObject(i);
                String label = buttonObject.getString("label");
                String tag = String.valueOf(buttonObject.getInt("callback"));
                newDoorHanger.addButton(label, tag, this);
            } catch (JSONException e) {
                Log.e(LOGTAG, "Error creating doorhanger button", e);
            }
        }

        mDoorHangers.add(newDoorHanger);
        mContent.addView(newDoorHanger);

        // Only update the popup if we're adding a notifcation to the selected tab
        if (tabId == Tabs.getInstance().getSelectedTab().getId())
            updatePopup();
    }


    /*
     * DoorHanger.OnButtonClickListener implementation
     */
    @Override
    public void onButtonClick(DoorHanger dh, String tag) {
        JSONObject response = new JSONObject();
        try {
            response.put("callback", tag);

            CheckBox checkBox = dh.getCheckBox();
            // If the checkbox is being used, pass its value
            if (checkBox != null) {
                response.put("checked", checkBox.isChecked());
            }

            List<PromptInput> doorHangerInputs = dh.getInputs();
            if (doorHangerInputs != null) {
                JSONObject inputs = new JSONObject();
                for (PromptInput input : doorHangerInputs) {
                    inputs.put(input.getId(), input.getValue());
                }
                response.put("inputs", inputs);
            }
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error creating onClick response", e);
        }

        GeckoEvent e = GeckoEvent.createBroadcastEvent("Doorhanger:Reply", response.toString());
        GeckoAppShell.sendEventToGecko(e);
        removeDoorHanger(dh);
        updatePopup();
    }

    /**
     * Gets a doorhanger.
     *
     * This method must be called on the UI thread.
     */
    DoorHanger getDoorHanger(int tabId, String value) {
        for (DoorHanger dh : mDoorHangers) {
            if (dh.getTabId() == tabId && dh.getValue().equals(value))
                return dh;
        }

        // If there's no doorhanger for the given tabId and value, return null
        return null;
    }

    /**
     * Removes a doorhanger.
     *
     * This method must be called on the UI thread.
     */
    void removeDoorHanger(final DoorHanger doorHanger) {
        mDoorHangers.remove(doorHanger);
        mContent.removeView(doorHanger);
    }

    /**
     * Removes doorhangers for a given tab.
     *
     * This method must be called on the UI thread.
     */
    void removeTransientDoorHangers(int tabId) {
        // Make a temporary set to avoid a ConcurrentModificationException
        HashSet<DoorHanger> doorHangersToRemove = new HashSet<DoorHanger>();
        for (DoorHanger dh : mDoorHangers) {
            // Only remove transient doorhangers for the given tab
            if (dh.getTabId() == tabId && dh.shouldRemove(isShowing()))
                doorHangersToRemove.add(dh);
        }

        for (DoorHanger dh : doorHangersToRemove) {
            removeDoorHanger(dh);
        }
    }

    /**
     * Updates the popup state.
     *
     * This method must be called on the UI thread.
     */
    void updatePopup() {
        // Bail if the selected tab is null, if there are no active doorhangers,
        // if we haven't inflated the layout yet (this can happen if updatePopup()
        // is called before the runnable from addDoorHanger() runs), or if the
        // doorhanger popup is temporarily disabled.
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab == null || mDoorHangers.size() == 0 || !mInflated || mDisabled) {
            dismiss();
            return;
        }

        // Show doorhangers for the selected tab
        int tabId = tab.getId();
        boolean shouldShowPopup = false;
        for (DoorHanger dh : mDoorHangers) {
            if (dh.getTabId() == tabId) {
                dh.setVisibility(View.VISIBLE);
                shouldShowPopup = true;
            } else {
                dh.setVisibility(View.GONE);
            }
        }
 
        // Dismiss the popup if there are no doorhangers to show for this tab
        if (!shouldShowPopup) {
            dismiss();
            return;
        }

        showDividers();
        if (isShowing()) {
            show();
            return;
        }

        // Make the popup focusable for accessibility. This gets done here
        // so the node can be accessibility focused, but on pre-ICS devices this
        // causes crashes, so it is done after the popup is shown.
        if (Build.VERSION.SDK_INT >= 14) {
            setFocusable(true);
        }

        show();

        if (Build.VERSION.SDK_INT < 14) {
            // Make the popup focusable for keyboard accessibility.
            setFocusable(true);
        }
    }

    //Show all inter-DoorHanger dividers (ie. Dividers on all visible DoorHangers except the last one)
    private void showDividers() {
        int count = mContent.getChildCount();
        DoorHanger lastVisibleDoorHanger = null;

        for (int i = 0; i < count; i++) {
            DoorHanger dh = (DoorHanger) mContent.getChildAt(i);
            dh.showDivider();
            if (dh.getVisibility() == View.VISIBLE) {
                lastVisibleDoorHanger = dh;
            }
        }
        if (lastVisibleDoorHanger != null) {
            lastVisibleDoorHanger.hideDivider();
        }
    }

    private void registerEventListener(String event) {
        GeckoAppShell.getEventDispatcher().registerEventListener(event, this);
    }

    private void unregisterEventListener(String event) {
        GeckoAppShell.getEventDispatcher().unregisterEventListener(event, this);
    }

    @Override
    public void dismiss() {
        // If the popup is focusable while it is hidden, we run into crashes
        // on pre-ICS devices when the popup gets focus before it is shown.
        setFocusable(false);
        super.dismiss();
    }
}
