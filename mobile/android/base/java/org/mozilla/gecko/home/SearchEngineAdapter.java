package org.mozilla.gecko.home;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.v4.content.ContextCompat;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import org.mozilla.gecko.R;

import java.util.Collections;
import java.util.List;

public class SearchEngineAdapter
        extends RecyclerView.Adapter<SearchEngineAdapter.SearchEngineViewHolder> {

    private static final String LOGTAG = SearchEngineAdapter.class.getSimpleName();

    private static final int VIEW_TYPE_SEARCH_ENGINE = 0;
    private static final int VIEW_TYPE_LABEL = 1;
    private final Context mContext;

    private int mContainerWidth;
    private List<SearchEngine> mSearchEngines = Collections.emptyList();

    public void setSearchEngines(List<SearchEngine> searchEngines) {
        mSearchEngines = searchEngines;
        notifyDataSetChanged();
    }

    /**
     * The container width is used for setting the appropriate calculated amount of width that
     * a search engine icon can have. This varies depending on the space available in the
     * {@link SearchEngineBar}. The setter exists for this attribute, in creating the view in the
     * adapter after said calculation is done when the search bar is created.
     * @param iconContainerWidth Width of each search icon.
     */
    void setIconContainerWidth(int iconContainerWidth) {
        mContainerWidth = iconContainerWidth;
    }

    public static class SearchEngineViewHolder extends RecyclerView.ViewHolder {
        final private ImageView faviconView;

        public void bindItem(SearchEngine searchEngine) {
            faviconView.setImageBitmap(searchEngine.getIcon());
            final String desc = itemView.getResources().getString(R.string.search_bar_item_desc,
                searchEngine.getEngineIdentifier());
            itemView.setContentDescription(desc);
        }

        public SearchEngineViewHolder(View itemView) {
            super(itemView);
            faviconView = (ImageView) itemView.findViewById(R.id.search_engine_icon);
        }
    }

    public SearchEngineAdapter(Context context) {
        mContext = context;
    }

    @Override
    public int getItemViewType(int position) {
        return position == 0 ? VIEW_TYPE_LABEL : VIEW_TYPE_SEARCH_ENGINE;
    }

    public SearchEngine getItem(int position) {
        // We omit the first position which is where the label currently is.
        return position == 0 ? null : mSearchEngines.get(position - 1);
    }

    @Override
    public SearchEngineViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        switch (viewType) {
            case VIEW_TYPE_LABEL:
                return new SearchEngineViewHolder(createLabelView(parent));
            case VIEW_TYPE_SEARCH_ENGINE:
                return new SearchEngineViewHolder(createSearchEngineView(parent));
            default:
                throw new IllegalArgumentException("Unknown view type: "  + viewType);
        }
    }

    @Override
    public void onBindViewHolder(SearchEngineViewHolder holder, int position) {
        if (position != 0) {
            holder.bindItem(getItem(position));
        }
    }

    @Override
    public int getItemCount() {
        return mSearchEngines.size() + 1;
    }

    private View createLabelView(ViewGroup parent) {
        View view = LayoutInflater.from(mContext)
                        .inflate(R.layout.search_engine_bar_label, parent, false);
        final Drawable icon = DrawableCompat.wrap(
                ContextCompat.getDrawable(mContext, R.drawable.search_icon_active).mutate());
        DrawableCompat.setTint(icon, ContextCompat.getColor(mContext, R.color.disabled_grey));

        final ImageView iconView = (ImageView) view.findViewById(R.id.search_engine_label);
        iconView.setImageDrawable(icon);
        return view;
    }

    private View createSearchEngineView(ViewGroup parent) {
        View view = LayoutInflater.from(mContext)
                    .inflate(R.layout.search_engine_bar_item, parent, false);

        ViewGroup.LayoutParams params = view.getLayoutParams();
        params.width = mContainerWidth;
        view.setLayoutParams(params);

        return view;
    }
}
