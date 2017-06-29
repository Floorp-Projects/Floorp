/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.overlays.ui.ShareDialog;
import org.mozilla.gecko.util.ActivityResultHandler;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.FileUtils;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.webapps.WebAppActivity;
import org.mozilla.gecko.widget.ExternalIntentDuringPrivateBrowsingPromptFragment;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.provider.Browser;
import android.support.annotation.Nullable;
import android.support.v4.app.FragmentActivity;
import android.text.TextUtils;
import android.util.Log;
import android.webkit.MimeTypeMap;

import java.io.File;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URLEncoder;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

import static org.mozilla.gecko.Tabs.INTENT_EXTRA_SESSION_UUID;
import static org.mozilla.gecko.Tabs.INTENT_EXTRA_TAB_ID;

public final class IntentHelper implements BundleEventListener {

    private static final String LOGTAG = "GeckoIntentHelper";
    private static final String[] GECKO_EVENTS = {
        // Need to be on Gecko thread for synchronous callback.
        "Intent:GetHandlers",
    };
    private static final String[] UI_EVENTS = {
        "Intent:Open",
        "Intent:OpenForResult",
        "Intent:OpenNoHandler",
    };

    // via http://developer.android.com/distribute/tools/promote/linking.html
    private static String MARKET_INTENT_URI_PACKAGE_PREFIX = "market://details?id=";
    private static String EXTRA_BROWSER_FALLBACK_URL = "browser_fallback_url";

    /** A partial URI to an error page - the encoded error URI should be appended before loading. */
    private static String UNKNOWN_PROTOCOL_URI_PREFIX = "about:neterror?e=unknownProtocolFound&u=";

    private static IntentHelper instance;

    private final FragmentActivity activity;

    private IntentHelper(final FragmentActivity activity) {
        this.activity = activity;
        EventDispatcher.getInstance().registerGeckoThreadListener(this, GECKO_EVENTS);
        EventDispatcher.getInstance().registerUiThreadListener(this, UI_EVENTS);
    }

    public static IntentHelper init(final FragmentActivity activity) {
        if (instance == null) {
            instance = new IntentHelper(activity);
        } else {
            Log.w(LOGTAG, "IntentHelper.init() called twice, ignoring.");
        }

        return instance;
    }

    public static void destroy() {
        if (instance != null) {
            EventDispatcher.getInstance().unregisterGeckoThreadListener(instance, GECKO_EVENTS);
            EventDispatcher.getInstance().unregisterUiThreadListener(instance, UI_EVENTS);
            instance = null;
        }
    }

    /**
     * Given the inputs to <code>getOpenURIIntent</code>, plus an optional
     * package name and class name, create and fire an intent to open the
     * provided URI. If a class name is specified but a package name is not,
     * we will default to using the current fennec package.
     *
     * @param targetURI the string spec of the URI to open.
     * @param mimeType an optional MIME type string.
     * @param packageName an optional app package name.
     * @param className an optional intent class name.
     * @param action an Android action specifier, such as
     *               <code>Intent.ACTION_SEND</code>.
     * @param title the title to use in <code>ACTION_SEND</code> intents.
     * @param showPromptInPrivateBrowsing whether or not the user should be prompted when opening
     *                                    this uri from private browsing. This should be true
     *                                    when the user doesn't explicitly choose to open an an
     *                                    external app (e.g. just clicked a link).
     * @return true if the activity started successfully or the user was prompted to open the
     *              application; false otherwise.
     */
    public static boolean openUriExternal(String targetURI,
                                          String mimeType,
                                          String packageName,
                                          String className,
                                          String action,
                                          String title,
                                          final boolean showPromptInPrivateBrowsing) {
        final Context activityContext =
                GeckoActivityMonitor.getInstance().getCurrentActivity();
        final Context context = (activityContext != null) ?
                activityContext : GeckoAppShell.getApplicationContext();
        final Intent intent = getOpenURIIntent(context, targetURI,
                                               mimeType, action, title);

        if (intent == null) {
            return false;
        }

        if (!TextUtils.isEmpty(className)) {
            if (!TextUtils.isEmpty(packageName)) {
                intent.setClassName(packageName, className);
            } else {
                // Default to using the fennec app context.
                intent.setClassName(context, className);
            }
        }

        if (!showPromptInPrivateBrowsing || !(activityContext instanceof FragmentActivity)) {
            if (activityContext == null) {
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            }
            return ActivityHandlerHelper.startIntentAndCatch(LOGTAG, context, intent);
        } else {
            // Ideally we retrieve the Activity from the calling args, rather than
            // statically, but since this method is called from Gecko and I'm
            // unfamiliar with that code, this is a simpler solution.
            final FragmentActivity fragmentActivity = (FragmentActivity) activityContext;
            return ExternalIntentDuringPrivateBrowsingPromptFragment.showDialogOrAndroidChooser(
                    context, fragmentActivity.getSupportFragmentManager(), intent);
        }
    }

