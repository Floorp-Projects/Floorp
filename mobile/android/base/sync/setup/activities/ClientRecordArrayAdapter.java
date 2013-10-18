/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup.activities;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

import org.mozilla.gecko.R;
import org.mozilla.gecko.sync.repositories.domain.ClientRecord;

import android.content.Context;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.TextView;

public class ClientRecordArrayAdapter extends ArrayAdapter<ClientRecord> {
  public static final String LOG_TAG = "ClientRecArrayAdapter";

  private boolean[] checkedItems;
  private SendTabActivity sendTabActivity;

  public ClientRecordArrayAdapter(Context context,
                                  int textViewResourceId) {
    super(context, textViewResourceId, new ArrayList<ClientRecord>());
    this.checkedItems = new boolean[0];
    this.sendTabActivity = (SendTabActivity) context;
  }

  public synchronized void setClientRecordList(final Collection<ClientRecord> clientRecordList) {
    this.clear();
    this.checkedItems = new boolean[clientRecordList.size()];
    for (ClientRecord clientRecord : clientRecordList) {
      this.add(clientRecord);
    }
    this.notifyDataSetChanged();
  }

  /**
   * If we have only a single client record in the list, mark it as checked.
   */
  public synchronized void checkItem(final int position, boolean checked) throws ArrayIndexOutOfBoundsException {
    if (position < 0 ||
        position >= checkedItems.length) {
      throw new ArrayIndexOutOfBoundsException(position);
    }

    if (setRowChecked(position, true)) {
      this.notifyDataSetChanged();
    }
  }

  /**
   * Set the specified row to the specified checked state.
   * @param position an index.
   * @param checked whether the checkbox should be checked.
   * @return <code>true</code> if the state changed, <code>false</code> if the
   *         box was already in the requested state.
   */
  protected synchronized boolean setRowChecked(int position, boolean checked) {
    boolean current = checkedItems[position];
    if (current == checked) {
      return false;
    }

    checkedItems[position] = checked;
    sendTabActivity.enableSend(getNumCheckedGUIDs() > 0);

    return true;
  }

  @Override
  public View getView(final int position, View convertView, ViewGroup parent) {
    final Context context = this.getContext();

    // Reuse View objects if they exist.
    View row = convertView;
    if (row == null) {
      row = View.inflate(context, R.layout.sync_list_item, null);
      setSelectable(row, true);
      row.setBackgroundResource(android.R.drawable.menuitem_background);
    }

    final ClientRecord clientRecord = this.getItem(position);
    ImageView clientType = (ImageView) row.findViewById(R.id.img);
    TextView clientName = (TextView) row.findViewById(R.id.client_name);

    // Set up checkbox and restore stored state.
    CheckBox checkbox = (CheckBox) row.findViewById(R.id.check);
    checkbox.setChecked(checkedItems[position]);
    setSelectable(checkbox, false);

    clientName.setText(clientRecord.name);
    clientType.setImageResource(getImage(clientRecord));

    row.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View view) {
        final CheckBox item = (CheckBox) view.findViewById(R.id.check);

        // Update the checked item, both in the UI and in our internal state.
        final boolean checked = !item.isChecked();    // Because it hasn't happened yet.
        item.setChecked(checked);
        setRowChecked(position, checked);
      }
    });

    return row;
  }

  /**
   * Get list of checked GUIDs.
   *
   * @return non-null list.
   */
  public synchronized List<String> getCheckedGUIDs() {
    final List<String> guids = new ArrayList<String>();
    for (int i = 0; i < checkedItems.length; i++) {
      if (checkedItems[i]) {
        guids.add(this.getItem(i).guid);
      }
    }
    return guids;
  }

  /**
   * Get number of checked GUIDs.
   *
   * @return non-negative integer.
   */
  public synchronized int getNumCheckedGUIDs() {
    int numCheckedGUIDs = 0;
    for (int i = 0; i < checkedItems.length; i++) {
      if (checkedItems[i]) {
        numCheckedGUIDs += 1;
      }
    }
    return numCheckedGUIDs;
  }

  private int getImage(ClientRecord record) {
    if ("mobile".equals(record.type)) {
      return R.drawable.sync_mobile;
    }
    return R.drawable.sync_desktop;
  }

  private void setSelectable(View view, boolean selectable) {
    view.setClickable(selectable);
    view.setFocusable(selectable);
  }
}
