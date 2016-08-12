package org.mozilla.gecko.home.activitystream;

import android.content.Context;
import android.support.v7.widget.CardView;
import android.support.v7.widget.RecyclerView;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.mozilla.gecko.R;

class HistoryRecyclerAdapter extends RecyclerView.Adapter<HistoryRecyclerAdapter.ViewHolder> {

    private final Context context;
    private final String[] items = {
            "What Do The Reviews Have To Say About the New Ghostbusters",
            "New “Leaf” Is More Efficient Than Natural Photosynthesis",
            "Zero-cost futures in Rust",
            "Indie Hackers: Learn how developers are making money",
            "The fight to cheat death is heating up"
    };

    HistoryRecyclerAdapter(Context context) {
        this.context = context;
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        View v = LayoutInflater
                .from(context)
                .inflate(R.layout.activity_stream_card_history_item, parent, false);
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
            vLabel = (TextView) itemView.findViewById(R.id.card_history_label);
        }
    }
}

