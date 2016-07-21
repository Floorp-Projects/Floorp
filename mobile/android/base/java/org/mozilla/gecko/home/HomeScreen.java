package org.mozilla.gecko.home;

import android.os.Bundle;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.LoaderManager;
import android.view.View;

import org.mozilla.gecko.animation.PropertyAnimator;

/**
 * Generic interface for any View that can be used as the homescreen.
 *
 * In the past we had the HomePager, which contained the usual homepanels (multiple panels: TopSites,
 * bookmarks, history, etc.), which could be swiped between.
 *
 * This interface allows easily switching between different homepanel implementations. For example
 * the prototype activity-stream panel (which will be a single panel combining the functionality
 * of the previous panels).
 */
public interface HomeScreen {
    /**
     * Interface for listening into ViewPager panel changes
     */
    public interface OnPanelChangeListener {
        /**
         * Called when a new panel is selected.
         *
         * @param panelId of the newly selected panel
         */
        public void onPanelSelected(String panelId);
    }

    // The following two methods are actually methods of View. Since there is no View interface
    // we're forced to do this instead of "extending" View. Any class implementing HomeScreen
    // will have to implement these and pass them through to the underlying View.
    boolean isVisible();
    boolean requestFocus();

    void onToolbarFocusChange(boolean hasFocus);

    // The following three methods are HomePager specific. The persistence framework might need
    // refactoring/generalising at some point, but it isn't entirely clear what other panels
    // might need so we can leave these as is for now.
    void showPanel(String panelId, Bundle restoreData);
    void setOnPanelChangeListener(OnPanelChangeListener listener);
    void setPanelStateChangeListener(HomeFragment.PanelStateChangeListener listener);

    /**
     * Set a banner that may be displayed at the bottom of the HomeScreen. This can be used
     * e.g. to show snippets.
     */
    void setBanner(HomeBanner banner);

    void load(LoaderManager lm, FragmentManager fm, String panelId, Bundle restoreData, PropertyAnimator animator);

    void unload();
}
