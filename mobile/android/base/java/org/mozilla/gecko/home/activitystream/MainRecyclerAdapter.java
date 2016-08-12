package org.mozilla.gecko.home.activitystream;


import android.content.Context;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.mozilla.gecko.R;

import java.util.Arrays;
import java.util.List;


class MainRecyclerAdapter extends RecyclerView.Adapter<MainRecyclerAdapter.ViewHolder> {

    private final Context context;

    private static final int VIEW_TYPE_TOP_SITES = 0;
    private static final int VIEW_TYPE_HIGHLIGHTS = 1;
    private static final int VIEW_TYPE_HISTORY = 2;

    private static final int VIEW_SIZE_TOP_SITES = 115;
    private static final int VIEW_SIZE_HIGHLIGHTS = 220;
    private static final int VIEW_SIZE_HISTORY = 80;

    private final List<Item> categories = Arrays.asList(
            new Item("Top Sites"),
            new Item("Highlights"),
            new Item("History")
    );

    MainRecyclerAdapter(Context context) {
        this.context = context;
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        View itemView = LayoutInflater
                .from(context)
                .inflate(R.layout.activity_stream_category, parent, false);
        return new ViewHolder(itemView);
    }

    @Override
    public void onBindViewHolder(ViewHolder holder, int position) {
        LinearLayoutManager llm = new LinearLayoutManager(context);
        if (position == VIEW_TYPE_TOP_SITES) {
            holder.vCategoryRv.setAdapter(new TopSitesRecyclerAdapter(context));
            setCorrectedLayoutOrientation(llm, LinearLayoutManager.HORIZONTAL);
            setCorrectedLayoutHeight(context, VIEW_SIZE_TOP_SITES, holder, llm);
        } else if (position == VIEW_TYPE_HIGHLIGHTS) {
            holder.vCategoryRv.setAdapter(new HighlightRecyclerAdapter(context));
            setCorrectedLayoutOrientation(llm, LinearLayoutManager.HORIZONTAL);
            setCorrectedLayoutHeight(context, VIEW_SIZE_HIGHLIGHTS, holder, llm);
        } else if (position == VIEW_TYPE_HISTORY) {
            holder.vCategoryRv.setAdapter(new HistoryRecyclerAdapter(context));
            setCorrectedLayoutOrientation(llm, LinearLayoutManager.VERTICAL);
            setCorrectedLayoutHeight(context, VIEW_SIZE_HISTORY, holder, llm);
        }

        holder.vCategoryRv.setLayoutManager(llm);

        if (position != VIEW_TYPE_HISTORY) {
            holder.vTitle.setText(categories.get(position).getLabel());
            holder.vMore.setVisibility(View.VISIBLE);
        }
    }

    @Override
    public int getItemCount() {
        return categories.size();
    }

    private void setCorrectedLayoutHeight(final Context context,
                                          final int height,
                                          final ViewHolder holder,
                                          final LinearLayoutManager llm) {
        int correctedHeight = height;
        if (isHistoryView(llm)) {
            int holderItemCount;
            holderItemCount = holder.vCategoryRv.getAdapter().getItemCount();
            correctedHeight = height * holderItemCount;
        }
        setHolderRecyclerViewHeight(holder,
                getActualPixelValue(context, correctedHeight));
    }

    private static int getActualPixelValue(Context context, int height) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, height,
                context.getResources().getDisplayMetrics());
    }

    private static void setHolderRecyclerViewHeight(ViewHolder holder, int height) {
        holder.vCategoryRv.setLayoutParams(new LinearLayout.LayoutParams(
                holder.vCategoryRv.getLayoutParams().width, height));
    }

    private static boolean isHistoryView(LinearLayoutManager llm) {
        return llm.getOrientation() == LinearLayoutManager.VERTICAL;
    }

    private void setCorrectedLayoutOrientation(final LinearLayoutManager lm,
                                               final int orientation) {
        lm.setOrientation(orientation);
    }

    static class ViewHolder extends RecyclerView.ViewHolder {
        TextView vTitle;
        TextView vMore;
        RecyclerView vCategoryRv;
        ViewHolder(View itemView) {
            super(itemView);
            vTitle = (TextView) itemView.findViewById(R.id.category_title);
            vMore = (TextView) itemView.findViewById(R.id.category_more_link);
            vCategoryRv = (RecyclerView) itemView.findViewById(R.id.recycler_category);
        }
    }

    static class Item {
        String label;

        Item(String label) {
            setLabel(label);
        }

        public void setLabel(String label) {
            this.label = label;
        }

        public String getLabel() {
            return label;
        }
    }
}