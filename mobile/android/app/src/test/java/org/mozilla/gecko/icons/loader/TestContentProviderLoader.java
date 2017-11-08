/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.icons.loader;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.icons.IconDescriptor;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.robolectric.RuntimeEnvironment;

@RunWith(TestRunner.class)
public class TestContentProviderLoader {
    @Test
    public void testNothingIsLoadedForHttpUrls() {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl("http://www.mozilla.org")
                .icon(IconDescriptor.createGenericIcon(
                        "https://www.mozilla.org/media/img/favicon/apple-touch-icon-180x180.00050c5b754e.png"))
                .build();

        IconLoader loader = new ContentProviderLoader();
        IconResponse response = loader.load(request);

        Assert.assertNull(response);
    }
}
