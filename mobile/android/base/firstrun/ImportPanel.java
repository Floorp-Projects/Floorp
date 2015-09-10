/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.firstrun;

import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import org.mozilla.gecko.R;
import org.mozilla.gecko.preferences.AndroidImport;
import org.mozilla.gecko.util.ThreadUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class ImportPanel extends FirstrunPanel {
    public static final String LOGTAG = "GeckoImportPanel";
    public static final int TITLE_RES = R.string.firstrun_import_title;
    private static final int AUTOADVANCE_DELAY_MS = 1500;

    // These match the item positions in R.array.pref_import_android_entries.
    private static int BOOKMARKS_INDEX = 0;
    private static int HISTORY_INDEX = 1;

    private ImageView confirmImage;
    private Button choiceButton;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstance) {
        final ViewGroup root = (ViewGroup) inflater.inflate(R.layout.firstrun_import_fragment, container, false);
        choiceButton = (Button) root.findViewById(R.id.import_action_button);
        confirmImage = (ImageView) root.findViewById(R.id.confirm_check);
        choiceButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                final AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
                final List<Integer> checked = new ArrayList<>(Arrays.asList(0, 1));
                builder.setTitle(R.string.firstrun_import_action)
                       .setMultiChoiceItems(R.array.pref_import_android_entries, makeBooleanArray(R.array.pref_import_android_defaults), new DialogInterface.OnMultiChoiceClickListener() {
                           @Override
                           public void onClick(DialogInterface dialogInterface, int index, boolean isChecked) {
                               if (isChecked && !checked.contains(index)) {
                                  checked.add(index);
                               } else if (!isChecked && checked.contains(index)) {
                                   checked.remove(checked.indexOf(index));
                               }
                           }
                       })
                       .setNegativeButton(R.string.button_cancel, new DialogInterface.OnClickListener() {
                           @Override
                           public void onClick(DialogInterface dialog, int i) {
                               dialog.dismiss();
                           }
                       })
                       .setPositiveButton(R.string.firstrun_import_dialog_button, new DialogInterface.OnClickListener() {
                           @Override
                           public void onClick(DialogInterface dialog, int i) {
                               final boolean importBookmarks = checked.contains(BOOKMARKS_INDEX);
                               final boolean importHistory = checked.contains(HISTORY_INDEX);

                               runImport(importBookmarks, importHistory);
                               dialog.dismiss();
                           }
                       });

                builder.create().show();
            }
        });

        root.findViewById(R.id.import_link).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                pagerNavigation.next();
            }
        });

        return root;
    }

    private boolean[] makeBooleanArray(int resId) {
        final String[] defaults = getResources().getStringArray(resId);
        final boolean[] booleanArray = new boolean[defaults.length];
        for (int i = 0; i < defaults.length; i++) {
            booleanArray[i] = defaults[i].equals("true");
        }
        return booleanArray;
    }

    protected void runImport(final boolean doBookmarks, final boolean doHistory) {
        Log.d(LOGTAG, "Importing Android history/bookmarks");
        if (!doBookmarks && !doHistory) {
            return;
        }

        final Context context = getActivity();
        final String dialogTitle = context.getString(R.string.firstrun_import_progress_title);

        final ProgressDialog dialog =
                ProgressDialog.show(context,
                        dialogTitle,
                        context.getString(R.string.bookmarkhistory_import_wait),
                        true);

        final Runnable stopCallback = new Runnable() {
            @Override
            public void run() {
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        confirmImage.setVisibility(View.VISIBLE);
                        choiceButton.setVisibility(View.GONE);

                        dialog.dismiss();

                        ThreadUtils.postDelayedToUiThread(new Runnable() {
                            @Override
                            public void run() {
                                next();
                            }
                        }, AUTOADVANCE_DELAY_MS);
                    }
                });
            }
        };

        ThreadUtils.postToBackgroundThread(
            // Constructing AndroidImport may need finding the profile,
            // which hits disk, so it needs to go into a Runnable too.
            new Runnable() {
                @Override
                public void run() {
                    new AndroidImport(context, stopCallback, doBookmarks, doHistory).run();
                }
            }
        );
    }
}
