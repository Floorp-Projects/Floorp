/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.lang.ref.WeakReference;
import java.util.EnumSet;

import org.mozilla.gecko.Clipboard;
import org.mozilla.gecko.GeckoApplication;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.IntentHelper;
import org.mozilla.gecko.R;
import org.mozilla.gecko.SnackbarBuilder;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.activitystream.ActivityStream;
import org.mozilla.gecko.bookmarks.EditBookmarkCallback;
import org.mozilla.gecko.bookmarks.BookmarkUtils;
import org.mozilla.gecko.bookmarks.UndoEditBookmarkTask;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserContract.SuggestedSites;
import org.mozilla.gecko.distribution.PartnerBookmarksProviderProxy;
import org.mozilla.gecko.home.HomeContextMenuInfo.RemoveItemType;
import org.mozilla.gecko.home.HomePager.OnUrlOpenInBackgroundListener;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.home.TopSitesGridView.TopSitesGridContextMenuInfo;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.reader.SavedReaderViewHelper;
import org.mozilla.gecko.reader.ReadingListHelper;
import org.mozilla.gecko.restrictions.Restrictable;
import org.mozilla.gecko.restrictions.Restrictions;
import org.mozilla.gecko.util.MenuUtils;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UIAsyncTask;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.os.Bundle;
import android.support.design.widget.Snackbar;
import android.support.v4.app.Fragment;
import android.text.TextUtils;
import android.util.Log;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;

/**
 * HomeFragment is an empty fragment that can be added to the HomePager.
 * Subclasses can add their own views.
 * <p>
 * The containing activity <b>must</b> implement {@link OnUrlOpenListener}.
 */
public abstract class HomeFragment extends Fragment implements EditBookmarkCallback {
    // Log Tag.
    private static final String LOGTAG = "GeckoHomeFragment";

    // Share MIME type.
    protected static final String SHARE_MIME_TYPE = "text/plain";

    // Default value for "can load" hint
    static final boolean DEFAULT_CAN_LOAD_HINT = false;

    // Whether the fragment can load its content or not
    // This is used to defer data loading until the editing
    // mode animation ends.
    private boolean mCanLoadHint;

    // Whether the fragment has loaded its content
    private boolean mIsLoaded;

    // On URL open listener
    protected OnUrlOpenListener mUrlOpenListener;

    // Helper for opening a tab in the background.
    protected OnUrlOpenInBackgroundListener mUrlOpenInBackgroundListener;

    protected PanelStateChangeListener mPanelStateChangeListener = null;

    /**
     * Listener to notify when a home panels' state has changed in a way that needs to be stored
     * for history/restoration. E.g. when a folder is opened/closed in bookmarks.
     */
    public interface PanelStateChangeListener {

        /**
         * @param bundle Data that should be persisted, and passed to this panel if restored at a later
         * stage.
         */
        void onStateChanged(Bundle bundle);

        void setCachedRecentTabsCount(int count);

        int getCachedRecentTabsCount();
    }

    public void restoreData(Bundle data) {
        // Do nothing
    }

