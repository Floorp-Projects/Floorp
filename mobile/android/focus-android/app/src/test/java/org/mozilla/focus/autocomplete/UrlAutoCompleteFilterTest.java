/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.autocomplete;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.widget.InlineAutocompleteEditText;
import org.robolectric.RobolectricTestRunner;

import java.util.Arrays;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;

@RunWith(RobolectricTestRunner.class)
public class UrlAutoCompleteFilterTest {
    @Test
    public void testAutocompletion() {
        final UrlAutoCompleteFilter filter = new UrlAutoCompleteFilter();
        filter.onDomainsLoaded(Arrays.asList(
                "mozilla.org",
                "google.com",
                "facebook.com"));

        assertAutocompletion(filter, "m", "mozilla.org");
        assertAutocompletion(filter, "www", "www.mozilla.org");
        assertAutocompletion(filter, "www.face", "www.facebook.com");
        assertAutocompletion(filter, "MOZ", "MOZilla.org");
        assertAutocompletion(filter, "www.GOO", "www.GOOgle.com");
        assertAutocompletion(filter, "WWW.GOOGLE.", "WWW.GOOGLE.com");
        assertAutocompletion(filter, "www.facebook.com", "www.facebook.com");
        assertAutocompletion(filter, "facebook.com", "facebook.com");

        assertNoAutocompletion(filter, "wwww");
        assertNoAutocompletion(filter, "yahoo");
    }

    @Test
    public void testWithoutDomains() {
        final UrlAutoCompleteFilter filter = new UrlAutoCompleteFilter();

        assertNoAutocompletion(filter, "mozilla");
    }

    @Test
    public void testWithoutView() {
        final UrlAutoCompleteFilter filter = new UrlAutoCompleteFilter();
        filter.onFilter("mozilla", null);
    }

    private void assertAutocompletion(UrlAutoCompleteFilter filter, String text, String completion) {
        final InlineAutocompleteEditText view = mock(InlineAutocompleteEditText.class);
        filter.onFilter(text, view);

        verify(view).onAutocomplete(completion);
    }

    private void assertNoAutocompletion(UrlAutoCompleteFilter filter, String text) {
        final InlineAutocompleteEditText view = mock(InlineAutocompleteEditText.class);
        filter.onFilter(text, view);

        verifyZeroInteractions(view);
    }
}
