package org.mozilla.gecko.home.activitystream;


import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.mozilla.gecko.R;

class HighlightRecyclerAdapter extends RecyclerView.Adapter<HighlightRecyclerAdapter.ViewHolder> {

    private final Context context;
    private final String[] items = {
            "What Do The Reviews Have To Say About the New Ghostbusters",
            "The Dark Secrets Of This Now-Empty Island in Maine",
            "How to Prototype a Game in Under 7 Days (2005)",
            "Fuchsia, a new operating system"
    };
    private final String[] items_time = {
            "3h",
            "3h",
            "3h",
            "3h"
    };

    HighlightRecyclerAdapter(Context context) {
        this.context = context;
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        View v = LayoutInflater
                .from(context)
                .inflate(R.layout.activity_stream_card_highlights_item, parent, false);
        return new ViewHolder(v);
    }

    @Override
    public void onBindViewHolder(ViewHolder holder, int position) {
        holder.vLabel.setText(items[position]);
        holder.vTimeSince.setText(items_time[position]);
        int res = context.getResources()
                .getIdentifier("thumb_" + (position + 1), "drawable", context.getPackageName());
        holder.vThumbnail.setImageResource(res);
    }

    @Override
    public int getItemCount() {
        return items.length;
    }

    static class ViewHolder extends RecyclerView.ViewHolder {
        TextView vLabel;
        TextView vTimeSince;
        ImageView vThumbnail;

        public ViewHolder(View itemView) {
            super(itemView);
            vLabel = (TextView) itemView.findViewById(R.id.card_highlights_label);
            vTimeSince = (TextView) itemView.findViewById(R.id.card_highlights_time_since);
            vThumbnail = (ImageView) itemView.findViewById(R.id.card_highlights_thumbnail);
        }
    }
}