    public static boolean hasHandlersForIntent(Intent intent) {
        try {
            return !GeckoAppShell.queryIntentActivities(intent).isEmpty();
        } catch (Exception ex) {
            Log.e(LOGTAG, "Exception in hasHandlersForIntent");
            return false;
        }
    }

    public static String[] getHandlersForIntent(Intent intent) {
        final PackageManager pm = GeckoAppShell.getApplicationContext().getPackageManager();
        try {
            final List<ResolveInfo> list = GeckoAppShell.queryIntentActivities(intent);

            int numAttr = 4;
            final String[] ret = new String[list.size() * numAttr];
            for (int i = 0; i < list.size(); i++) {
                ResolveInfo resolveInfo = list.get(i);
                ret[i * numAttr] = resolveInfo.loadLabel(pm).toString();
                if (resolveInfo.isDefault)
                    ret[i * numAttr + 1] = "default";
                else
                    ret[i * numAttr + 1] = "";
                ret[i * numAttr + 2] = resolveInfo.activityInfo.applicationInfo.packageName;
                ret[i * numAttr + 3] = resolveInfo.activityInfo.name;
            }
            return ret;
        } catch (Exception ex) {
            Log.e(LOGTAG, "Exception in getHandlersForIntent");
            return new String[0];
        }
    }

    public static Intent getIntentForActionString(String aAction) {
        // Default to the view action if no other action as been specified.
        if (TextUtils.isEmpty(aAction)) {
            return new Intent(Intent.ACTION_VIEW);
        }
        return new Intent(aAction);
    }

    /**
     * Given a URI, a MIME type, and a title,
     * produce a share intent which can be used to query all activities
     * than can open the specified URI.
     *
     * @param context a <code>Context</code> instance.
     * @param targetURI the string spec of the URI to open.
     * @param mimeType an optional MIME type string.
     * @param title the title to use in <code>ACTION_SEND</code> intents.
     * @return an <code>Intent</code>, or <code>null</code> if none could be
     *         produced.
     */
    public static Intent getShareIntent(final Context context,
                                        final String targetURI,
                                        final String mimeType,
                                        final String title) {
        Intent shareIntent = getIntentForActionString(Intent.ACTION_SEND);
        shareIntent.putExtra(Intent.EXTRA_TEXT, targetURI);
        shareIntent.putExtra(Intent.EXTRA_SUBJECT, title);
        shareIntent.putExtra(ShareDialog.INTENT_EXTRA_DEVICES_ONLY, true);

        // Note that EXTRA_TITLE is intended to be used for share dialog
        // titles. Common usage (e.g., Pocket) suggests that it's sometimes
        // interpreted as an alternate to EXTRA_SUBJECT, so we include it.
        shareIntent.putExtra(Intent.EXTRA_TITLE, title);

        if (mimeType != null && mimeType.length() > 0) {
            shareIntent.setType(mimeType);
        }

        return shareIntent;
    }

