/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Environment;
import android.os.Parcelable;
import android.provider.MediaStore;
import android.text.TextUtils;
import android.util.Log;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

public class FilePicker implements BundleEventListener {
    private static final String LOGTAG = "GeckoFilePicker";
    private static FilePicker sFilePicker;
    private final Context context;

    public interface ResultHandler {
        public void gotFile(String filename);
    }

    public static void init(Context context) {
        if (sFilePicker == null) {
            sFilePicker = new FilePicker(context.getApplicationContext());
        }
    }

    protected FilePicker(Context context) {
        this.context = context;
        EventDispatcher.getInstance().registerUiThreadListener(this, "FilePicker:Show");
    }

    @Override // BundleEventListener
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if ("FilePicker:Show".equals(event)) {
            String mimeType = "*/*";
            final String mode = message.getString("mode");
            final int tabId = message.getInt("tabId", -1);
            final String title = message.getString("title");

            if ("mimeType".equals(mode)) {
                mimeType = message.getString("mimeType");
            } else if ("extension".equals(mode)) {
                mimeType = GeckoAppShell.getMimeTypeFromExtensions(message.getString("extensions"));
            }

            showFilePickerAsync(title, mimeType, new ResultHandler() {
                @Override
                public void gotFile(final String filename) {
                    callback.sendSuccess(filename);
                }
            }, tabId);
        }
    }

    private void addActivities(Intent intent, HashMap<String, Intent> intents, HashMap<String, Intent> filters) {
        PackageManager pm = context.getPackageManager();
        List<ResolveInfo> lri = pm.queryIntentActivities(intent, 0);
        for (ResolveInfo ri : lri) {
            ComponentName cn = new ComponentName(ri.activityInfo.applicationInfo.packageName, ri.activityInfo.name);
            if (filters != null && !filters.containsKey(cn.toString())) {
                Intent rintent = new Intent(intent);
                rintent.setComponent(cn);
                intents.put(cn.toString(), rintent);
            }
        }
    }

    private Intent getIntent(String mimeType) {
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.setType(mimeType);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        return intent;
    }

    private List<Intent> getIntentsForFilePicker(final String mimeType,
                                                 final FilePickerResultHandler fileHandler) {
        // The base intent to use for the file picker. Even if this is an implicit intent, Android will
        // still show a list of Activities that match this action/type.
        Intent baseIntent;
        // A HashMap of Activities the base intent will show in the chooser. This is used
        // to filter activities from other intents so that we don't show duplicates.
        HashMap<String, Intent> baseIntents = new HashMap<String, Intent>();
        // A list of other activities to shwo in the picker (and the intents to launch them).
        HashMap<String, Intent> intents = new HashMap<String, Intent> ();

        if ("audio/*".equals(mimeType)) {
            // For audio the only intent is the mimetype
            baseIntent = getIntent(mimeType);
            addActivities(baseIntent, baseIntents, null);
        } else if ("image/*".equals(mimeType)) {
            // For images the base is a capture intent
            baseIntent = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
            baseIntent.putExtra(MediaStore.EXTRA_OUTPUT,
                            Uri.fromFile(new File(Environment.getExternalStorageDirectory(),
                                                  fileHandler.generateImageName())));
            addActivities(baseIntent, baseIntents, null);

            // We also add the mimetype intent
            addActivities(getIntent(mimeType), intents, baseIntents);
        } else if ("video/*".equals(mimeType)) {
            // For videos the base is a capture intent
            baseIntent = new Intent(MediaStore.ACTION_VIDEO_CAPTURE);
            addActivities(baseIntent, baseIntents, null);

            // We also add the mimetype intent
            addActivities(getIntent(mimeType), intents, baseIntents);
        } else {
            baseIntent = getIntent("*/*");
            addActivities(baseIntent, baseIntents, null);

            Intent intent = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
            intent.putExtra(MediaStore.EXTRA_OUTPUT,
                            Uri.fromFile(new File(Environment.getExternalStorageDirectory(),
                                                  fileHandler.generateImageName())));
            addActivities(intent, intents, baseIntents);
            intent = new Intent(MediaStore.ACTION_VIDEO_CAPTURE);
            addActivities(intent, intents, baseIntents);
        }

        // If we didn't find any activities, we fall back to the */* mimetype intent
        if (baseIntents.size() == 0 && intents.size() == 0) {
            intents.clear();

            baseIntent = getIntent("*/*");
            addActivities(baseIntent, baseIntents, null);
        }

        ArrayList<Intent> vals = new ArrayList<Intent>(intents.values());
        vals.add(0, baseIntent);
        return vals;
    }

    private String getFilePickerTitle(String mimeType) {
        if (mimeType.equals("audio/*")) {
            return context.getString(R.string.filepicker_audio_title);
        } else if (mimeType.equals("image/*")) {
            return context.getString(R.string.filepicker_image_title);
        } else if (mimeType.equals("video/*")) {
            return context.getString(R.string.filepicker_video_title);
        } else {
            return context.getString(R.string.filepicker_title);
        }
    }

    /* Gets an intent that can open a particular mimetype. Will show a prompt with a list
     * of Activities that can handle the mietype. Asynchronously calls the handler when
     * one of the intents is selected. If the caller passes in null for the handler, will still
     * prompt for the activity, but will throw away the result.
     */
    private Intent getFilePickerIntent(String title,
                                       final String mimeType,
                                       final FilePickerResultHandler fileHandler) {
        final List<Intent> intents = getIntentsForFilePicker(mimeType, fileHandler);

        if (intents.size() == 0) {
            Log.i(LOGTAG, "no activities for the file picker!");
            return null;
        }

        final Intent base = intents.remove(0);

        if (intents.size() == 0) {
            return base;
        }

        if (TextUtils.isEmpty(title)) {
            title = getFilePickerTitle(mimeType);
        }
        final Intent chooser = Intent.createChooser(base, title);
        chooser.putExtra(Intent.EXTRA_INITIAL_INTENTS,
                         intents.toArray(new Parcelable[intents.size()]));
        return chooser;
    }

    /* Allows the user to pick an activity to load files from using a list prompt. Then opens the activity and
     * sends the file returned to the passed in handler. If a null handler is passed in, will still
     * pick and launch the file picker, but will throw away the result.
     */
    protected void showFilePickerAsync(final String title, final String mimeType,
                                       final ResultHandler handler, final int tabId) {
        final FilePickerResultHandler fileHandler =
                new FilePickerResultHandler(handler, context, tabId);
        final Intent intent = getFilePickerIntent(title, mimeType, fileHandler);

        if (intent == null) {
            handler.gotFile("");
            return;
        }

        ActivityHandlerHelper.startIntent(intent, fileHandler);
    }
}
