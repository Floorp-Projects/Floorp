package org.mozilla.focus.web;

import android.content.Intent;
import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;
import android.support.customtabs.CustomTabsIntent;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.utils.SafeIntent;
import org.robolectric.RobolectricTestRunner;

import java.lang.reflect.Field;

import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class)
public class CustomTabConfigTest {

    /**
     * This class can't be unparceled, and can therefore be used to test that SafeIntent and SafeBundle
     * work as expected.
     */
    private static class UnparcelableParcel implements Parcelable {
        UnparcelableParcel() {}

        protected UnparcelableParcel(Parcel in) {
            // Used when unparceling:
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
        public void writeToParcel(Parcel dest, int flags) {}

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
    public void malformedExtras() throws Exception {
        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();

        final CustomTabsIntent customTabsIntent = builder.build();

        customTabsIntent.intent.putExtra("garbage", new UnparcelableParcel());

        // Warning: yuck
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

        // And we can't access any extra now because unparcelling fails:
        assertFalse(CustomTabConfig.isCustomTabIntent(new SafeIntent(customTabsIntent.intent)));

        // Ensure we don't crash regardless
        final CustomTabConfig c = CustomTabConfig.parseCustomTabIntent(new SafeIntent(customTabsIntent.intent));

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
        final CustomTabConfig c = CustomTabConfig.parseCustomTabIntent(new SafeIntent(customTabsIntent.intent));

        // But we weren't able to read the action button data because of the unparcelable data
        assertNull(c.actionButtonConfig);
    }

}