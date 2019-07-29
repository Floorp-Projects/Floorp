package org.mozilla.geckoview_example;

import android.content.Context;
import android.graphics.Typeface;
import android.support.v4.content.ContextCompat;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.PopupMenu;

public class ToolbarLayout extends LinearLayout {

    public interface TabListener {
        void switchToTab(int tabId);
    }

    private LocationView mLocationView;
    private Button mTabsCountButton;
    private TabListener mTabListener;
    private TabSessionManager mSessionManager;

    public ToolbarLayout(Context context, TabSessionManager sessionManager) {
        super(context);
        mSessionManager = sessionManager;
        initView();
    }

    private void initView() {
        setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT, 1.0f));
        setOrientation(LinearLayout.HORIZONTAL);
        mLocationView = new LocationView(getContext());
        mLocationView.setLayoutParams(new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.MATCH_PARENT, 1.0f));
        mLocationView.setId(R.id.url_bar);
        addView(mLocationView);

        mTabsCountButton = getTabsCountButton();
        addView(mTabsCountButton);
    }

    private Button getTabsCountButton() {
        Button button = new Button(getContext());
        button.setLayoutParams(new LayoutParams(150, LayoutParams.MATCH_PARENT));
        button.setId(R.id.tabs_button);
        button.setOnClickListener(this::onTabButtonClicked);
        button.setBackground(ContextCompat.getDrawable(getContext(), R.drawable.tab_number_background));
        button.setTypeface(button.getTypeface(), Typeface.BOLD);
        return button;
    }

    public LocationView getLocationView() {
        return mLocationView;
    }

    public void setTabListener(TabListener listener) {
        this.mTabListener = listener;
    }

    public void updateTabCount() {
        mTabsCountButton.setText(String.valueOf(mSessionManager.sessionCount()));
    }

    public void onTabButtonClicked(View view) {
        PopupMenu tabButtonMenu = new PopupMenu(getContext(), mTabsCountButton);
        for(int idx = 0; idx < mSessionManager.sessionCount(); ++idx) {
            tabButtonMenu.getMenu().add(0, idx, idx,
                    mSessionManager.getSession(idx).getTitle());
        }
        tabButtonMenu.setOnMenuItemClickListener(item -> {
            mTabListener.switchToTab(item.getItemId());
            return true;
        });
        tabButtonMenu.show();
    }

}
