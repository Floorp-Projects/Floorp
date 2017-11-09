/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.distribution;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

@RunWith(TestRunner.class)
public class TestReferrerDescriptor {
    @Test
    public void testReferrerDescriptor() {
        String referrerString1 = "utm_source%3Dsource%26utm_content%3Dcontent%26utm_campaign%3Dcampaign%26utm_medium%3Dmedium%26utm_term%3Dterm";
        String referrerString2 = "utm_source=source&utm_content=content&utm_campaign=campaign&utm_medium=medium&utm_term=term";
        ReferrerDescriptor referrer1 = new ReferrerDescriptor(referrerString1);
        Assert.assertNotNull(referrer1);
        Assert.assertEquals(referrer1.source, "source");
        Assert.assertEquals(referrer1.content, "content");
        Assert.assertEquals(referrer1.campaign, "campaign");
        Assert.assertEquals(referrer1.medium, "medium");
        Assert.assertEquals(referrer1.term, "term");
        ReferrerDescriptor referrer2 = new ReferrerDescriptor(referrerString2);
        Assert.assertNotNull(referrer2);
        Assert.assertEquals(referrer2.source, "source");
        Assert.assertEquals(referrer2.content, "content");
        Assert.assertEquals(referrer2.campaign, "campaign");
        Assert.assertEquals(referrer2.medium, "medium");
        Assert.assertEquals(referrer2.term, "term");
    }
}
