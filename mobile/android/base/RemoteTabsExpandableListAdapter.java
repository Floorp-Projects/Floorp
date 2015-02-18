/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;



import android.text.format.DateUtils;
import org.mozilla.gecko.db.RemoteClient;
import org.mozilla.gecko.db.RemoteTab;
import org.mozilla.gecko.home.TwoLinePageRow;

import android.content.Context;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseExpandableListAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.GregorianCalendar;
import java.util.List;

/**
 * An adapter that populates group and child views with remote client and tab
 * data maintained in a monolithic static array.
 * <p>
 * The group and child view resources are parameters to allow future
 * specialization to home fragment styles.
 */
public class RemoteTabsExpandableListAdapter extends BaseExpandableListAdapter {
    /**
     * If a device claims to have synced before this date, we will assume it has never synced.
     */
    private static final Date EARLIEST_VALID_SYNCED_DATE;
    static {
        final Calendar c = GregorianCalendar.getInstance();
        c.set(2000, Calendar.JANUARY, 1, 0, 0, 0);
        EARLIEST_VALID_SYNCED_DATE = c.getTime();
    }
    protected final ArrayList<RemoteClient> clients;
    private final boolean showGroupIndicator;
    protected int groupLayoutId;
    protected int childLayoutId;

    public static class GroupViewHolder {
        final TextView nameView;
        final TextView lastModifiedView;
        final ImageView deviceTypeView;
        final ImageView deviceExpandedView;

        public GroupViewHolder(View view) {
            nameView = (TextView) view.findViewById(R.id.client);
            lastModifiedView = (TextView) view.findViewById(R.id.last_synced);
            deviceTypeView = (ImageView) view.findViewById(R.id.device_type);
            deviceExpandedView = (ImageView) view.findViewById(R.id.device_expanded);
        }
    }

    /**
     * Construct a new adapter.
     * <p>
     * It's fine to create with clients to be null, and then to use
     * {@link RemoteTabsExpandableListAdapter#replaceClients(List)} to
     * update this list of clients.
     *
     * @param groupLayoutId
     * @param childLayoutId
     * @param clients
     *            initial list of clients; can be null.
     * @param showGroupIndicator
     */
    public RemoteTabsExpandableListAdapter(int groupLayoutId, int childLayoutId, List<RemoteClient> clients, boolean showGroupIndicator) {
        this.groupLayoutId = groupLayoutId;
        this.childLayoutId = childLayoutId;
        this.clients = new ArrayList<>();
        if (clients != null) {
            this.clients.addAll(clients);
        }
        this.showGroupIndicator = showGroupIndicator;
    }

    public void replaceClients(List<RemoteClient> clients) {
        this.clients.clear();
        if (clients != null) {
            this.clients.addAll(clients);
            this.notifyDataSetChanged();
        } else {
            this.notifyDataSetInvalidated();
        }
    }

    @Override
    public boolean hasStableIds() {
        return false; // Client GUIDs are stable, but tab hashes are not.
    }

    @Override
    public long getGroupId(int groupPosition) {
        return clients.get(groupPosition).guid.hashCode();
    }

    @Override
    public int getGroupCount() {
        return clients.size();
    }

    @Override
    public Object getGroup(int groupPosition) {
        return clients.get(groupPosition);
    }

    @Override
    public int getChildrenCount(int groupPosition) {
        return clients.get(groupPosition).tabs.size();
    }

    @Override
    public View getGroupView(int groupPosition, boolean isExpanded, View convertView, ViewGroup parent) {
        final Context context = parent.getContext();
        final View view;
        if (convertView != null) {
            view = convertView;
        } else {
            final LayoutInflater inflater = LayoutInflater.from(context);
            view = inflater.inflate(groupLayoutId, parent, false);
            final GroupViewHolder holder = new GroupViewHolder(view);
            view.setTag(holder);
        }

        final RemoteClient client = clients.get(groupPosition);
        updateClientsItemView(isExpanded, context, view, client);

        return view;
    }

