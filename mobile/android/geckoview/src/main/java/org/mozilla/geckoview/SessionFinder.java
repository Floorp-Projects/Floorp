/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.geckoview.GeckoSession.FinderFindFlags;
import org.mozilla.geckoview.GeckoSession.FinderDisplayFlags;
import org.mozilla.geckoview.GeckoSession.FinderResult;

import android.support.annotation.AnyThread;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.Pair;

import java.util.Arrays;
import java.util.List;

/**
 * {@code SessionFinder} instances returned by {@link GeckoSession#getFinder()} performs
 * find-in-page operations.
 */
@AnyThread
public final class SessionFinder {
    private static final String LOGTAG = "GeckoSessionFinder";

    private static final List<Pair<Integer, String>> sFlagNames = Arrays.asList(
            new Pair<>(GeckoSession.FINDER_FIND_BACKWARDS, "backwards"),
            new Pair<>(GeckoSession.FINDER_FIND_LINKS_ONLY, "linksOnly"),
            new Pair<>(GeckoSession.FINDER_FIND_MATCH_CASE, "matchCase"),
            new Pair<>(GeckoSession.FINDER_FIND_WHOLE_WORD, "wholeWord")
    );

    private static void addFlagsToBundle(@FinderFindFlags final int flags,
                                         @NonNull final GeckoBundle bundle) {
        for (final Pair<Integer, String> name : sFlagNames) {
            if ((flags & name.first) != 0) {
                bundle.putBoolean(name.second, true);
            }
        }
    }

    /* package */ static int getFlagsFromBundle(@Nullable final GeckoBundle bundle) {
        if (bundle == null) {
            return 0;
        }

        int flags = 0;
        for (final Pair<Integer, String> name : sFlagNames) {
            if (bundle.getBoolean(name.second)) {
                flags |= name.first;
            }
        }
        return flags;
    }

    private final EventDispatcher mDispatcher;
    @FinderDisplayFlags private int mDisplayFlags;

    /* package */ SessionFinder(@NonNull final EventDispatcher dispatcher) {
        mDispatcher = dispatcher;
        setDisplayFlags(0);
    }

    /**
     * Find and select a string on the current page, starting from the current selection or the
     * start of the page if there is no selection. Optionally return results related to the
     * search in a {@link FinderResult} object. If {@code searchString} is null, search
     * is performed using the previous search string.
     *
     * @param searchString String to search, or null to find again using the previous string.
     * @param flags Flags for performing the search; either 0 or a combination of {@link
     *              GeckoSession#FINDER_FIND_BACKWARDS FINDER_FIND_*} constants.
     * @return Result of the search operation as a {@link GeckoResult} object.
     * @see #clear
     * @see #setDisplayFlags
     */
    @NonNull
    public GeckoResult<FinderResult> find(@Nullable final String searchString,
                                          @FinderFindFlags final int flags) {
        final GeckoBundle bundle = new GeckoBundle(sFlagNames.size() + 1);
        bundle.putString("searchString", searchString);
        addFlagsToBundle(flags, bundle);

        final CallbackResult<FinderResult> result = new CallbackResult<FinderResult>() {
            @Override
            public void sendSuccess(final Object response) {
                complete(new FinderResult((GeckoBundle) response));
            }
        };
        mDispatcher.dispatch("GeckoView:FindInPage", bundle, result);
        return result;
    }

    /**
     * Clear any highlighted find-in-page matches.
     *
     * @see #find
     * @see #setDisplayFlags
     */
    public void clear() {
        mDispatcher.dispatch("GeckoView:ClearMatches", null);
    }

    /**
     * Return flags for displaying find-in-page matches.
     *
     * @return Display flags as a combination of {@link GeckoSession#FINDER_DISPLAY_HIGHLIGHT_ALL
     *         FINDER_DISPLAY_*} constants.
     * @see #setDisplayFlags
     * @see #find
     */
    @FinderDisplayFlags public int getDisplayFlags() {
        return mDisplayFlags;
    }

    /**
     * Set flags for displaying find-in-page matches.
     *
     * @param flags Display flags as a combination of {@link
     *              GeckoSession#FINDER_DISPLAY_HIGHLIGHT_ALL FINDER_DISPLAY_*} constants.
     * @see #getDisplayFlags
     * @see #find
     */
    public void setDisplayFlags(@FinderDisplayFlags final int flags) {
        mDisplayFlags = flags;

        final GeckoBundle bundle = new GeckoBundle(3);
        bundle.putBoolean("highlightAll",
                          (flags & GeckoSession.FINDER_DISPLAY_HIGHLIGHT_ALL) != 0);
        bundle.putBoolean("dimPage",
                          (flags & GeckoSession.FINDER_DISPLAY_DIM_PAGE) != 0);
        bundle.putBoolean("drawOutline",
                          (flags & GeckoSession.FINDER_DISPLAY_DRAW_LINK_OUTLINE) != 0);
        mDispatcher.dispatch("GeckoView:DisplayMatches", bundle);
    }
}
