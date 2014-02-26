/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.prompts;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.widget.GeckoActionProvider;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.widget.ListView;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

/**
 * Shows a prompt letting the user pick from a list of intent handlers for a set of Intents or
 * for a GeckoActionProvider.  Basic usage:
 *   IntentChooserPrompt prompt = new IntentChooserPrompt(context, new Intent[] {
 *      ... // some intents
 *    });
 *    prompt.show("Title", context, new IntentHandler() {
 *        public void onIntentSelected(Intent intent, int position) { }
 *        public void onCancelled() { }
 *    });
 **/
public class IntentChooserPrompt {
    private static final String LOGTAG = "GeckoIntentChooser";

    private final ArrayList<PromptListItem> mItems;

    public IntentChooserPrompt(Context context, Intent[] intents) {
        mItems = getItems(context, intents);
    }

    public IntentChooserPrompt(Context context, GeckoActionProvider provider) {
        mItems = getItems(context, provider);
    }

    /* If an IntentHandler is passed in, will asynchronously call the handler when the dialog is closed
     * Otherwise, will return the Intent that was chosen by the user. Must be called on the UI thread.
     */
    public void show(final String title, final Context context, final IntentHandler handler) {
        ThreadUtils.assertOnUiThread();

        if (mItems.isEmpty()) {
            Log.i(LOGTAG, "No activities for the intent chooser!");
            handler.onCancelled();
            return;
        }

        // If there's only one item in the intent list, just return it
        if (mItems.size() == 1) {
            handler.onIntentSelected(mItems.get(0).intent, 0);
            return;
        }

        final Prompt prompt = new Prompt(context, new Prompt.PromptCallback() {
            @Override
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
                    handler.onCancelled();
                } else {
                    handler.onIntentSelected(mItems.get(itemId).intent, itemId);
                }
            }
        });

        PromptListItem[] arrays = new PromptListItem[mItems.size()];
        mItems.toArray(arrays);
        prompt.show(title, "", arrays, ListView.CHOICE_MODE_NONE);

        return;
    }

    // Whether or not any activities were found. Useful for checking if you should try a different Intent set
    public boolean hasActivities(Context context) {
        return mItems.isEmpty();
    }

    // Gets a list of PromptListItems for an Intent array
    private ArrayList<PromptListItem> getItems(final Context context, Intent[] intents) {
        final ArrayList<PromptListItem> items = new ArrayList<PromptListItem>();

        // If we have intents, use them to build the initial list
        for (final Intent intent : intents) {
            items.addAll(getItemsForIntent(context, intent));
        }

        return items;
    }

    // Gets a list of PromptListItems for a GeckoActionProvider
    private ArrayList<PromptListItem> getItems(final Context context, final GeckoActionProvider provider) {
        final ArrayList<PromptListItem> items = new ArrayList<PromptListItem>();

        // Add any intents from the provider.
        final PackageManager packageManager = context.getPackageManager();
        final ArrayList<ResolveInfo> infos = provider.getSortedActivites();

        for (final ResolveInfo info : infos) {
            items.add(getItemForResolveInfo(info, packageManager, provider.getIntent()));
        }

        return items;
    }

    private PromptListItem getItemForResolveInfo(ResolveInfo info, PackageManager pm, Intent intent) {
        PromptListItem item = new PromptListItem(info.loadLabel(pm).toString());
        item.icon = info.loadIcon(pm);
        item.intent = new Intent(intent);

        // These intents should be implicit.
        item.intent.setComponent(new ComponentName(info.activityInfo.applicationInfo.packageName,
                                                   info.activityInfo.name));
        return item;
    }

    private ArrayList<PromptListItem> getItemsForIntent(Context context, Intent intent) {
        ArrayList<PromptListItem> items = new ArrayList<PromptListItem>();
        PackageManager pm = context.getPackageManager();
        List<ResolveInfo> lri = pm.queryIntentActivityOptions(GeckoAppShell.getGeckoInterface().getActivity().getComponentName(), null, intent, 0);

        // If we didn't find any activities, just return the empty list
        if (lri == null) {
            return items;
        }

        // Otherwise, convert the ResolveInfo. Note we don't currently check for duplicates here.
        for (ResolveInfo ri : lri) {
            items.add(getItemForResolveInfo(ri, pm, intent));
        }

        return items;
    }
}