    public void updateClientsItemView(final boolean isExpanded, final Context context, final View view, final RemoteClient client) {
        final GroupViewHolder holder = (GroupViewHolder) view.getTag();
        if (!showGroupIndicator) {
            view.setBackgroundColor(0);
        }

        // UI elements whose state depends on isExpanded, roughly from left to
        // right: device type icon; client name text color; expanded state
        // indicator.
        final int deviceTypeResId;
        final int textColorResId;
        final int deviceExpandedResId;

        if (isExpanded && !client.tabs.isEmpty()) {
            deviceTypeResId = "desktop".equals(client.deviceType) ? R.drawable.sync_desktop : R.drawable.sync_mobile;
            textColorResId = R.color.home_text_color;
            deviceExpandedResId = R.drawable.home_group_expanded;
        } else {
            deviceTypeResId = "desktop".equals(client.deviceType) ? R.drawable.sync_desktop_inactive : R.drawable.sync_mobile_inactive;
            textColorResId = R.color.home_text_color_disabled;
            deviceExpandedResId = R.drawable.home_group_collapsed;
        }

        // Now update the UI.
        holder.nameView.setText(client.name);
        holder.nameView.setTextColor(context.getResources().getColor(textColorResId));

        final long now = System.currentTimeMillis();

        // It's OK to access the DB on the main thread here, as we're just
        // getting a string.
        final GeckoProfile profile = GeckoProfile.get(context);
        holder.lastModifiedView.setText(this.getLastSyncedString(context, now, client.lastModified));

        // These views exists only in some of our group views: they are present
        // for the home panel groups and not for the tabs panel groups.
        // Therefore, we must handle null.
        if (holder.deviceTypeView != null) {
            holder.deviceTypeView.setImageResource(deviceTypeResId);
        }

        if (showGroupIndicator && holder.deviceExpandedView != null) {
            // If there are no tabs to display, don't show an indicator at all.
            holder.deviceExpandedView.setImageResource(client.tabs.isEmpty() ? 0 : deviceExpandedResId);
        }
    }

    @Override
    public boolean isChildSelectable(int groupPosition, int childPosition) {
        return true;
    }

    @Override
    public Object getChild(int groupPosition, int childPosition) {
        return clients.get(groupPosition).tabs.get(childPosition);
    }

    @Override
    public long getChildId(int groupPosition, int childPosition) {
        return clients.get(groupPosition).tabs.get(childPosition).hashCode();
    }

    @Override
    public View getChildView(int groupPosition, int childPosition, boolean isLastChild, View convertView, ViewGroup parent) {
        final Context context = parent.getContext();
        final View view;
        if (convertView != null) {
            view = convertView;
        } else {
            final LayoutInflater inflater = LayoutInflater.from(context);
            view = inflater.inflate(childLayoutId, parent, false);
        }

        final RemoteClient client = clients.get(groupPosition);
        final RemoteTab tab = client.tabs.get(childPosition);

        // The view is a TwoLinePageRow only for some of our child views: it's
        // present for the home panel children and not for the tabs panel
        // children. Therefore, we must handle one case manually.
        if (view instanceof TwoLinePageRow) {
            ((TwoLinePageRow) view).update(tab.title, tab.url);
        } else {
            final TextView titleView = (TextView) view.findViewById(R.id.title);
            titleView.setText(TextUtils.isEmpty(tab.title) ? tab.url : tab.title);

            final TextView urlView = (TextView) view.findViewById(R.id.url);
            urlView.setText(tab.url);
        }

        return view;
    }

    /**
     * Return a relative "Last synced" time span for the given tab record.
     *
     * @param now local time.
     * @param time to format string for.
     * @return string describing time span
     */
    public String getLastSyncedString(Context context, long now, long time) {
        if (new Date(time).before(EARLIEST_VALID_SYNCED_DATE)) {
            return context.getString(R.string.remote_tabs_never_synced);
        }
        final CharSequence relativeTimeSpanString = DateUtils.getRelativeTimeSpanString(time, now, DateUtils.MINUTE_IN_MILLIS);
        return context.getResources().getString(R.string.remote_tabs_last_synced, relativeTimeSpanString);
    }
}
