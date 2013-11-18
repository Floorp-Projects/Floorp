package org.mozilla.gecko.tests;

import org.mozilla.gecko.db.BrowserDB;

import android.content.ContentResolver;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

/**
 * Test for thumbnail updates.
 * - loads 2 pages, each of which yield an HTTP 200
 * - verifies thumbnails are updated for both pages
 * - loads pages again; first page yields HTTP 200, second yields HTTP 404
 * - verifies thumbnail is updated for HTTP 200, but not HTTP 404
 * - finally, test that BrowserDB.removeThumbnails drops the thumbnails
 */
public class testThumbnails extends BaseTest {
    private int mTopSitesId;
    private int mThumbnailId;
    private int mTitleId;

    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    public void testThumbnails() {
        mTopSitesId = mDriver.findElement(getActivity(), "top_sites_grid").getId();
        mThumbnailId = mDriver.findElement(getActivity(), "thumbnail").getId();
        mTitleId = mDriver.findElement(getActivity(), "title").getId();

        final String site1Url = getAbsoluteUrl("/robocop/robocop_404.sjs?type=changeColor");
        final String site2Url = getAbsoluteUrl("/robocop/robocop_404.sjs?type=do404");
        final String site1Title = "changeColor";
        final String site2Title = "do404";

        // the session snapshot runnable is run 500ms after document stop. a
        // 3000ms delay gives us 2.5 seconds to take the screenshot, which
        // should be plenty of time, even on slow devices
        final int thumbnailDelay = 3000;

        blockForGeckoReady();

        // load sites; both will return HTTP 200 with a green background
        inputAndLoadUrl(site1Url);
        mSolo.sleep(thumbnailDelay);
        inputAndLoadUrl(site2Url);
        mSolo.sleep(thumbnailDelay);
        inputAndLoadUrl("about:home");
        waitForTest(new ThumbnailTest(site1Title, Color.GREEN), 5000);
        mAsserter.is(getTopSiteThumbnailColor(site1Title), Color.GREEN, "Top site thumbnail updated for HTTP 200");
        waitForTest(new ThumbnailTest(site2Title, Color.GREEN), 5000);
        mAsserter.is(getTopSiteThumbnailColor(site2Title), Color.GREEN, "Top site thumbnail updated for HTTP 200");

        // load sites again; both will have red background, and do404 will return HTTP 404
        inputAndLoadUrl(site1Url);
        mSolo.sleep(thumbnailDelay);
        inputAndLoadUrl(site2Url);
        mSolo.sleep(thumbnailDelay);
        inputAndLoadUrl("about:home");
        waitForTest(new ThumbnailTest(site1Title, Color.RED), 5000);
        mAsserter.is(getTopSiteThumbnailColor(site1Title), Color.RED, "Top site thumbnail updated for HTTP 200");
        waitForTest(new ThumbnailTest(site2Title, Color.GREEN), 5000);
        mAsserter.is(getTopSiteThumbnailColor(site2Title), Color.GREEN, "Top site thumbnail not updated for HTTP 404");

        // test dropping thumbnails
        final ContentResolver resolver = getActivity().getContentResolver();
        // check that the thumbnail is non-null
        byte[] thumbnailData = BrowserDB.getThumbnailForUrl(resolver, site1Url);
        mAsserter.ok(thumbnailData != null && thumbnailData.length > 0, "Checking for thumbnail data", "No thumbnail data found");
        // drop thumbnails
        BrowserDB.removeThumbnails(resolver);
        // check that the thumbnail is now null
        thumbnailData = BrowserDB.getThumbnailForUrl(resolver, site1Url);
        mAsserter.ok(thumbnailData == null || thumbnailData.length == 0, "Checking for thumbnail data", "Thumbnail data found");
    }

    private class ThumbnailTest implements BooleanTest {
        private String mTitle;
        private int mColor;

        public ThumbnailTest(String title, int color) {
            mTitle = title;
            mColor = color;
        }

        @Override
        public boolean test() {
            return getTopSiteThumbnailColor(mTitle) == mColor;
        }
    }

    private int getTopSiteThumbnailColor(String title) {
        ViewGroup topSites = (ViewGroup) getActivity().findViewById(mTopSitesId);
        if (topSites != null) {
            final int childCount = topSites.getChildCount();
            for (int i = 0; i < childCount; i++) {
                View child = topSites.getChildAt(i);
                if (child != null) {
                    TextView titleView = (TextView) child.findViewById(mTitleId);
                    if (titleView != null) {
                        if (titleView.getText().equals(title)) {
                            ImageView thumbnailView = (ImageView) child.findViewById(mThumbnailId);
                            if (thumbnailView != null) {
                                Bitmap thumbnail = ((BitmapDrawable) thumbnailView.getDrawable()).getBitmap();
                                return thumbnail.getPixel(0, 0);
                            } else {
                                mAsserter.dumpLog("getTopSiteThumbnailColor: unable to find mThumbnailId: "+mThumbnailId);
                            }
                        }
                    } else {
                        mAsserter.dumpLog("getTopSiteThumbnailColor: unable to find mTitleId: "+mTitleId);
                    }
                } else {
                    mAsserter.dumpLog("getTopSiteThumbnailColor: skipped null child at index "+i);
                }
            }
        } else {
            mAsserter.dumpLog("getTopSiteThumbnailColor: unable to find mTopSitesId: "+mTopSitesId);
        }
        return -1;
    }
}
