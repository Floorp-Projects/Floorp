/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import androidx.annotation.AnyThread;
import androidx.annotation.NonNull;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.geckoview.GeckoSession.PdfSaveResult;

/**
 * {@code PdfFileSaver} instances returned by {@link GeckoSession#getPdfFileSaver()} performs save
 * operation.
 */
@AnyThread
public final class SessionPdfFileSaver {
  private static final String LOGTAG = "GeckoPdfFileSaver";

  private final EventDispatcher mDispatcher;

  /* package */ SessionPdfFileSaver(@NonNull final EventDispatcher dispatcher) {
    mDispatcher = dispatcher;
  }

  /**
   * Save the current PDF.
   *
   * @return Result of the save operation as a {@link GeckoResult} object.
   */
  @NonNull
  public GeckoResult<PdfSaveResult> save() {
    return mDispatcher
        .queryBundle("GeckoView:PDFSave", null)
        .map(response -> new PdfSaveResult(response));
  }
}
