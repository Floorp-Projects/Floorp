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
public class TestDataUriLoader {
    @Test
    public void testNothingIsLoadedForHttpUrls() {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl("http://www.mozilla.org")
                .icon(IconDescriptor.createGenericIcon(
                        "https://www.mozilla.org/media/img/favicon/apple-touch-icon-180x180.00050c5b754e.png"))
                .build();

        IconLoader loader = new DataUriLoader();
        IconResponse response = loader.load(request);

        Assert.assertNull(response);
    }

    @Test
    public void testIconIsLoadedFromDataUri() {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl("http://www.mozilla.org")
                .icon(IconDescriptor.createGenericIcon(
                        "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91JpzAAAAEklEQVR4AWP4z8AAxCDiP8N/AB3wBPxcBee7AAAAAElFTkSuQmCC"))
                .build();

        IconLoader loader = new DataUriLoader();
        IconResponse response = loader.load(request);

        Assert.assertNotNull(response);
        Assert.assertNotNull(response.getBitmap());
    }
}
