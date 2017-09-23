/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.activitystream.homepanel.menu;

import android.app.Activity;
import android.content.Context;
import android.net.Uri;
import android.support.design.widget.BottomSheetBehavior;
import android.support.design.widget.BottomSheetDialog;
import android.support.design.widget.NavigationView;
import android.text.TextUtils;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.accessibility.AccessibilityEvent;
import android.widget.TextView;
import org.mozilla.gecko.R;
import org.mozilla.gecko.activitystream.ActivityStreamTelemetry;
import org.mozilla.gecko.activitystream.homepanel.stream.StreamOverridablePageIconLayout;
import org.mozilla.gecko.activitystream.homepanel.model.WebpageModel;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.URIUtils;

import java.lang.ref.WeakReference;
import java.net.URI;
import java.net.URISyntaxException;

/* package-private */ class BottomSheetContextMenu
        extends ActivityStreamContextMenu {

    private static final String LOGTAG = "GeckoASBottomSheet";

    private final BottomSheetDialog bottomSheetDialog;

    private final NavigationView navigationView;

    private final View content;
    private final View activityView;

    /** A reference, that represents the page domain, that allows a return value from an async task. */
    private String[] pageDomainTextReference = new String[] { "" };

    public BottomSheetContextMenu(final Context context,
                                  final View anchor,
                                  final ActivityStreamTelemetry.Extras.Builder telemetryExtraBuilder,
                                  final MenuMode mode,
                                  final WebpageModel item,
                                  final boolean shouldOverrideIconWithImageProvider,
                                  HomePager.OnUrlOpenListener onUrlOpenListener,
                                  HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener,
                                  final int tilesWidth, final int tilesHeight) {

        super(context,
                anchor,
                telemetryExtraBuilder,
                mode,
                item,
                onUrlOpenListener,
                onUrlOpenInBackgroundListener);

        // The View encompassing the activity area
        this.activityView = ((Activity) context).findViewById(android.R.id.content);

        bottomSheetDialog = new BottomSheetDialog(context);
        final LayoutInflater inflater = LayoutInflater.from(context);
        this.content = inflater.inflate(R.layout.activity_stream_contextmenu_bottomsheet, (ViewGroup) activityView, false);

        bottomSheetDialog.setContentView(content);

        final String pageTitle = item.getTitle();
        final String sheetPageTitle = !TextUtils.isEmpty(pageTitle) ? pageTitle : item.getUrl();
        final TextView titleView = (TextView) content.findViewById(R.id.title);
        titleView.setText(sheetPageTitle);

        final TextView pageDomainView = (TextView) content.findViewById(R.id.url);
        final URI itemURI;
        try {
            itemURI = new URI(item.getUrl());
            final UpdatePageDomainAsyncTask updateDomainAsyncTask = new UpdatePageDomainAsyncTask(context, pageDomainView,
                    itemURI, pageDomainTextReference);
            updateDomainAsyncTask.execute();
        } catch (final URISyntaxException e) {
            // Invalid URI: not much processing we can do. Like the async task, the page title view sets itself to the
            // url on error so we leave this field blank.
            pageDomainView.setText("");
        }

        overrideInitialAccessibilityAnnouncement(pageDomainView, titleView, sheetPageTitle, item.getUrl());

        // Copy layouted parameters from the Highlights / TopSites items to ensure consistency
        final StreamOverridablePageIconLayout pageIconLayout =
                (StreamOverridablePageIconLayout) content.findViewById(R.id.page_icon_layout);
        final ViewGroup.LayoutParams layoutParams = pageIconLayout.getLayoutParams();
        layoutParams.width = tilesWidth;
        layoutParams.height = tilesHeight;
        pageIconLayout.setLayoutParams(layoutParams);

        // We're matching the specific icon behavior for highlights and top sites.
        final String overrideIconURL = !shouldOverrideIconWithImageProvider ? null : item.getImageUrl();
        pageIconLayout.updateIcon(item.getUrl(), overrideIconURL);

        navigationView = (NavigationView) content.findViewById(R.id.menu);
        navigationView.setNavigationItemSelectedListener(this);

        super.postInit();
    }

    /**
     * Override the announcement made when the dialog first appears with Talkback enabled.
     *
     * By default, the dialog will find the first TextView (the page domain) and speak its contents as the initial
     * announcement. However, in order to uniquely identify a site, we need both the page domain and the page title: we
     * override the announcement with that content.
     *
     * Caveat: the page domain is retrieved asynchronously so this becomes tricky. In theory, we could block the UI
     * thread for the page domain and with the current implementation, it'd be fine:
     * - It's async only because the first time this method is called, it reads a file from disk.
     * Otherwise, it's unnecessary.
     * - This method is guaranteed to have already been called because about:home shows before this context menu can show
     *
     * But the implementation could change and it seemed incorrect to block showing this context menu for
     * *all* users solely for a11y text that can be estimated.
     */
    private void overrideInitialAccessibilityAnnouncement(final View pageDomainView, final View pageTitleView,
            final String pageTitle, final String urlStr) {
        final View.AccessibilityDelegate initialAnnouncementDelegate = new View.AccessibilityDelegate() {
            @Override
            public void onPopulateAccessibilityEvent(final View hostView, final AccessibilityEvent event) {
                // The page domain is retrieved with an async operation and the return value is stored here.
                final String shortenedHost = pageDomainTextReference[0];

                final String finalHost;
                if (!TextUtils.isEmpty(shortenedHost)) {
                    finalHost = shortenedHost;
                } else if (TextUtils.isEmpty(urlStr)) {
                    // There's no url so we can't do any better.
                    finalHost = "";
                } else {
                    // The async page domain isn't completed yet so we'll do a best approximation.
                    final Uri uri = Uri.parse(urlStr);
                    final String host = uri.getHost();
                    finalHost = !TextUtils.isEmpty(host) ? host : urlStr;
                }

                final String announcementText = finalHost + ", " + pageTitle;
                event.getText().add(announcementText);
                super.onPopulateAccessibilityEvent(hostView, event);
            }
        };

        // The dialog finds the first available TextView and announces its text as the accessibility title. The
        // pageDomainView is first but since the title is completed asynchronously, it may not have text yet, and
        // may be an invalid first TextView, we have to set the listener on both the first and second TextViews.
        pageDomainView.setAccessibilityDelegate(initialAnnouncementDelegate);
        pageTitleView.setAccessibilityDelegate(initialAnnouncementDelegate);
    }

    @Override
    public MenuItem getItemByID(int id) {
        return navigationView.getMenu().findItem(id);
    }

    @Override
    public void show() {
        // Try to use a 16:9 "keyline", i.e. we leave a 16:9 window of activity content visible
        // above the menu. We only do this in portrait mode - in landscape mode, 16:9 is likely
        // to be similar to the size of the display, so we'd only show very little, or even none of,
        // the menu.
        // Note that newer versions of the support library (possibly 25+) will do this automatically,
        // so we can remove that code then.
        if (activityView.getHeight() > activityView.getWidth()) {
            final int peekHeight = activityView.getHeight() - (activityView.getWidth() * 9 / 16);

            BottomSheetBehavior<View> bsBehaviour = BottomSheetBehavior.from((View) content.getParent());
            bsBehaviour.setPeekHeight(peekHeight);
        }

        overrideBottomSheetDialogAccessibility();
        bottomSheetDialog.show();
    }

    /**
     * Overrides the default accessibility properties of the {@link BottomSheetDialog}.
     * We do this because the dialog has undesirable behavior.
     */
    private void overrideBottomSheetDialogAccessibility() {
        boolean isSuccess = true;
        final Window window = bottomSheetDialog.getWindow();
        if (window != null) {
            // The dialog layout contains a view, R.id.touch_outside, that is placed outside the visible dialog and is used
            // to dismiss the dialog when tapped. However, it gets focused in Talkback which is unintuitive to Talkback
            // users (who are accustomed to using the back button for such use cases) so we hide it for a11y here.
            //
            // NB: this is fixed in later versions of the support library than the we're using, see most recent source:
            // https://android.googlesource.com/platform/frameworks/support/+/461d9c4b4b51cd552fc890d7feb001c6bd2097ea/design/res/layout/design_bottom_sheet_dialog.xml
            // I opted not to update to save time (we have an AS deadline) and to prevent possible regressions.
            final View tapToDismissView = window.findViewById(android.support.design.R.id.touch_outside);
            if (tapToDismissView != null) {
                tapToDismissView.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
            } else {
                isSuccess = false;
            }

            // We don't want to directly focus the dialog: just the list items.
            // It's unclear if this is also fixed in the newer source code.
            final View dialogView = window.findViewById(android.support.design.R.id.design_bottom_sheet);
            if (dialogView != null) {
                dialogView.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
            } else {
                isSuccess = false;
            }
        }

        if (!isSuccess) {
            Log.w(LOGTAG, "Unable to fully override Activity Stream bottom sheet accessibility behavior.");
        }
    }

    public void dismiss() {
        bottomSheetDialog.dismiss();
    }

    /** Updates the given TextView's text to the page domain. */
    private static class UpdatePageDomainAsyncTask extends URIUtils.GetFormattedDomainAsyncTask {
        private final WeakReference<TextView> pageDomainViewWeakReference;
        private final String[] pageDomainTextReference;

        private UpdatePageDomainAsyncTask(final Context context, final TextView pageDomainView, final URI uri,
                final String[] pageDomainTextReference) {
            super(context, uri, true, 1); // subdomain.domain.tld.
            this.pageDomainViewWeakReference = new WeakReference<>(pageDomainView);
            this.pageDomainTextReference = pageDomainTextReference;
        }

        @Override
        protected void onPostExecute(final String baseDomain) {
            super.onPostExecute(baseDomain);

            final TextView pageDomainView = pageDomainViewWeakReference.get();
            if (pageDomainView == null) {
                return;
            }

            final String updateText;
            if (!TextUtils.isEmpty(baseDomain)) {
                updateText = baseDomain;

            // In the unlikely error case, we leave the field blank rather than setting it to the url because
            // the page title view sets itself to the url on error.
            } else {
                final String normalizedHost = StringUtils.stripCommonSubdomains(uri.getHost());
                updateText = !TextUtils.isEmpty(normalizedHost) ? normalizedHost : "";
            }

            pageDomainTextReference[0] = updateText;
            pageDomainView.setText(updateText);
        }
    }
}
