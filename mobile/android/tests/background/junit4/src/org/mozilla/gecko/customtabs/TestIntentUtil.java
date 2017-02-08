/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.customtabs;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.support.annotation.AnimRes;
import android.support.customtabs.CustomTabsIntent;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.R;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.Objects;

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
    public void testIntentWithActionButton() {
        // create properties for CustomTabsIntent
        final String description = "Description";
        final boolean tinted = true;
        final Intent actionIntent = new Intent(Intent.ACTION_VIEW);
        final int reqCode = 0x123;
        final PendingIntent pendingIntent = PendingIntent.getActivities(spyContext,
                reqCode,
                new Intent[]{actionIntent},
                PendingIntent.FLAG_CANCEL_CURRENT);

        final Bitmap bitmap = BitmapFactory.decodeResource(
                spyContext.getResources(),
                R.drawable.ic_action_settings); // arbitrary icon resource

        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setActionButton(bitmap, description, pendingIntent, tinted);

        Intent intent = builder.build().intent;
        Assert.assertTrue(IntentUtil.hasActionButton(intent));
        Assert.assertEquals(tinted, IntentUtil.isActionButtonTinted(intent));
        Assert.assertEquals(bitmap, IntentUtil.getActionButtonIcon(intent));
        Assert.assertEquals(description, IntentUtil.getActionButtonDescription(intent));
        Assert.assertTrue(
                Objects.equals(pendingIntent, IntentUtil.getActionButtonPendingIntent(intent)));
    }

    @Test
    public void testIntentWithoutActionButton() {
        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();

        Intent intent = builder.build().intent;
        Assert.assertFalse(IntentUtil.hasActionButton(intent));
        Assert.assertFalse(IntentUtil.isActionButtonTinted(intent));
        Assert.assertNull(IntentUtil.getActionButtonIcon(intent));
        Assert.assertNull(IntentUtil.getActionButtonDescription(intent));
        Assert.assertNull(IntentUtil.getActionButtonPendingIntent(intent));
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
