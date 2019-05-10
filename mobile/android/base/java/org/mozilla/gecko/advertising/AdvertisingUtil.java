package org.mozilla.gecko.advertising;

import android.content.Context;

import com.google.android.gms.ads.identifier.AdvertisingIdClient;
import com.leanplum.internal.Log;

import org.mindrot.jbcrypt.BCrypt;
import org.mozilla.gecko.annotation.ReflectionTarget;

@ReflectionTarget
public class AdvertisingUtil {
    /* Use the same SALT for all BCrypt hashings. We want the SALT to be stable for all Fennec users but it should differ from the one from Fenix.
     * Generated using Bcrypt.gensalt(). */
    private static final String BCRYPT_SALT = "$2a$10$ZfglUfcbmTyaBbAQ7SL9OO";

    /**
     * Retrieves the advertising ID hashed with BCrypt. Requires Google Play Services. Note: This method must not run on
     * the main thread.
     */
    @ReflectionTarget
    public static String getAdvertisingId(Context caller) {
        try {
            AdvertisingIdClient.Info info = AdvertisingIdClient.getAdvertisingIdInfo(caller);
            String advertisingId = info.getId();
            return advertisingId != null ? BCrypt.hashpw(advertisingId, BCRYPT_SALT) : null;
        } catch (Throwable t) {
            Log.e("Error retrieving advertising ID.", t);
        }
        return null;
    }
}
