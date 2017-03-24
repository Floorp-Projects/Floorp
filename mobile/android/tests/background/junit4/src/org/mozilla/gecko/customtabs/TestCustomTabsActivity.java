/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.customtabs;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.support.annotation.AnimRes;
import android.support.customtabs.CustomTabsIntent;
import android.view.Menu;
import android.view.MenuItem;

import junit.framework.Assert;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.internal.util.reflection.Whitebox;
import org.mozilla.gecko.R;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.fakes.RoboMenu;

import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

@RunWith(TestRunner.class)
public class TestCustomTabsActivity {

    private static final String THIRD_PARTY_PACKAGE_NAME = "mozilla.unit.test";
    private Context spyContext;  // 3rd party app context
    private CustomTabsActivity spyActivity;

    @AnimRes
    private final int enterRes = 0x123; // arbitrary number as animation resource id
    @AnimRes
    private final int exitRes = 0x456; // arbitrary number as animation resource id

    @Before
    public void setUp() {
        spyContext = spy(RuntimeEnvironment.application);
        doReturn(THIRD_PARTY_PACKAGE_NAME).when(spyContext).getPackageName();

        spyActivity = spy(new CustomTabsActivity());
        doReturn(RuntimeEnvironment.application.getResources()).when(spyActivity).getResources();

        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setExitAnimations(spyContext, enterRes, exitRes);
        final Intent i = builder.build().intent;
    }

    /**
     * Activity should not call overridePendingTransition if custom animation does not exist.
     */
    @Test
    public void testFinishWithoutCustomAnimation() {
        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        final Intent i = builder.build().intent;

        doReturn(i).when(spyActivity).getIntent();

        spyActivity.finish();
        verify(spyActivity, times(0)).overridePendingTransition(anyInt(), anyInt());
    }

    /**
     * Activity should call overridePendingTransition if custom animation exists.
     */
    @Test
    public void testFinish() {
        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setExitAnimations(spyContext, enterRes, exitRes);
        final Intent i = builder.build().intent;

        doReturn(i).when(spyActivity).getIntent();

        spyActivity.finish();
        verify(spyActivity, times(1)).overridePendingTransition(eq(enterRes), eq(exitRes));
    }

    /**
     * To get 3rd party app's package name, if custom animation exists.
     */
    @Test
    public void testGetPackageName() {
        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setExitAnimations(spyContext, enterRes, exitRes);
        final Intent i = builder.build().intent;

        doReturn(i).when(spyActivity).getIntent();
        Whitebox.setInternalState(spyActivity, "usingCustomAnimation", true);

        Assert.assertEquals(THIRD_PARTY_PACKAGE_NAME, spyActivity.getPackageName());
    }

    @Test
    public void testInsertActionButton() {
        // create properties for CustomTabsIntent
        final String description = "Description";
        final Intent actionIntent = new Intent(Intent.ACTION_VIEW);
        final int reqCode = 0x123;
        final PendingIntent pendingIntent = PendingIntent.getActivities(spyContext,
                reqCode,
                new Intent[]{actionIntent},
                PendingIntent.FLAG_CANCEL_CURRENT);
        final Bitmap bitmap = BitmapFactory.decodeResource(
                spyContext.getResources(),
                R.drawable.ic_action_settings); // arbitrary icon resource

        // To create a CustomTabsIntent which is asking for ActionButton.
        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setActionButton(bitmap, description, pendingIntent, true);

        // CustomTabsActivity should return a MenuItem with corresponding attributes.
        Menu menu = new RoboMenu(spyContext);
        MenuItem item = spyActivity.insertActionButton(menu, builder.build().intent, 0xFF0000);
        Assert.assertNotNull(item);
        Assert.assertEquals(item.getTitle(), description);
        Assert.assertEquals(0, item.getOrder()); // should be the first one
        Assert.assertTrue(item.isVisible());
    }
}