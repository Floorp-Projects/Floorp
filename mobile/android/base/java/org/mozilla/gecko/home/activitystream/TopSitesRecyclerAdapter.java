package org.mozilla.gecko.home.activitystream;

import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.mozilla.gecko.R;

class TopSitesRecyclerAdapter extends RecyclerView.Adapter<TopSitesRecyclerAdapter.ViewHolder> {

    private final Context context;
    private final String[] items = {
            "FastMail",
            "Firefox",
            "Mozilla",
            "Hacker News",
            "Github",
            "YouTube",
            "Google Maps"
    };

    TopSitesRecyclerAdapter(Context context) {
        this.context = context;
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        View v = LayoutInflater
                .from(context)
                .inflate(R.layout.activity_stream_card_top_sites_item, parent, false);
        return new ViewHolder(v);
    }

    @Override
    public void onBindViewHolder(ViewHolder holder, int position) {
        holder.vLabel.setText(items[position]);
    }

    @Override
    public int getItemCount() {
        return items.length;
    }

    static class ViewHolder extends RecyclerView.ViewHolder {
        TextView vLabel;
        ViewHolder(View itemView) {
            super(itemView);
            vLabel = (TextView) itemView.findViewById(R.id.card_row_label);
        }
    }
}

