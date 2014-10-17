/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.overlays.ui;

import static org.mozilla.gecko.overlays.ui.SendTabList.State.LIST;
import static org.mozilla.gecko.overlays.ui.SendTabList.State.LOADING;
import static org.mozilla.gecko.overlays.ui.SendTabList.State.SHOW_DEVICES;

import java.util.Arrays;

import org.mozilla.gecko.Assert;
import org.mozilla.gecko.R;
import org.mozilla.gecko.overlays.service.sharemethods.ParcelableClientRecord;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.widget.ListAdapter;
import android.widget.ListView;

/**
 * The SendTab button has a few different states depending on the available devices (and whether
 * we've loaded them yet...)
 *
 * Initially, the view resembles a disabled button. (the LOADING state)
 * Once state is loaded from Sync's database, we know how many devices the user may send their tab
 * to.
 *
 * If there are no targets, the user was found to not have a Sync account, or their Sync account is
 * in a state that prevents it from being able to send a tab, we enter the NONE state and display
 * a generic button which launches an appropriate activity to fix the situation when tapped (such
 * as the set up Sync wizard).
 *
 * If the number of targets does not MAX_INLINE_SYNC_TARGETS, we present a button for each of them.
 * (the LIST state)
 *
 * Otherwise, we enter the SHOW_DEVICES state, in which we display a "Send to other devices" button
 * that takes the user to a menu for selecting a target device from their complete list of many
 * devices.
 */
public class SendTabList extends ListView {
    @SuppressWarnings("unused")
    private static final String LOGTAG = "GeckoSendTabList";

    // The maximum number of target devices to show in the main list. Further devices are available
    // from a secondary menu.
    public static final int MAXIMUM_INLINE_ELEMENTS = 2;

    private SendTabDeviceListArrayAdapter clientListAdapter;

    // Listener to fire when a share target is selected (either directly or via the prompt)
    private SendTabTargetSelectedListener listener;

    private final State currentState = LOADING;

    /**
     * Enum defining the states this view may occupy.
     */
    public enum State {
        // State when no sync targets exist (a generic "Send to Firefox Sync" button which launches
        // an activity to set it up)
        NONE,

        // As NONE, but disabled. Initial state. Used until we get information from Sync about what
        // we really want.
        LOADING,

        // A list of devices to share to.
        LIST,

        // A single button prompting the user to select a device to share to.
        SHOW_DEVICES
    }

    public SendTabList(Context context) {
        super(context);
    }

    public SendTabList(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void setAdapter(ListAdapter adapter) {
        Assert.isTrue(adapter instanceof SendTabDeviceListArrayAdapter);

        clientListAdapter = (SendTabDeviceListArrayAdapter) adapter;
        super.setAdapter(adapter);
    }

    public void setSendTabTargetSelectedListener(SendTabTargetSelectedListener aListener) {
        listener = aListener;
    }

    public void switchState(State state) {
        if (state == currentState) {
            return;
        }

        clientListAdapter.switchState(state);
        if (state == SHOW_DEVICES) {
            clientListAdapter.setDialog(getDialog());
        }
    }

    public void setSyncClients(final ParcelableClientRecord[] c) {
        final ParcelableClientRecord[] clients = c == null ? new ParcelableClientRecord[0] : c;

        clientListAdapter.setClientRecordList(Arrays.asList(clients));

        if (clients.length <= MAXIMUM_INLINE_ELEMENTS) {
            // Show the list of devices in-line.
            switchState(LIST);
            return;
        }

        // Just show a button to launch the list of devices to choose from.
        switchState(SHOW_DEVICES);
    }

    /**
     * Get an AlertDialog listing all devices, allowing the user to select the one they want.
     * Used when more than MAXIMUM_INLINE_ELEMENTS devices are found (to avoid displaying them all
     * inline and looking crazy).
     */
    public AlertDialog getDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(getContext());

        final ParcelableClientRecord[] records = clientListAdapter.toArray();
        final String[] dialogElements = new String[records.length];

        for (int i = 0; i < records.length; i++) {
            dialogElements[i] = records[i].name;
        }

        builder.setTitle(R.string.overlay_share_select_device)
               .setItems(dialogElements, new DialogInterface.OnClickListener() {
                   @Override
                   public void onClick(DialogInterface dialog, int index) {
                       listener.onSendTabTargetSelected(records[index].guid);
                   }
               });

        return builder.create();
    }

    /**
     * Prevent scrolling of this ListView.
     */
    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        if (ev.getAction() == MotionEvent.ACTION_MOVE) {
            return true;
        }

        return super.dispatchTouchEvent(ev);
    }
}
