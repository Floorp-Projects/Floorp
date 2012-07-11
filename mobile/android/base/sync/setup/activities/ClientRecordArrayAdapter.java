package org.mozilla.gecko.sync.setup.activities;

import java.util.ArrayList;
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

public class ClientRecordArrayAdapter extends ArrayAdapter<Object> {
  public static final String LOG_TAG = "ClientRecArrayAdapter";
  private ClientRecord[] clientRecordList;
  private boolean[] checkedItems;
  private int numCheckedGUIDs;
  private SendTabActivity sendTabActivity;

  public ClientRecordArrayAdapter(Context context, int textViewResourceId,
      ClientRecord[] clientRecordList) {
    super(context, textViewResourceId, clientRecordList);
    this.sendTabActivity = (SendTabActivity) context;
    this.clientRecordList = clientRecordList;
    this.checkedItems = new boolean[clientRecordList.length];
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

    final ClientRecord clientRecord = clientRecordList[position];
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
        CheckBox item = (CheckBox) view.findViewById(R.id.check);

        // Update the checked item, both in the UI and in the checkedItems array.
        boolean newCheckedValue = !item.isChecked();
        item.setChecked(newCheckedValue);
        checkedItems[position] = newCheckedValue;

        numCheckedGUIDs += newCheckedValue ? 1 : -1;
        if (numCheckedGUIDs <= 0) {
          sendTabActivity.enableSend(false);
          return;
        }
        sendTabActivity.enableSend(true);
      }

    });

    return row;
  }

  public List<String> getCheckedGUIDs() {
    final List<String> guids = new ArrayList<String>();
    for (int i = 0; i < checkedItems.length; i++) {
      if (checkedItems[i]) {
        guids.add(clientRecordList[i].guid);
      }
    }
    return guids;
  }

  public int getNumCheckedGUIDs() {
    return numCheckedGUIDs;
  }

  private int getImage(ClientRecord record) {
    if ("mobile".equals(record.type)) {
      return R.drawable.mobile;
    }
    return R.drawable.desktop;
  }

  private void setSelectable(View view, boolean selectable) {
    view.setClickable(selectable);
    view.setFocusable(selectable);
  }
}