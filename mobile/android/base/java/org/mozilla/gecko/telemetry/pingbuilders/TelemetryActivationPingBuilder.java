package org.mozilla.gecko.telemetry.pingbuilders;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.support.annotation.NonNull;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.distribution.DistributionStoreCallback;
import org.mozilla.gecko.telemetry.TelemetryOutgoingPing;
import org.mozilla.gecko.util.DateUtil;
import org.mozilla.gecko.util.StringUtils;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Locale;

/**
 * Builds a {@link TelemetryOutgoingPing} representing a activation ping.
 *
 */
public class TelemetryActivationPingBuilder extends TelemetryPingBuilder {
    private static final String LOGTAG = StringUtils.safeSubstring(TelemetryActivationPingBuilder.class.getSimpleName(), 0, 23);

    //Using MOZ_APP_BASENAME would be more elegant but according to the server side schema we need to be sure that we always send the "Fennec" value.
    private static final String APP_NAME_VALUE = "Fennec";

    private static final String PREFS_ACTIVATION_ID = "activation_ping_id";
    private static final String PREFS_ACTIVATION_SENT = "activation_ping_sent";

    private static final String NAME = "activation";
    private static final int VERSION_VALUE = 1;

    private static final String IDENTIFIER = "identifier";
    private static final String CLIENT_ID = "clientId";
    private static final String MANUFACTURER = "manufacturer";
    private static final String MODEL = "model";
    private static final String DISTRIBUTION_ID = "distribution_id";
    private static final String LOCALE = "locale";
    private static final String OS_ATTR = "os";
    private static final String OS_VERSION = "osversion";
    private static final String PING_CREATION_DATE = "created";
    private static final String TIMEZONE_OFFSET = "tz";
    private static final String APP_NAME = "app_name";
    private static final String CHANNEL = "channel";

    public TelemetryActivationPingBuilder(final Context context) {
        super(VERSION_VALUE, true);
        initPayloadConstants(context);
    }

    private void initPayloadConstants(final Context context) {
        payload.put(MANUFACTURER, Build.MANUFACTURER);
        payload.put(MODEL, Build.MODEL);
        payload.put(LOCALE, Locales.getLanguageTag(Locale.getDefault()));
        payload.put(OS_ATTR, TelemetryPingBuilder.OS_NAME);
        payload.put(OS_VERSION, Integer.toString(Build.VERSION.SDK_INT)); // A String for cross-platform reasons.

        final Calendar nowCalendar = Calendar.getInstance();
        final DateFormat pingCreationDateFormat = new SimpleDateFormat("yyyy-MM-dd", Locale.US);
        payload.put(PING_CREATION_DATE, pingCreationDateFormat.format(nowCalendar.getTime()));
        payload.put(TIMEZONE_OFFSET, DateUtil.getTimezoneOffsetInMinutesForGivenDate(nowCalendar));
        payload.put(APP_NAME, APP_NAME_VALUE);
        payload.put(CHANNEL, AppConstants.ANDROID_PACKAGE_NAME);

        SharedPreferences prefs = GeckoSharedPrefs.forApp(context);
        final String distributionId = prefs.getString(DistributionStoreCallback.PREF_DISTRIBUTION_ID, null);
        if (distributionId != null) {
            payload.put(DISTRIBUTION_ID, distributionId);
        }

        prefs.edit().putString(PREFS_ACTIVATION_ID, docID).apply();
    }

    public static boolean activationPingAlreadySent(Context context) {
        return GeckoSharedPrefs.forApp(context).getBoolean(PREFS_ACTIVATION_SENT, false);
    }

    public static void setActivationPingSent(Context context, boolean value) {
        SharedPreferences prefs = GeckoSharedPrefs.forApp(context);
        prefs.edit().putBoolean(PREFS_ACTIVATION_SENT, value).apply();
        prefs.edit().remove(PREFS_ACTIVATION_ID).apply();
    }

    public static String getActivationPingId(Context context) {
        return GeckoSharedPrefs.forApp(context).getString(PREFS_ACTIVATION_ID, null);
    }

    @Override
    public String getDocType() {
        return NAME;
    }

    @Override
    public String[] getMandatoryFields() {
        return new String[] {
                MANUFACTURER,
                MODEL,
                LOCALE,
                OS_ATTR,
                OS_VERSION,
                PING_CREATION_DATE,
                TIMEZONE_OFFSET,
                APP_NAME,
                CHANNEL
        };
    }

    public TelemetryActivationPingBuilder setIdentifier(@NonNull final String identifier) {
        if (identifier == null) {
            throw new IllegalArgumentException("Expected non-null identifier");
        }

        payload.put(IDENTIFIER, identifier);
        return this;
    }

    public TelemetryActivationPingBuilder setClientID(@NonNull final String clientID) {
        if (clientID == null) {
            throw new IllegalArgumentException("Expected non-null clientID");
        }
        payload.put(CLIENT_ID, clientID);
        return this;
    }
}
