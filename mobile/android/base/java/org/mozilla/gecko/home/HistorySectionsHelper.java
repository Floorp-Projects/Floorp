/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.content.res.Resources;

import org.mozilla.gecko.home.CombinedHistoryAdapter.SectionHeader;
import org.mozilla.gecko.R;

import java.util.Calendar;
import java.util.Locale;


public class HistorySectionsHelper {

    // Constants for different time sections.
    private static final long MS_PER_DAY = 86400000;
    private static final long MS_PER_WEEK = MS_PER_DAY * 7;

    public static class SectionDateRange {
        public final long start;
        public final long end;
        public final String displayName;

        private SectionDateRange(long start, long end, String displayName) {
            this.start = start;
            this.end = end;
            this.displayName = displayName;
        }
    }

    /**
     * Updates the time range in milliseconds covered by each section header and sets the title.
     * @param res Resources for fetching string names
     * @param sectionsArray Array of section bookkeeping objects
     */
    public static void updateRecentSectionOffset(final Resources res, SectionDateRange[] sectionsArray) {
        final long now = System.currentTimeMillis();
        final Calendar cal  = Calendar.getInstance();

        // Update calendar to this day.
        cal.set(Calendar.HOUR_OF_DAY, 0);
        cal.set(Calendar.MINUTE, 0);
        cal.set(Calendar.SECOND, 0);
        cal.set(Calendar.MILLISECOND, 1);
        final long currentDayMS = cal.getTimeInMillis();

        // Calculate the start and end time for each section header and set its display text.
        sectionsArray[SectionHeader.TODAY.ordinal()] =
                new SectionDateRange(currentDayMS, now, res.getString(R.string.history_today_section));

        sectionsArray[SectionHeader.YESTERDAY.ordinal()] =
                new SectionDateRange(currentDayMS - MS_PER_DAY, currentDayMS, res.getString(R.string.history_yesterday_section));

        sectionsArray[SectionHeader.WEEK.ordinal()] =
                new SectionDateRange(currentDayMS - MS_PER_WEEK, now, res.getString(R.string.history_week_section));

        // Update the calendar to beginning of next month to avoid problems calculating the last day of this month.
        cal.add(Calendar.MONTH, 1);
        cal.set(Calendar.DAY_OF_MONTH, cal.getMinimum(Calendar.DAY_OF_MONTH));

        // Iterate over the remaining history sections, moving backwards in time.
        for (int i = SectionHeader.THIS_MONTH.ordinal(); i < SectionHeader.OLDER_THAN_SIX_MONTHS.ordinal(); i++) {
            final long end = cal.getTimeInMillis();

            cal.add(Calendar.MONTH, -1);
            final long start = cal.getTimeInMillis();

            final String displayName = cal.getDisplayName(Calendar.MONTH, Calendar.LONG, Locale.getDefault());

            sectionsArray[i] = new SectionDateRange(start, end, displayName);
        }

        sectionsArray[SectionHeader.OLDER_THAN_SIX_MONTHS.ordinal()] =
                new SectionDateRange(0L, cal.getTimeInMillis(), res.getString(R.string.history_older_section));
    }
}