    public static Intent getTabSwitchIntent(final Tab tab) {
        final Intent intent;
        switch (tab.getType()) {
            case CUSTOMTAB:
                if (tab.getCustomTabIntent() != null) {
                    intent = tab.getCustomTabIntent().getUnsafe();
                } else {
                    intent = new Intent(Intent.ACTION_VIEW);
                    intent.setData(Uri.parse(tab.getURL()));
                }
                break;
            case WEBAPP:
                intent = new Intent(GeckoApp.ACTION_WEBAPP);
                final String manifestPath = tab.getManifestPath();
                try {
                    intent.setData(getStartUriFromManifest(manifestPath));
                } catch (IOException | JSONException e) {
                    Log.e(LOGTAG, "Failed to get start URI from manifest", e);
                    intent.setData(Uri.parse(tab.getURL()));
                }
                intent.putExtra(WebAppActivity.MANIFEST_PATH, manifestPath);
                break;
            default:
                intent = new Intent(GeckoApp.ACTION_SWITCH_TAB);
                break;
        }

        intent.setClassName(AppConstants.ANDROID_PACKAGE_NAME, tab.getTargetClassNameForTab());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(BrowserContract.SKIP_TAB_QUEUE_FLAG, true);
        intent.putExtra(INTENT_EXTRA_TAB_ID, tab.getId());
        intent.putExtra(INTENT_EXTRA_SESSION_UUID, GeckoApplication.getSessionUUID());
        return intent;
    }

    // TODO: When things have settled down a bit, we should split this and everything similar
    // TODO: in the WebAppActivity into a dedicated WebAppManifest class (bug 1353868).
    private static Uri getStartUriFromManifest(String manifestPath) throws IOException, JSONException {
        File manifestFile = new File(manifestPath);
        final JSONObject manifest = FileUtils.readJSONObjectFromFile(manifestFile);
        final JSONObject manifestField = manifest.getJSONObject("manifest");

        return Uri.parse(manifestField.getString("start_url"));
    }

    /**
     * Given a URI, a MIME type, an Android intent "action", and a title,
     * produce an intent which can be used to start an activity to open
     * the specified URI.
     *
     * @param context a <code>Context</code> instance.
     * @param targetURI the string spec of the URI to open.
     * @param mimeType an optional MIME type string.
     * @param action an Android action specifier, such as
     *               <code>Intent.ACTION_SEND</code>.
     * @param title the title to use in <code>ACTION_SEND</code> intents.
     * @return an <code>Intent</code>, or <code>null</code> if none could be
     *         produced.
     */
    static Intent getOpenURIIntent(final Context context,
                                   final String targetURI,
                                   final String mimeType,
                                   final String action,
                                   final String title) {

        // The resultant chooser can return non-exported activities in 4.1 and earlier.
        // https://code.google.com/p/android/issues/detail?id=29535
        final Intent intent = getOpenURIIntentInner(context, targetURI, mimeType, action, title);

        if (intent != null) {
            // Some applications use this field to return to the same browser after processing the
            // Intent. While there is some danger (e.g. denial of service), other major browsers already
            // use it and so it's the norm.
            intent.putExtra(Browser.EXTRA_APPLICATION_ID, AppConstants.ANDROID_PACKAGE_NAME);
        }

        return intent;
    }

