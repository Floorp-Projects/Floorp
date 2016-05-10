/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.HashSet;

import android.text.TextUtils;
import android.widget.PopupWindow;
import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONArray;
import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.widget.AnchoredPopup;
import org.mozilla.gecko.widget.DoorHanger;

import android.content.Context;
import android.util.Log;
import android.view.View;
import org.mozilla.gecko.widget.DoorhangerConfig;

public class DoorHangerPopup extends AnchoredPopup
                             implements GeckoEventListener,
                                        Tabs.OnTabsChangedListener,
                                        PopupWindow.OnDismissListener,
                                        DoorHanger.OnButtonClickListener {
    private static final String LOGTAG = "GeckoDoorHangerPopup";

    // Stores a set of all active DoorHanger notifications. A DoorHanger is
    // uniquely identified by its tabId and value.
    private final HashSet<DoorHanger> mDoorHangers;

    // Whether or not the doorhanger popup is disabled.
    private boolean mDisabled;

    public DoorHangerPopup(Context context) {
        super(context);

        mDoorHangers = new HashSet<DoorHanger>();

        EventDispatcher.getInstance().registerGeckoThreadListener(this,
            "Doorhanger:Add",
            "Doorhanger:Remove");
        Tabs.registerOnTabsChangedListener(this);

        setOnDismissListener(this);
    }

    void destroy() {
        EventDispatcher.getInstance().unregisterGeckoThreadListener(this,
            "Doorhanger:Add",
            "Doorhanger:Remove");
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
                final DoorhangerConfig config = makeConfigFromJSON(geckoObject);

                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        addDoorHanger(config);
                    }
                });
            } else if (event.equals("Doorhanger:Remove")) {
                final int tabId = geckoObject.getInt("tabID");
                final String value = geckoObject.getString("value");

                ThreadUtils.postToUiThread(new Runnable() {
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

    private DoorhangerConfig makeConfigFromJSON(JSONObject json) throws JSONException {
        final int tabId = json.getInt("tabID");
        final String id = json.getString("value");

        final String typeString = json.optString("category");
        DoorHanger.Type doorhangerType = DoorHanger.Type.DEFAULT;
        if (DoorHanger.Type.LOGIN.toString().equals(typeString)) {
            doorhangerType = DoorHanger.Type.LOGIN;
        } else if (DoorHanger.Type.GEOLOCATION.toString().equals(typeString)) {
            doorhangerType = DoorHanger.Type.GEOLOCATION;
        } else if (DoorHanger.Type.DESKTOPNOTIFICATION2.toString().equals(typeString)) {
            doorhangerType = DoorHanger.Type.DESKTOPNOTIFICATION2;
        } else if (DoorHanger.Type.WEBRTC.toString().equals(typeString)) {
            doorhangerType = DoorHanger.Type.WEBRTC;
        } else if (DoorHanger.Type.VIBRATION.toString().equals(typeString)) {
            doorhangerType = DoorHanger.Type.VIBRATION;
        }

        final DoorhangerConfig config = new DoorhangerConfig(tabId, id, doorhangerType, this);

        config.setMessage(json.getString("message"));
        config.setOptions(json.getJSONObject("options"));

        final JSONArray buttonArray = json.getJSONArray("buttons");
        int numButtons = buttonArray.length();
        if (numButtons > 2) {
            Log.e(LOGTAG, "Doorhanger can have a maximum of two buttons!");
            numButtons = 2;
        }

        for (int i = 0; i < numButtons; i++) {
            final JSONObject buttonJSON = buttonArray.getJSONObject(i);
            final boolean isPositive = buttonJSON.optBoolean("positive", false);
            config.setButton(buttonJSON.getString("label"), buttonJSON.getInt("callback"), isPositive);
        }

        return config;
    }

    // This callback is automatically executed on the UI thread.
    @Override
    public void onTabChanged(final Tab tab, final Tabs.TabEvents msg, final String data) {
        switch (msg) {
            case CLOSED:
                // Remove any doorhangers for a tab when it's closed (make
                // a temporary set to avoid a ConcurrentModificationException)
                removeTabDoorHangers(tab.getId(), true);
                break;

            case LOCATION_CHANGE:
                // Only remove doorhangers if the popup is hidden or if we're navigating to a new URL
                if (!isShowing() || !data.equals(tab.getURL()))
                    removeTabDoorHangers(tab.getId(), false);

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
    void addDoorHanger(DoorhangerConfig config) {
        final int tabId = config.getTabId();
        // Don't add a doorhanger for a tab that doesn't exist
        if (Tabs.getInstance().getTab(tabId) == null) {
            return;
        }

        // Replace the doorhanger if it already exists
        DoorHanger oldDoorHanger = getDoorHanger(tabId, config.getId());
        if (oldDoorHanger != null) {
            removeDoorHanger(oldDoorHanger);
        }

        if (!mInflated) {
            init();
        }

        final DoorHanger newDoorHanger = DoorHanger.Get(mContext, config);

        mDoorHangers.add(newDoorHanger);
        mContent.addView(newDoorHanger);

        // Only update the popup if we're adding a notification to the selected tab
        if (tabId == Tabs.getInstance().getSelectedTab().getId())
            updatePopup();
    }


    /*
     * DoorHanger.OnButtonClickListener implementation
     */
    @Override
    public void onButtonClick(JSONObject response, DoorHanger doorhanger) {
        GeckoAppShell.notifyObservers("Doorhanger:Reply", response.toString());
        removeDoorHanger(doorhanger);
        updatePopup();
    }

    /**
     * Gets a doorhanger.
     *
     * This method must be called on the UI thread.
     */
    DoorHanger getDoorHanger(int tabId, String value) {
        for (DoorHanger dh : mDoorHangers) {
            if (dh.getTabId() == tabId && dh.getIdentifier().equals(value))
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
     * @param tabId identifier of the tab to remove doorhangers from
     * @param forceRemove boolean for force-removing tabs. If true, all doorhangers associated
     *                    with  the tab specified are removed; if false, only remove the doorhangers
     *                    that are not persistent, as specified by the doorhanger options.
     *
     * This method must be called on the UI thread.
     */
    void removeTabDoorHangers(int tabId, boolean forceRemove) {
        // Make a temporary set to avoid a ConcurrentModificationException
        HashSet<DoorHanger> doorHangersToRemove = new HashSet<DoorHanger>();
        for (DoorHanger dh : mDoorHangers) {
            // Only remove transient doorhangers for the given tab
            if (dh.getTabId() == tabId
                && (forceRemove || (!forceRemove && dh.shouldRemove(isShowing())))) {
                    doorHangersToRemove.add(dh);
            }
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
        DoorHanger firstDoorhanger = null;
        for (DoorHanger dh : mDoorHangers) {
            if (dh.getTabId() == tabId) {
                dh.setVisibility(View.VISIBLE);
                shouldShowPopup = true;
                if (firstDoorhanger == null) {
                    firstDoorhanger = dh;
                } else {
                    dh.hideTitle();
                }
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

        final String baseDomain = tab.getBaseDomain();

        if (TextUtils.isEmpty(baseDomain)) {
            firstDoorhanger.hideTitle();
        } else {
            firstDoorhanger.showTitle(tab.getFavicon(), baseDomain);
        }

        if (isShowing()) {
            show();
            return;
        }

        // Make the popup focusable for accessibility. This gets done here
        // so the node can be accessibility focused, but on pre-ICS devices this
        // causes crashes, so it is done after the popup is shown.
        if (Versions.feature14Plus) {
            setFocusable(true);
        }

        show();
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

    @Override
    public void onDismiss() {
        final int tabId = Tabs.getInstance().getSelectedTab().getId();
        removeTabDoorHangers(tabId, true);
    }

    @Override
    public void dismiss() {
        // If the popup is focusable while it is hidden, we run into crashes
        // on pre-ICS devices when the popup gets focus before it is shown.
        setFocusable(false);
        super.dismiss();
    }
}
