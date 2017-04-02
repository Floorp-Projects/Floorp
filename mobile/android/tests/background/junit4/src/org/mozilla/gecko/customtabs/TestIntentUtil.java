/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.customtabs;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.support.annotation.AnimRes;
import android.support.annotation.Nullable;
import android.support.customtabs.CustomTabsIntent;
import android.text.TextUtils;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.R;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.mozglue.SafeIntent;
import org.robolectric.RuntimeEnvironment;

import java.util.List;
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
        final PendingIntent pendingIntent = createPendingIntent(0x123, "https://mozilla.org");

        final Bitmap bitmap = BitmapFactory.decodeResource(
                spyContext.getResources(),
                R.drawable.ic_action_settings); // arbitrary icon resource

        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setActionButton(bitmap, description, pendingIntent, tinted);

        SafeIntent intent = new SafeIntent(builder.build().intent);
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

        SafeIntent intent = new SafeIntent(builder.build().intent);
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
        final SafeIntent i = new SafeIntent(builder.build().intent);

        Assert.assertEquals(true, IntentUtil.hasExitAnimation(i));
        Assert.assertEquals(THIRD_PARTY_PACKAGE_NAME, IntentUtil.getAnimationPackageName(i));
        Assert.assertEquals(enterRes, IntentUtil.getEnterAnimationRes(i));
        Assert.assertEquals(exitRes, IntentUtil.getExitAnimationRes(i));
    }

    @Test
    public void testIntentWithoutCustomAnimation() {
        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        final SafeIntent i = new SafeIntent(builder.build().intent);

        Assert.assertEquals(false, IntentUtil.hasExitAnimation(i));
        Assert.assertEquals(null, IntentUtil.getAnimationPackageName(i));
        Assert.assertEquals(IntentUtil.NO_ANIMATION_RESOURCE,
                IntentUtil.getEnterAnimationRes(i));
        Assert.assertEquals(IntentUtil.NO_ANIMATION_RESOURCE,
                IntentUtil.getExitAnimationRes(i));
    }

    @Test
    public void testMenuItems() {
        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.addDefaultShareMenuItem(); // This should not effect menu-item method
        PendingIntent intent0 = createPendingIntent(0x100, "http://mozilla.com/0");
        PendingIntent intent1 = createPendingIntent(0x100, "http://mozilla.com/1");
        PendingIntent intent2 = createPendingIntent(0x100, "http://mozilla.com/2");
        builder.addMenuItem("Label 0", intent0);
        builder.addMenuItem("Label 1", intent1);
        builder.addMenuItem("Label 2", intent2);

        final SafeIntent intent = new SafeIntent(builder.build().intent);
        List<String> titles = IntentUtil.getMenuItemsTitle(intent);
        List<PendingIntent> intents = IntentUtil.getMenuItemsPendingIntent(intent);
        Assert.assertEquals(3, titles.size());
        Assert.assertEquals(3, intents.size());
        Assert.assertEquals("Label 0", titles.get(0));
        Assert.assertEquals("Label 1", titles.get(1));
        Assert.assertEquals("Label 2", titles.get(2));
        Assert.assertTrue(Objects.equals(intent0, intents.get(0)));
        Assert.assertTrue(Objects.equals(intent1, intents.get(1)));
        Assert.assertTrue(Objects.equals(intent2, intents.get(2)));
    }

    @Test
    public void testToolbarColor() {
        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();

        Assert.assertEquals(IntentUtil.getToolbarColor(new SafeIntent(builder.build().intent)),
                IntentUtil.DEFAULT_ACTION_BAR_COLOR);

        // Test red color
        builder.setToolbarColor(0xFF0000);
        Assert.assertEquals(IntentUtil.getToolbarColor(new SafeIntent(builder.build().intent)), 0xFFFF0000);
        builder.setToolbarColor(0xFFFF0000);
        Assert.assertEquals(IntentUtil.getToolbarColor(new SafeIntent(builder.build().intent)), 0xFFFF0000);

        // Test translucent green color, it should force alpha value to be 0xFF
        builder.setToolbarColor(0x0000FF00);
        Assert.assertEquals(IntentUtil.getToolbarColor(new SafeIntent(builder.build().intent)), 0xFF00FF00);
    }

    @Test
    public void testMenuShareItem() {
        final CustomTabsIntent.Builder builderNoShareItem = new CustomTabsIntent.Builder();
        Assert.assertFalse(IntentUtil.hasShareItem(new SafeIntent(builderNoShareItem.build().intent)));

        final CustomTabsIntent.Builder builderHasShareItem = new CustomTabsIntent.Builder();
        builderHasShareItem.addDefaultShareMenuItem();
        Assert.assertTrue(IntentUtil.hasShareItem(new SafeIntent(builderHasShareItem.build().intent)));
    }

    private PendingIntent createPendingIntent(int reqCode, @Nullable String uri) {
        final Intent actionIntent = new Intent(Intent.ACTION_VIEW);
        if (!TextUtils.isEmpty(uri)) {
            actionIntent.setData(Uri.parse(uri));
        }
        return PendingIntent.getActivity(spyContext, reqCode, actionIntent,
                PendingIntent.FLAG_CANCEL_CURRENT);
    }
}