    private static Intent getOpenURIIntentInner(final Context context, final String targetURI,
                                                final String mimeType, final String action, final String title) {

        if (action.equalsIgnoreCase(Intent.ACTION_SEND)) {
            Intent shareIntent = getShareIntent(context, targetURI, mimeType, title);
            return Intent.createChooser(shareIntent,
                                        context.getResources().getString(R.string.share_title));
        }

        Uri uri = normalizeUriScheme(targetURI.indexOf(':') >= 0 ? Uri.parse(targetURI) : new Uri.Builder().scheme(targetURI).build());
        if (!TextUtils.isEmpty(mimeType)) {
            Intent intent = getIntentForActionString(action);
            intent.setDataAndType(uri, mimeType);
            return intent;
        }

        if (!GeckoAppShell.isUriSafeForScheme(uri)) {
            return null;
        }

        final String scheme = uri.getScheme();
        if ("intent".equals(scheme) || "android-app".equals(scheme)) {
            final Intent intent;
            try {
                intent = Intent.parseUri(targetURI, 0);
            } catch (final URISyntaxException e) {
                Log.e(LOGTAG, "Unable to parse URI - " + e);
                return null;
            }

            final Uri data = intent.getData();
            if (data != null && "file".equals(normalizeUriScheme(data).getScheme())) {
                Log.w(LOGTAG, "Blocked intent with \"file://\" data scheme.");
                return null;
            }

            // Only open applications which can accept arbitrary data from a browser.
            intent.addCategory(Intent.CATEGORY_BROWSABLE);

            // Prevent site from explicitly opening our internal activities, which can leak data.
            intent.setComponent(null);
            nullIntentSelector(intent);

            return intent;
        }

        // Compute our most likely intent, then check to see if there are any
        // custom handlers that would apply.
        // Start with the original URI. If we end up modifying it, we'll
        // overwrite it.
        final String extension = MimeTypeMap.getFileExtensionFromUrl(targetURI);
        final Intent intent = getIntentForActionString(action);
        intent.setData(uri);

        if ("file".equals(scheme)) {
            // Only set explicit mimeTypes on file://.
            final String mimeType2 = GeckoAppShell.getMimeTypeFromExtension(extension);
            intent.setType(mimeType2);
            return intent;
        }

        // Have a special handling for SMS based schemes, as the query parameters
        // are not extracted from the URI automatically.
        if (!"sms".equals(scheme) && !"smsto".equals(scheme) && !"mms".equals(scheme) && !"mmsto".equals(scheme)) {
            return intent;
        }

        final String query = uri.getEncodedQuery();
        if (TextUtils.isEmpty(query)) {
            return intent;
        }

        // It is common to see sms*/mms* uris on the web without '//', it is W3C standard not to have the slashes,
        // but android's Uri builder & Uri require the slashes and will interpret those without as malformed.
        String currentUri = uri.toString();
        String correctlyFormattedDataURIScheme = scheme + "://";
        if (!currentUri.contains(correctlyFormattedDataURIScheme)) {
            uri = Uri.parse(currentUri.replaceFirst(scheme + ":", correctlyFormattedDataURIScheme));
        }

        final String[] fields = query.split("&");
        boolean shouldUpdateIntent = false;
        String resultQuery = "";
        for (String field : fields) {
            if (field.startsWith("body=")) {
                final String body = Uri.decode(field.substring(5));
                intent.putExtra("sms_body", body);
                shouldUpdateIntent = true;
            } else if (field.startsWith("subject=")) {
                final String subject = Uri.decode(field.substring(8));
                intent.putExtra("subject", subject);
                shouldUpdateIntent = true;
            } else if (field.startsWith("cc=")) {
                final String ccNumber = Uri.decode(field.substring(3));
                String phoneNumber = uri.getAuthority();
                if (phoneNumber != null) {
                    uri = uri.buildUpon().encodedAuthority(phoneNumber + ";" + ccNumber).build();
                }
                shouldUpdateIntent = true;
            } else {
                resultQuery = resultQuery.concat(resultQuery.length() > 0 ? "&" + field : field);
            }
        }

        if (!shouldUpdateIntent) {
            // No need to rewrite the URI, then.
            return intent;
        }

        // Form a new URI without the extracted fields in the query part, and
        // push that into the new Intent.
        final String newQuery = resultQuery.length() > 0 ? "?" + resultQuery : "";
        final Uri pruned = uri.buildUpon().encodedQuery(newQuery).build();
        intent.setData(pruned);

        return intent;
    }

    // We create a separate method to better encapsulate the @TargetApi use.
    @TargetApi(15)
    private static void nullIntentSelector(final Intent intent) {
        intent.setSelector(null);
    }

