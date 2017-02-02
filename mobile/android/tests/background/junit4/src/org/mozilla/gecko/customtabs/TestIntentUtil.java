/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.customtabs;

import android.content.Context;
import android.content.Intent;
import android.support.annotation.AnimRes;
import android.support.customtabs.CustomTabsIntent;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.robolectric.RuntimeEnvironment;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

@RunWith(TestRunner.class)
public class TestIntentUtil {

    private static final String THIRD_PARTY_PACKAGE_NAME = "mozilla.unit.test";
    private Context spyContext;  // 3rd party app context

    @Before
    public void setUp() {
        spyContext = spy(RuntimeEnvironment.application);
        doReturn(THIRD_PARTY_PACKAGE_NAME).when(spyContext).getPackageName();
    }

    @Test
    public void testIntentWithCustomAnimation() {
        @AnimRes final int enterRes = 0x123; // arbitrary number as animation resource id
        @AnimRes final int exitRes = 0x456; // arbitrary number as animation resource id

        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setExitAnimations(spyContext, enterRes, exitRes);
        final Intent i = builder.build().intent;

        Assert.assertEquals(true, IntentUtil.hasExitAnimation(i));
        Assert.assertEquals(THIRD_PARTY_PACKAGE_NAME, IntentUtil.getAnimationPackageName(i));
        Assert.assertEquals(enterRes, IntentUtil.getEnterAnimationRes(i));
        Assert.assertEquals(exitRes, IntentUtil.getExitAnimationRes(i));
    }

    @Test
    public void testIntentWithoutCustomAnimation() {
        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        final Intent i = builder.build().intent;

        Assert.assertEquals(false, IntentUtil.hasExitAnimation(i));
        Assert.assertEquals(null, IntentUtil.getAnimationPackageName(i));
        Assert.assertEquals(IntentUtil.NO_ANIMATION_RESOURCE,
                IntentUtil.getEnterAnimationRes(i));
        Assert.assertEquals(IntentUtil.NO_ANIMATION_RESOURCE,
                IntentUtil.getExitAnimationRes(i));
    }
}
