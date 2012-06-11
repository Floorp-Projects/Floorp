package org.mozilla.gecko.sync.setup.activities;

import org.mozilla.gecko.sync.repositories.domain.ClientRecord;

import org.mozilla.gecko.R;

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
    View view = View.inflate(context, R.layout.sync_list_item, null);
    setSelectable(view, true);
    view.setBackgroundResource(android.R.drawable.menuitem_background);

    final ClientRecord clientRecord = clientRecordList[position];
    ImageView clientType = (ImageView) view.findViewById(R.id.img);
    TextView clientName = (TextView) view.findViewById(R.id.client_name);
    CheckBox checkbox = (CheckBox) view.findViewById(R.id.check);
    setSelectable(checkbox, false);

    clientName.setText(clientRecord.name);
    clientType.setImageResource(getImage(clientRecord));

    view.setOnClickListener(new OnClickListener() {
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

    return view;
  }

  public String[] getCheckedGUIDs() {
    String[] guids = new String[numCheckedGUIDs];
    for (int i = 0, j = 0; i < checkedItems.length; i++) {
      if (checkedItems[i]) {
        guids[j++] = clientRecordList[i].guid;
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