    /**
     * Return a <code>Uri</code> instance which is equivalent to <code>u</code>,
     * but with a guaranteed-lowercase scheme as if the API level 16 method
     * <code>u.normalizeScheme</code> had been called.
     *
     * @param u the <code>Uri</code> to normalize.
     * @return a <code>Uri</code>, which might be <code>u</code>.
     */
    private static Uri normalizeUriScheme(final Uri u) {
        final String scheme = u.getScheme();
        final String lower  = scheme.toLowerCase(Locale.US);
        if (lower.equals(scheme)) {
            return u;
        }

        // Otherwise, return a new URI with a normalized scheme.
        return u.buildUpon().scheme(lower).build();
    }

    @Override // BundleEventHandler
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if ("Intent:OpenNoHandler".equals(event)) {
            openNoHandler(message, callback);

        } else if ("Intent:GetHandlers".equals(event)) {
            getHandlers(message, callback);

        } else if ("Intent:Open".equals(event)) {
            open(message);

        } else if ("Intent:OpenForResult".equals(event)) {
            openForResult(message, callback);
        }
    }

    private void getHandlers(final GeckoBundle message, final EventCallback callback) {
        final Intent intent = getOpenURIIntent(activity,
                                               message.getString("url", ""),
                                               message.getString("mime", ""),
                                               message.getString("action", ""),
                                               message.getString("title", ""));
        callback.sendSuccess(getHandlersForIntent(intent));
    }

    private void open(final GeckoBundle message) {
        openUriExternal(message.getString("url", ""),
                        message.getString("mime", ""),
                        message.getString("packageName", ""),
                        message.getString("className", ""),
                        message.getString("action", ""),
                        message.getString("title", ""), false);
    }

    private void openForResult(final GeckoBundle message, final EventCallback callback) {
        Intent intent = getOpenURIIntent(activity,
                                         message.getString("url", ""),
                                         message.getString("mime", ""),
                                         message.getString("action", ""),
                                         message.getString("title", ""));
        intent.setClassName(message.getString("packageName", ""),
                            message.getString("className", ""));
        intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);

        final ResultHandler handler = new ResultHandler(callback);
        try {
            ActivityHandlerHelper.startIntentForActivity(activity, intent, handler);
        } catch (SecurityException e) {
            Log.w(LOGTAG, "Forbidden to launch activity.", e);
        }
    }

    /**
     * Opens a URI without any valid handlers on device. In the best case, a package is specified
     * and we can bring the user directly to the application page in an app market. If a package is
     * not specified and there is a fallback url in the intent extras, we open that url. If neither
     * is present, we alert the user that we were unable to open the link.
     *
     * @param msg A message with the uri with no handlers as the value for the "uri" key
     * @param callback A callback that will be called with success & no params if Java loads a page, or with error and
     *                 the uri to load if Java does not load a page
     */
    private void openNoHandler(final GeckoBundle msg, final EventCallback callback) {
        final String uri = msg.getString("uri");

        if (TextUtils.isEmpty(uri)) {
            Log.w(LOGTAG, "Received empty URL - loading about:neterror");
            callback.sendError(getUnknownProtocolErrorPageUri(""));
            return;
        }

        final Intent intent;
        try {
            // TODO (bug 1173626): This will not handle android-app uris on non 5.1 devices.
            intent = Intent.parseUri(uri, 0);
        } catch (final URISyntaxException e) {
            String errorUri;
            try {
                errorUri = getUnknownProtocolErrorPageUri(URLEncoder.encode(uri, "UTF-8"));
            } catch (final UnsupportedEncodingException encodingE) {
                errorUri = getUnknownProtocolErrorPageUri("");
            }

            // Don't log the exception to prevent leaking URIs.
            Log.w(LOGTAG, "Unable to parse Intent URI - loading about:neterror");
            callback.sendError(errorUri);
            return;
        }

        // For this flow, we follow Chrome's lead:
        //   https://developer.chrome.com/multidevice/android/intents
        final String fallbackUrl = intent.getStringExtra(EXTRA_BROWSER_FALLBACK_URL);
        if (isFallbackUrlValid(fallbackUrl)) {
            // Opens the page in JS.
            callback.sendError(fallbackUrl);

        } else if (intent.getPackage() != null) {
            // Note on alternative flows: we could get the intent package from a component, however, for
            // security reasons, components are ignored when opening URIs (bug 1168998) so we should
            // ignore it here too.
            //
            // Our old flow used to prompt the user to search for their app in the market by scheme and
            // while this could help the user find a new app, there is not always a correlation in
            // scheme to application name and we could end up steering the user wrong (potentially to
            // malicious software). Better to leave that one alone.
            final String marketUri = MARKET_INTENT_URI_PACKAGE_PREFIX + intent.getPackage();
            final Intent marketIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(marketUri));
            marketIntent.addCategory(Intent.CATEGORY_BROWSABLE);
            marketIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

            // (Bug 1192436) We don't know if marketIntent matches any Activities (e.g. non-Play
            // Store devices). If it doesn't, clicking the link will cause no action to occur.
            ExternalIntentDuringPrivateBrowsingPromptFragment.showDialogOrAndroidChooser(
                    activity, activity.getSupportFragmentManager(), marketIntent);
            callback.sendSuccess(null);

        }  else {
            // We return the error page here, but it will only be shown if we think the load did
            // not come from clicking a link. Chrome does not show error pages in that case, and
            // many websites have catered to this behavior. For example, the site might set a timeout and load a play
            // store url for their app if the intent link fails to load, i.e. the app is not installed.
            // These work-arounds would often end with our users seeing about:neterror instead of the intended experience.
            // While I feel showing about:neterror is a better solution for users (when not hacked around),
            // we should match the status quo for the good of our users.
            //
            // Don't log the URI to prevent leaking it.
            Log.w(LOGTAG, "Unable to open URI, maybe showing neterror");
            callback.sendError(getUnknownProtocolErrorPageUri(intent.getData().toString()));
        }
    }

    private static boolean isFallbackUrlValid(@Nullable final String fallbackUrl) {
        if (fallbackUrl == null) {
            return false;
        }

        try {
            final String anyCaseScheme = new URI(fallbackUrl).getScheme();
            final String scheme = (anyCaseScheme == null) ? null : anyCaseScheme.toLowerCase(Locale.US);
            if ("http".equals(scheme) || "https".equals(scheme)) {
                return true;
            } else {
                Log.w(LOGTAG, "Fallback URI uses unsupported scheme: " + scheme + ". Try http or https.");
            }
        } catch (final URISyntaxException e) {
            // Do not include Exception to avoid leaking uris.
            Log.w(LOGTAG, "URISyntaxException parsing fallback URI");
        }
        return false;
    }

    /**
     * Returns an about:neterror uri with the unknownProtocolFound text as a parameter.
     * @param encodedUri The encoded uri. While the page does not open correctly without specifying
     *                   a uri parameter, it happily accepts the empty String so this argument may
     *                   be the empty String.
     */
    private String getUnknownProtocolErrorPageUri(final String encodedUri) {
        return UNKNOWN_PROTOCOL_URI_PREFIX + encodedUri;
    }

    private static class ResultHandler implements ActivityResultHandler {
        private final EventCallback callback;

        public ResultHandler(final EventCallback callback) {
            this.callback = callback;
        }

        @Override
        public void onActivityResult(int resultCode, Intent data) {
            final GeckoBundle response = new GeckoBundle(3);
            if (data != null) {
                if (data.getExtras() != null) {
                    response.putBundle("extras", GeckoBundle.fromBundle(data.getExtras()));
                }
                if (data.getData() != null) {
                    response.putString("uri", data.getData().toString());
                }
            }
            response.putInt("resultCode", resultCode);
            callback.sendSuccess(response);
        }
    }
}
