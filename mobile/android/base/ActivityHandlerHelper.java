/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.prompts.Prompt;
import org.mozilla.gecko.prompts.PromptService;
import org.mozilla.gecko.util.ActivityResultHandler;
import org.mozilla.gecko.util.ActivityResultHandlerMap;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.GeckoEventListener;

import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Environment;
import android.provider.MediaStore;
import android.util.Log;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.TimeUnit;

public class ActivityHandlerHelper implements GeckoEventListener {
    private static final String LOGTAG = "GeckoActivityHandlerHelper";


    private final ActivityResultHandlerMap mActivityResultHandlerMap;
    public interface FileResultHandler {
        public void gotFile(String filename);
    }

    @SuppressWarnings("serial")
    public ActivityHandlerHelper() {
        mActivityResultHandlerMap = new ActivityResultHandlerMap();
        GeckoAppShell.getEventDispatcher().registerEventListener("FilePicker:Show", this);
    }

    @Override
    public void handleMessage(String event, final JSONObject message) {
        if (event.equals("FilePicker:Show")) {
            String mimeType = "*/*";
            String mode = message.optString("mode");

            if ("mimeType".equals(mode))
                mimeType = message.optString("mimeType");
            else if ("extension".equals(mode))
                mimeType = GeckoAppShell.getMimeTypeFromExtensions(message.optString("extensions"));

            Log.i(LOGTAG, "Mime: " + mimeType);

            showFilePickerAsync(GeckoAppShell.getGeckoInterface().getActivity(), mimeType, new FileResultHandler() {
                public void gotFile(String filename) {
                    try {
                        message.put("file", filename);
                    } catch (JSONException ex) {
                        Log.i(LOGTAG, "Can't add filename to message " + filename);
                    }
                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent(
                        "FilePicker:Result", message.toString()));
                }
            });
        }
    }

    public int makeRequestCode(ActivityResultHandler aHandler) {
        return mActivityResultHandlerMap.put(aHandler);
    }

    public void startIntentForActivity (Activity activity, Intent intent, ActivityResultHandler activityResultHandler) {
        activity.startActivityForResult(intent, mActivityResultHandlerMap.put(activityResultHandler));
    }

    private int addIntentActivitiesToList(Context context, Intent intent, ArrayList<Prompt.PromptListItem> items, ArrayList<Intent> aIntents) {
        PackageManager pm = context.getPackageManager();
        List<ResolveInfo> lri = pm.queryIntentActivityOptions(GeckoAppShell.getGeckoInterface().getActivity().getComponentName(), null, intent, 0);

        if (lri == null) {
            return 0;
        }

        for (ResolveInfo ri : lri) {
            Intent rintent = new Intent(intent);
            rintent.setComponent(new ComponentName(
                    ri.activityInfo.applicationInfo.packageName,
                    ri.activityInfo.name));

            Prompt.PromptListItem item = new Prompt.PromptListItem(ri.loadLabel(pm).toString());
            item.icon = ri.loadIcon(pm);
            items.add(item);
            aIntents.add(rintent);
        }

        return lri.size();
    }

    private int addFilePickingActivities(Context context, ArrayList<Prompt.PromptListItem> aItems, String aType, ArrayList<Intent> aIntents) {
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.setType(aType);
        intent.addCategory(Intent.CATEGORY_OPENABLE);

        return addIntentActivitiesToList(context, intent, aItems, aIntents);
    }

    private Prompt.PromptListItem[] getItemsAndIntentsForFilePicker(Context context, String aMimeType, final FilePickerResultHandler fileHandler, ArrayList<Intent> aIntents) {
        ArrayList<Prompt.PromptListItem> items = new ArrayList<Prompt.PromptListItem>();

        if (aMimeType.equals("audio/*")) {
            if (addFilePickingActivities(context, items, "audio/*", aIntents) <= 0) {
                addFilePickingActivities(context, items, "*/*", aIntents);
            }
        } else if (aMimeType.equals("image/*")) {
            Intent intent = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
            intent.putExtra(MediaStore.EXTRA_OUTPUT,
                            Uri.fromFile(new File(Environment.getExternalStorageDirectory(),
                                                  fileHandler.generateImageName())));
            addIntentActivitiesToList(context, intent, items, aIntents);

            if (addFilePickingActivities(context, items, "image/*", aIntents) <= 0) {
                addFilePickingActivities(context, items, "*/*", aIntents);
            }
        } else if (aMimeType.equals("video/*")) {
            Intent intent = new Intent(MediaStore.ACTION_VIDEO_CAPTURE);
            addIntentActivitiesToList(context, intent, items, aIntents);

            if (addFilePickingActivities(context, items, "video/*", aIntents) <= 0) {
                addFilePickingActivities(context, items, "*/*", aIntents);
            }
        } else {
            Intent intent = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
            intent.putExtra(MediaStore.EXTRA_OUTPUT,
                            Uri.fromFile(new File(Environment.getExternalStorageDirectory(),
                                                  fileHandler.generateImageName())));
            addIntentActivitiesToList(context, intent, items, aIntents);

            intent = new Intent(MediaStore.ACTION_VIDEO_CAPTURE);
            addIntentActivitiesToList(context, intent, items, aIntents);

            addFilePickingActivities(context, items, "*/*", aIntents);
        }

        return items.toArray(new Prompt.PromptListItem[] {});
    }

    private String getFilePickerTitle(Context context, String aMimeType) {
        if (aMimeType.equals("audio/*")) {
            return context.getString(R.string.filepicker_audio_title);
        } else if (aMimeType.equals("image/*")) {
            return context.getString(R.string.filepicker_image_title);
        } else if (aMimeType.equals("video/*")) {
            return context.getString(R.string.filepicker_video_title);
        } else {
            return context.getString(R.string.filepicker_title);
        }
    }

    private interface IntentHandler {
        public void gotIntent(Intent intent);
    }

    /* Gets an intent that can open a particular mimetype. Will show a prompt with a list
     * of Activities that can handle the mietype. Asynchronously calls the handler when
     * one of the intents is selected. If the caller passes in null for the handler, will still
     * prompt for the activity, but will throw away the result.
     */
    private void getFilePickerIntentAsync(final Context context, String aMimeType, final FilePickerResultHandler fileHandler, final IntentHandler handler) {
        final ArrayList<Intent> intents = new ArrayList<Intent>();
        final Prompt.PromptListItem[] items =
            getItemsAndIntentsForFilePicker(context, aMimeType, fileHandler, intents);

        if (intents.size() == 0) {
            Log.i(LOGTAG, "no activities for the file picker!");
            handler.gotIntent(null);
            return;
        }

        if (intents.size() == 1) {
            handler.gotIntent(intents.get(0));
            return;
        }

        final Prompt prompt = new Prompt(context, new Prompt.PromptCallback() {
            public void onPromptFinished(String promptServiceResult) {
                if (handler == null) {
                    return;
                }

                int itemId = -1;
                try {
                    itemId = new JSONObject(promptServiceResult).getInt("button");
                } catch (JSONException e) {
                    Log.e(LOGTAG, "result from promptservice was invalid: ", e);
                }

                if (itemId == -1) {
                    handler.gotIntent(null);
                } else {
                    handler.gotIntent(intents.get(itemId));
                }
            }
        });

        final String title = getFilePickerTitle(context, aMimeType);
        // Runnable has to be called to show an intent-like
        // context menu UI using the PromptService.
        ThreadUtils.postToUiThread(new Runnable() {
            @Override public void run() {
                prompt.show(title, "", items, false);
            }
        });
    }

    /* Allows the user to pick an activity to load files from using a list prompt. Then opens the activity and
     * sends the file returned to the passed in handler. If a null handler is passed in, will still
     * pick and launch the file picker, but will throw away the result.
     */
    public void showFilePickerAsync(final Activity parentActivity, String aMimeType, final FileResultHandler handler) {
        final FilePickerResultHandler fileHandler = new FilePickerResultHandler(handler);
        getFilePickerIntentAsync(parentActivity, aMimeType, fileHandler, new IntentHandler() {
            public void gotIntent(Intent intent) {
                if (handler == null) {
                    return;
                }

                if (intent == null) {
                    handler.gotFile("");
                    return;
                }

                parentActivity.startActivityForResult(intent, mActivityResultHandlerMap.put(fileHandler));
            }
        });
    }

    boolean handleActivityResult(int requestCode, int resultCode, Intent data) {
        ActivityResultHandler handler = mActivityResultHandlerMap.getAndRemove(requestCode);
        if (handler != null) {
            handler.onActivityResult(resultCode, data);
            return true;
        }
        return false;
    }
}
