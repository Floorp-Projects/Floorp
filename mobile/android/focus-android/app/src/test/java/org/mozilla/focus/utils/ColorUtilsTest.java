/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils;

import android.graphics.Color;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class)
public class ColorUtilsTest {
    @Test
    public void testGetReadableTextColor() {
        assertEquals(Color.BLACK, ColorUtils.getReadableTextColor(Color.WHITE));
        assertEquals(Color.WHITE, ColorUtils.getReadableTextColor(Color.BLACK));

        // Slack
        assertEquals(Color.BLACK, ColorUtils.getReadableTextColor(0xfff6f4ec));

        // Google+
        assertEquals(Color.WHITE, ColorUtils.getReadableTextColor(0xffdb4437));

        // Telegram
        assertEquals(Color.WHITE, ColorUtils.getReadableTextColor(0xff527da3));

        // IRCCloud
        assertEquals(Color.BLACK, ColorUtils.getReadableTextColor(0xfff2f7fc));

        // Yahnac
        assertEquals(Color.WHITE, ColorUtils.getReadableTextColor(0xfff57c00));
    }
}