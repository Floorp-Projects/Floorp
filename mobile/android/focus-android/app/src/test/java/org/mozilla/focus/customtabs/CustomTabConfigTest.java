package org.mozilla.focus.customtabs;

import android.app.PendingIntent;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;
import android.os.StrictMode;
import android.support.customtabs.CustomTabsIntent;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.BuildConfig;
import org.mozilla.focus.R;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.lang.reflect.Field;
import java.util.UUID;

import mozilla.components.utils.SafeIntent;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

@RunWith(RobolectricTestRunner.class)
@Config(constants = BuildConfig.class, packageName = "org.mozilla.focus")
public class CustomTabConfigTest {

    @After
    public void cleanup() {
        // Reset strict mode: for every test, Robolectric will create FocusApplication again.
        // FocusApplication expects strict mode to be disabled (since it loads some preferences from disk),
        // before enabling it itself. If we run multiple tests, strict mode will stay enabled
        // and FocusApplication crashes during initialisation for the second test.
        // This applies across multiple Test classes, e.g. DisconnectTest can cause
        // TrackingProtectionWebViewCLientTest to fail, unless it clears StrictMode first.
        // (FocusApplicaiton is initialised before @Before methods are run.)
        StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder().build());
    }

    /**
     * This class can't be unparceled, and can therefore be used to test that SafeIntent and SafeBundle
     * work as expected.
     */
    private static class UnparcelableParcel implements Parcelable {
        // Called when constructing for test purposes
        UnparcelableParcel() {
        }

        // Used only when unparceling:
        protected UnparcelableParcel(Parcel in) {
            throw new RuntimeException("Haha");
        }

        public static final Creator<UnparcelableParcel> CREATOR = new Creator<UnparcelableParcel>() {
            @Override
            public UnparcelableParcel createFromParcel(Parcel in) {
                return new UnparcelableParcel(in);
            }

            @Override
            public UnparcelableParcel[] newArray(int size) {
                return new UnparcelableParcel[size];
            }
        };

        @Override
        public void writeToParcel(Parcel dest, int flags) {
        }

        @Override
        public int describeContents() {
            return 0;
        }
    }

    @Test
    public void isCustomTabIntent() throws Exception {
        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        final CustomTabsIntent customTabsIntent = builder.build();

        assertTrue(CustomTabConfig.isCustomTabIntent(new SafeIntent(customTabsIntent.intent)));
    }

    @Test
    public void menuTest() throws Exception {
        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        final PendingIntent pendingIntent = PendingIntent.getActivity(null, 0, null, 0);

        builder.addMenuItem("menuitem1", pendingIntent);
        builder.addMenuItem("menuitem2", pendingIntent);
        // We can only handle menu items with an actual PendingIntent, other ones should be ignored:
        builder.addMenuItem("menuitemIGNORED", null);

        final CustomTabsIntent customTabsIntent = builder.build();
        customTabsIntent.intent.putExtra(CustomTabConfig.EXTRA_CUSTOM_TAB_ID, UUID.randomUUID().toString());

        final CustomTabConfig config = CustomTabConfig.parseCustomTabIntent(RuntimeEnvironment.application, new SafeIntent(customTabsIntent.intent));

        assertEquals("Menu should contain 2 items", 2, config.menuItems.size());
        final String s = config.menuItems.get(0).name;
        assertEquals("Unexpected menu item",
                "menuitem1", config.menuItems.get(0).name);
        assertEquals("Unexpected menu item",
                "menuitem2", config.menuItems.get(1).name);
    }

    @Test
    public void malformedExtras() throws Exception {
        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();

        final CustomTabsIntent customTabsIntent = builder.build();

        customTabsIntent.intent.putExtra("garbage", new UnparcelableParcel());

        // We write the extras into a parcel so that we can check what happens when unparcelling fails
        final Parcel parcel = Parcel.obtain();
        final Bundle extras = customTabsIntent.intent.getExtras();
        extras.writeToParcel(parcel, 0);
        extras.clear();

        // Bundle is lazy and doesn't unparcel when calling readBundle()
        parcel.setDataPosition(0);
        final Bundle injectedBundle = parcel.readBundle();
        parcel.recycle();

        // We aren't usually allowed to overwrite an intent's extras:
        final Field extrasField = Intent.class.getDeclaredField("mExtras");
        extrasField.setAccessible(true);
        extrasField.set(customTabsIntent.intent, injectedBundle);
        extrasField.setAccessible(false);

        // And we can't access any extras now because unparcelling fails:
        assertFalse(CustomTabConfig.isCustomTabIntent(new SafeIntent(customTabsIntent.intent)));

        customTabsIntent.intent.putExtra(CustomTabConfig.EXTRA_CUSTOM_TAB_ID, UUID.randomUUID().toString());

        // Ensure we don't crash regardless
        final CustomTabConfig c = CustomTabConfig.parseCustomTabIntent(RuntimeEnvironment.application, new SafeIntent(customTabsIntent.intent));

        // And we don't have any data:
        assertNull(c.actionButtonConfig);
    }

    @Test
    public void malformedActionButtonConfig() throws Exception {
        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        final CustomTabsIntent customTabsIntent = builder.build();

        final Bundle garbage = new Bundle();
        garbage.putParcelable("foobar", new UnparcelableParcel());
        customTabsIntent.intent.putExtra(CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE, garbage);

        // We should still detect that this is a custom tab
        assertTrue(CustomTabConfig.isCustomTabIntent(new SafeIntent(customTabsIntent.intent)));

        // And we still don't crash
        final CustomTabConfig.ActionButtonConfig actionButtonConfig = CustomTabConfig.getActionButtonConfig(RuntimeEnvironment.application, new SafeIntent(customTabsIntent.intent));

        // But we weren't able to read the action button data because of the unparcelable data
        assertNull(actionButtonConfig);
    }

    @Test
    public void actionButtonConfig() throws Exception {
        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();

        final String description = "description";
        final String intentAction = "ACTION";
        {
            final Bitmap bitmap = Bitmap.createBitmap(new int[]{Color.RED}, 1, 1, Bitmap.Config.ARGB_8888);
            final PendingIntent intent = PendingIntent.getActivity(RuntimeEnvironment.application, 0, new Intent(intentAction), 0);

            builder.setActionButton(bitmap, description, intent);
        }

        final CustomTabsIntent customTabsIntent = builder.build();
        final CustomTabConfig.ActionButtonConfig actionButtonConfig = CustomTabConfig.getActionButtonConfig(RuntimeEnvironment.application, new SafeIntent(customTabsIntent.intent));

        assertEquals(description, actionButtonConfig.description);
        assertNotNull(actionButtonConfig.pendingIntent);

        final Bitmap bitmap = actionButtonConfig.icon;
        assertEquals(1, bitmap.getWidth());
        assertEquals(1, bitmap.getHeight());
        assertEquals(Color.RED, bitmap.getPixel(0, 0));
    }

    // Tests that a small icon is correctly processed
    @Test
    public void closeButton() throws Exception {
        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();

        {
            final Bitmap bitmap = Bitmap.createBitmap(new int[]{Color.RED}, 1, 1, Bitmap.Config.ARGB_8888);

            builder.setCloseButtonIcon(bitmap);
        }
        final CustomTabsIntent customTabsIntent = builder.build();

        final Bitmap bitmap = CustomTabConfig.getCloseButtonIcon(RuntimeEnvironment.application, new SafeIntent(customTabsIntent.intent));

        // An arbitrary icon
        assertEquals(1, bitmap.getWidth());
        assertEquals(1, bitmap.getHeight());
        assertEquals(Color.RED, bitmap.getPixel(0, 0));
    }

    // Tests that a non-Bitmap is ignored
    @Test
    public void malformedCloseButton() throws Exception {
        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        final CustomTabsIntent customTabsIntent = builder.build();

        // Intent is a parcelable but not a Bitmap
        customTabsIntent.intent.putExtra(CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON, new Intent());

        final Bitmap bitmap = CustomTabConfig.getCloseButtonIcon(RuntimeEnvironment.application, new SafeIntent(customTabsIntent.intent));

        assertNull(bitmap);
    }

    // Tests that a max-size bitmap is OK:
    @Test
    public void maxSizeCloseButton() throws Exception {
        final int maxSize = RuntimeEnvironment.application.getResources().getDimensionPixelSize(R.dimen.customtabs_toolbar_icon_size);
        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();

        {
            final Bitmap bitmap = Bitmap.createBitmap(new int[maxSize * maxSize], maxSize, maxSize, Bitmap.Config.ARGB_8888);

            builder.setCloseButtonIcon(bitmap);
        }
        final CustomTabsIntent customTabsIntent = builder.build();

        final Bitmap bitmap = CustomTabConfig.getCloseButtonIcon(RuntimeEnvironment.application, new SafeIntent(customTabsIntent.intent));

        // An arbitrary icon
        assertNotNull(bitmap);
        assertEquals(maxSize, bitmap.getWidth());
        assertEquals(maxSize, bitmap.getHeight());
    }

    // Tests that a close bitmap that's too large is ignored:
    @Test
    public void oversizedCloseButton() throws Exception {
        final int maxSize = RuntimeEnvironment.application.getResources().getDimensionPixelSize(R.dimen.customtabs_toolbar_icon_size);
        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();

        {
            final Bitmap bitmap = Bitmap.createBitmap(new int[(maxSize + 1) * (maxSize + 1)], maxSize + 1, maxSize + 1, Bitmap.Config.ARGB_8888);

            builder.setCloseButtonIcon(bitmap);
        }
        final CustomTabsIntent customTabsIntent = builder.build();

        final Bitmap bitmap = CustomTabConfig.getCloseButtonIcon(RuntimeEnvironment.application, new SafeIntent(customTabsIntent.intent));

        assertNull(bitmap);
    }

    // Test that parser throws an exception if no ID is in the intent
    @Test(expected = IllegalArgumentException.class)
    public void idIsRequired() {
        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        final CustomTabsIntent customTabsIntent = builder.build();

        CustomTabConfig.parseCustomTabIntent(RuntimeEnvironment.application, new SafeIntent(customTabsIntent.intent));
    }
}