    public void setPanelStateChangeListener(
            PanelStateChangeListener mPanelStateChangeListener) {
        this.mPanelStateChangeListener = mPanelStateChangeListener;
    }

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);

        try {
            mUrlOpenListener = (OnUrlOpenListener) activity;
        } catch (ClassCastException e) {
            throw new ClassCastException(activity.toString()
                    + " must implement HomePager.OnUrlOpenListener");
        }

        try {
            mUrlOpenInBackgroundListener = (OnUrlOpenInBackgroundListener) activity;
        } catch (ClassCastException e) {
            throw new ClassCastException(activity.toString()
                    + " must implement HomePager.OnUrlOpenInBackgroundListener");
        }
    }

    @Override
    public void onDetach() {
        super.onDetach();
        mUrlOpenListener = null;
        mUrlOpenInBackgroundListener = null;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final Bundle args = getArguments();
        if (args != null) {
            mCanLoadHint = args.getBoolean(HomePager.CAN_LOAD_ARG, DEFAULT_CAN_LOAD_HINT);
        } else {
            mCanLoadHint = DEFAULT_CAN_LOAD_HINT;
        }

        mIsLoaded = false;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        GeckoApplication.watchReference(getActivity(), this);
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View view, ContextMenuInfo menuInfo) {
        if (!(menuInfo instanceof HomeContextMenuInfo)) {
            return;
        }

        final HomeContextMenuInfo info = (HomeContextMenuInfo) menuInfo;

        // Don't show the context menu for folders if full bookmark management isn't enabled.
        final boolean enableFullBookmarkManagement = BookmarkUtils.isEnabled(getContext());
        if (info.isFolder && !enableFullBookmarkManagement) {
            return;
        }

        MenuInflater inflater = new MenuInflater(view.getContext());
        inflater.inflate(R.menu.home_contextmenu, menu);

        menu.setHeaderTitle(info.getDisplayTitle());

        // Hide unused menu items.
        menu.findItem(R.id.top_sites_edit).setVisible(false);
        menu.findItem(R.id.top_sites_pin).setVisible(false);
        menu.findItem(R.id.top_sites_unpin).setVisible(false);

        // Hide the "Edit" menuitem if this item isn't a bookmark,
        // or if this is a reading list item.
        if (!info.hasBookmarkId()) {
            menu.findItem(R.id.home_edit_bookmark).setVisible(false);
        }

        // Hide the "Remove" menuitem if this item not removable.
        if (!info.canRemove()) {
            menu.findItem(R.id.home_remove).setVisible(false);
        }

        if (!StringUtils.isShareableUrl(info.url) || GeckoProfile.get(getActivity()).inGuestMode()) {
            menu.findItem(R.id.home_share).setVisible(false);
        }

        if (!Restrictions.isAllowed(view.getContext(), Restrictable.PRIVATE_BROWSING)) {
            menu.findItem(R.id.home_open_private_tab).setVisible(false);
        }
        final boolean distSetAsHomepage = GeckoSharedPrefs.forProfile(view.getContext()).getBoolean(GeckoPreferences.PREFS_SET_AS_HOMEPAGE, false);
        menu.findItem(R.id.home_set_as_homepage).setVisible(distSetAsHomepage);

        // Hide unused menu items for bookmark folder.
        if (info.isFolder) {
            menu.findItem(R.id.home_open_new_tab).setVisible(false);
            menu.findItem(R.id.home_open_private_tab).setVisible(false);
            menu.findItem(R.id.home_copyurl).setVisible(false);
            menu.findItem(R.id.home_share).setVisible(false);
            MenuUtils.safeSetVisible(menu, R.id.home_add_to_launcher, false);
            menu.findItem(R.id.home_set_as_homepage).setVisible(false);

            menu.findItem(R.id.home_as_pin).setVisible(false);
            return;
        }

        // If Activity Stream is disabled, simply hide "AS Pin" menu item as classic Top Sites do not
        // support pinning from outside of the Top Site tiles.
        if (!ActivityStream.isEnabled(getContext())) {
            menu.findItem(R.id.home_as_pin).setVisible(false);
            return;
        }

        // Asynchronously update pin state for this item.
        // This code should be superseded by context menu unification work in Bug 1377292.
        final MenuItem asPinItem = menu.findItem(R.id.home_as_pin);
        // Do not let user interact with this menu item before we figure out pinned state.
        asPinItem.setEnabled(false);

        (new UIAsyncTask.WithoutParams<Boolean>(ThreadUtils.getBackgroundHandler()) {
            @Override
            protected Boolean doInBackground() {
                return BrowserDB.from(getContext()).isPinnedForAS(
                        getContext().getContentResolver(), info.url);
            }

            @Override
            protected void onPostExecute(Boolean hasPin) {
                if (hasPin) {
                    asPinItem.setTitle(R.string.contextmenu_top_sites_unpin);
                }

                info.updateAsPinned(hasPin);
                asPinItem.setEnabled(true);
            }
        }).execute();
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        // onContextItemSelected() is first dispatched to the activity and
        // then dispatched to its fragments. Since fragments cannot "override"
        // menu item selection handling, it's better to avoid menu id collisions
        // between the activity and its fragments.

        ContextMenuInfo menuInfo = item.getMenuInfo();
        if (!(menuInfo instanceof HomeContextMenuInfo)) {
            return false;
        }

        final HomeContextMenuInfo info = (HomeContextMenuInfo) menuInfo;
        final Context context = getActivity();

        final int itemId = item.getItemId();

        // Track the menu action. We don't know much about the context, but we can use this to determine
        // the frequency of use for various actions.
        String extras = getResources().getResourceEntryName(itemId);
        if (TextUtils.equals(extras, "home_open_private_tab")) {
            // Mask private browsing
            extras = "home_open_new_tab";
        }
        Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.CONTEXT_MENU, extras);

        if (itemId == R.id.home_copyurl) {
            if (info.url == null) {
                Log.e(LOGTAG, "Can't copy address because URL is null");
                return false;
            }

            Clipboard.setText(context, info.url);
            return true;
        }

        if (itemId == R.id.home_share) {
            if (info.url == null) {
                Log.e(LOGTAG, "Can't share because URL is null");
                return false;
            } else {
                IntentHelper.openUriExternal(info.url, SHARE_MIME_TYPE, "", "",
                                              Intent.ACTION_SEND, info.getDisplayTitle(), false);

                // Context: Sharing via chrome homepage contextmenu list (home session should be active)
                Telemetry.sendUIEvent(TelemetryContract.Event.SHARE, TelemetryContract.Method.LIST, "home_contextmenu");
                return true;
            }
        }

        if (itemId == R.id.home_add_to_launcher) {
            if (info.url == null) {
                Log.e(LOGTAG, "Can't add to home screen because URL is null");
                return false;
            }

            // Fetch an icon big enough for use as a home screen icon.
            final String displayTitle = info.getDisplayTitle();
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    GeckoApplication.createBrowserShortcut(displayTitle, info.url);
                }
            });

            Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.CONTEXT_MENU, "home_add_to_launcher");
            return true;
        }

        if (itemId == R.id.home_open_private_tab || itemId == R.id.home_open_new_tab) {
            if (info.url == null) {
                Log.e(LOGTAG, "Can't open in new tab because URL is null");
                return false;
            }

            // Some pinned site items have "user-entered" urls. URLs entered in
            // the PinSiteDialog are wrapped in a special URI until we can get a
            // valid URL. If the url is a user-entered url, decode the URL
            // before loading it.
            final String url = StringUtils.decodeUserEnteredUrl(info.url);

            final EnumSet<OnUrlOpenInBackgroundListener.Flags> flags = EnumSet.noneOf(OnUrlOpenInBackgroundListener.Flags.class);
            if (item.getItemId() == R.id.home_open_private_tab) {
                flags.add(OnUrlOpenInBackgroundListener.Flags.PRIVATE);
            }

            mUrlOpenInBackgroundListener.onUrlOpenInBackground(url, flags);

            Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.CONTEXT_MENU);

            return true;
        }

        if (itemId == R.id.home_remove) {
            // For Top Sites grid items, position is required in case item is Pinned.
            final int position = info instanceof TopSitesGridContextMenuInfo ? info.position : -1;

            if (info.hasPartnerBookmarkId()) {
                new RemovePartnerBookmarkTask(getActivity(), info.bookmarkId).execute();
            } else {
                new RemoveItemTask(getActivity(), info, position, getContext(), this).execute();
            }
            return true;
        }

        if (itemId == R.id.home_set_as_homepage) {
            final SharedPreferences prefs = GeckoSharedPrefs.forProfile(context);
            final SharedPreferences.Editor editor = prefs.edit();
            editor.putString(GeckoPreferences.PREFS_HOMEPAGE, info.url);
            editor.apply();
            Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.CONTEXT_MENU,
                getResources().getResourceEntryName(itemId));
            return true;
        }

        if (itemId == R.id.home_as_pin) {
            new ToggleASPinTask(getActivity(), info).execute();
            return true;
        }

        return false;
    }

    @Override
    public void setUserVisibleHint (boolean isVisibleToUser) {
        if (isVisibleToUser == getUserVisibleHint()) {
            return;
        }

        super.setUserVisibleHint(isVisibleToUser);
        loadIfVisible();
    }

    /**
     * Handle a configuration change by detaching and re-attaching.
     * <p>
     * A HomeFragment only needs to handle onConfiguration change (i.e.,
     * re-attach) if its UI needs to change (i.e., re-inflate layouts, use
     * different styles, etc) for different device orientations. Handling
     * configuration changes in all HomeFragments will simply cause some
     * redundant re-inflations on device rotation. This slight inefficiency
     * avoids potentially not handling a needed onConfigurationChanged in a
     * subclass.
     */
    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        // Reattach the fragment, forcing a re-inflation of its view.
        // We use commitAllowingStateLoss() instead of commit() here to avoid
        // an IllegalStateException. If the phone is rotated while Fennec
        // is in the background, onConfigurationChanged() is fired.
        // onConfigurationChanged() is called before onResume(), so
        // using commit() would throw an IllegalStateException since it can't
        // be used between the Activity's onSaveInstanceState() and
        // onResume().
        if (isVisible()) {
            getFragmentManager().beginTransaction()
                                .detach(this)
                                .attach(this)
                                .commitAllowingStateLoss();
        }
    }

    void setCanLoadHint(boolean canLoadHint) {
        if (mCanLoadHint == canLoadHint) {
            return;
        }

        mCanLoadHint = canLoadHint;
        loadIfVisible();
    }

    boolean getCanLoadHint() {
        return mCanLoadHint;
    }

    protected abstract void load();

    protected boolean canLoad() {
        return (mCanLoadHint && isVisible() && getUserVisibleHint());
    }

    protected void loadIfVisible() {
        if (!canLoad() || mIsLoaded) {
            return;
        }

        load();
        mIsLoaded = true;
    }

    @Override
    public void onUndoEditBookmark(Bundle bundle) {
        new UndoEditBookmarkTask(getActivity(), bundle).execute();
    }

    @Override
    public void onUndoRemoveBookmark(HomeContextMenuInfo info, int position) {
        new UndoRemoveBookmarkTask(getActivity(), info, position).execute();
    }

    private static class ToggleASPinTask extends UIAsyncTask.WithoutParams<Void> {
        private final WeakReference<Activity> activityWeakReference;
        private final Context context;
        private final HomeContextMenuInfo info;
        private final BrowserDB db;
        private final ContentResolver cr;
        private final boolean toggleWillPin;

        ToggleASPinTask(Activity activity, HomeContextMenuInfo info) {
            super(ThreadUtils.getBackgroundHandler());

            this.activityWeakReference = new WeakReference<Activity>(activity);
            this.context = activity.getApplicationContext();
            this.db = BrowserDB.from(context);
            this.info = info;
            this.cr = context.getContentResolver();

            if (info.isAsPinned == null) {
                throw new IllegalStateException("Tried changing pinned state before it's determined.");
            }

            this.toggleWillPin = !info.isAsPinned;
        }

        @Override
        protected Void doInBackground() {
            if (toggleWillPin) {
                db.pinSiteForAS(cr, info.url, info.title);
            } else {
                db.unpinSiteForAS(cr, info.url);
            }
            return null;
        }

        @Override
        protected void onPostExecute(Void aVoid) {
            final Activity activity = activityWeakReference.get();
            if (activity == null || activity.isFinishing()) {
                return;
            }

            int messageId = R.string.home_pinned_site;
            if (!toggleWillPin) {
                messageId = R.string.home_unpinned_site;
            }

            SnackbarBuilder.builder(activity)
                    .message(messageId)
                    .duration(Snackbar.LENGTH_LONG)
                    .buildAndShow();
        }
    }

    static class RemoveItemTask extends UIAsyncTask.WithoutParams<Void> {
        private final WeakReference<Activity> activityWeakReference;
        private final Context context;
        private final HomeContextMenuInfo info;
        private final int position;
        private final BrowserDB db;

        private EditBookmarkCallback editBookmarkCallback;

        /**
         * Remove bookmark/history/reading list type item, and also unpin the
         * Top Sites grid item at index <code>position</code>.
         */
        RemoveItemTask(Activity activity, HomeContextMenuInfo info, int position, Context context, EditBookmarkCallback editBookmarkCallback) {
            super(ThreadUtils.getBackgroundHandler());

            this.activityWeakReference = new WeakReference<>(activity);
            this.context = activity.getApplicationContext();
            this.info = info;
            this.position = position;
            this.db = BrowserDB.from(context);
            this.editBookmarkCallback = editBookmarkCallback;
        }

        @Override
        public Void doInBackground() {
            ContentResolver cr = context.getContentResolver();

            if (position > -1) {
                db.unpinSite(cr, position);
                if (db.hideSuggestedSite(info.url)) {
                    cr.notifyChange(SuggestedSites.CONTENT_URI, null);
                }
            }

            final RemoveItemType type = info.itemType;
            switch (type) {
                case BOOKMARKS:
                    removeBookmark(cr);
                    break;

                case HISTORY:
                    removeHistory(cr);
                    break;

                case COMBINED:
                    removeBookmark(cr);
                    removeHistory(cr);
                    break;

                default:
                    Log.e(LOGTAG, "Can't remove item type " + type.toString());
                    break;
            }
            return null;
        }

        @Override
        public void onPostExecute(Void result) {
            final Activity activity = activityWeakReference.get();
            if (activity == null || activity.isFinishing()) {
                return;
            }
            final int message;
            if (info.isFolder) {
                message = R.string.folder_removed;
            } else {
                message = R.string.page_removed;
            }

            if (RemoveItemType.BOOKMARKS == info.itemType) {

                SnackbarBuilder.builder(activity)
                        .message(message)
                        .action(R.string.bookmark_edit_undo)
                        .callback(new SnackbarBuilder.SnackbarCallback() {
                            @Override
                            public void onClick(View v) {
                                editBookmarkCallback.onUndoRemoveBookmark(info, position);
                            }
                        })
                        .duration(Snackbar.LENGTH_LONG)
                        .buildAndShow();
            } else {
                SnackbarBuilder.builder(activity)
                        .message(message)
                        .duration(Snackbar.LENGTH_LONG)
                        .buildAndShow();
            }
        }

        private void removeBookmark(ContentResolver cr) {
            SavedReaderViewHelper rch = SavedReaderViewHelper.getSavedReaderViewHelper(context);
            final boolean isReaderViewPage = rch.isURLCached(info.url);

            final String extra;
            if (info.isFolder) {
                extra = "bookmark_folder";
            } else if (isReaderViewPage) {
                extra = "bookmark_reader";
            } else {
                extra = "bookmark";
            }

            Telemetry.sendUIEvent(TelemetryContract.Event.UNSAVE, TelemetryContract.Method.CONTEXT_MENU, extra);
            db.updateSoftDeleteForBookmarkWithId(cr, info.bookmarkId, true);

            if (isReaderViewPage) {
                ReadingListHelper.removeCachedReaderItem(info.url, context);
            }
        }

        private void removeHistory(ContentResolver cr) {
            db.removeHistoryEntry(cr, info.url);
        }
    }

    private static class RemovePartnerBookmarkTask extends UIAsyncTask.WithoutParams<Void> {
        private final WeakReference<Activity> activityWeakReference;
        private Context context;
        private long bookmarkId;

        private RemovePartnerBookmarkTask(Activity activity, long bookmarkId) {
            super(ThreadUtils.getBackgroundHandler());

            this.activityWeakReference = new WeakReference<>(activity);
            this.context = activity.getApplicationContext();
            this.bookmarkId = bookmarkId;
        }

        @Override
        protected Void doInBackground() {
            context.getContentResolver().delete(
                    PartnerBookmarksProviderProxy.getUriForBookmark(context, bookmarkId),
                    null,
                    null
            );

            return null;
        }

        @Override
        protected void onPostExecute(Void aVoid) {
            final Activity activity = activityWeakReference.get();
            if (activity == null || activity.isFinishing()) {
                return;
            }

            SnackbarBuilder.builder(activity)
                    .message(R.string.page_removed)
                    .duration(Snackbar.LENGTH_LONG)
                    .buildAndShow();
        }
    }
}
