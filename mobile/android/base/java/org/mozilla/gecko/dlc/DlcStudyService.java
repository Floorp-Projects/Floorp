/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc;

import android.content.Context;
import android.content.Intent;
import android.support.annotation.NonNull;
import android.support.v4.app.JobIntentService;

import org.mozilla.gecko.JobIdsConstants;
import org.mozilla.gecko.dlc.catalog.DownloadContentCatalog;

public class DlcStudyService extends JobIntentService {
    private static final String LOGTAG = "GeckoDlcStudyService";

    @Override
    protected void onHandleWork(@NonNull Intent intent) {
        if (!DlcHelper.isDlcPossible(LOGTAG)) {
            return;
        }

        final DownloadContentCatalog catalog = new DownloadContentCatalog(this);

        new StudyAction().perform(this, catalog);
        catalog.persistChanges();
    }

    public static void enqueueServiceWork(@NonNull final Context context) {
        enqueueWork(context, DlcStudyService.class, JobIdsConstants.getIdForDlcStudyJob(), new Intent());
    }
}
