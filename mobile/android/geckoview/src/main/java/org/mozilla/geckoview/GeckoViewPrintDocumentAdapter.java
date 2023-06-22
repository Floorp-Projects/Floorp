/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.geckoview;

import android.content.Context;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.print.PageRange;
import android.print.PrintAttributes;
import android.print.PrintDocumentAdapter;
import android.print.PrintDocumentInfo;
import android.util.Log;
import androidx.annotation.AnyThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import org.mozilla.gecko.util.ThreadUtils;

public class GeckoViewPrintDocumentAdapter extends PrintDocumentAdapter {
  private static final String LOGTAG = "GVPrintDocumentAdapter";
  private static final String PRINT_NAME_DEFAULT = "Document";
  private String mPrintName = PRINT_NAME_DEFAULT;
  private File mPdfFile;
  private GeckoResult<File> mGeneratedPdfFile;
  private Boolean mDoDeleteTmpPdf;
  private GeckoResult<Boolean> mPrintDialogFinish = null;

  /**
   * Default GeckoView PrintDocumentAdapter to be used with a PrintManager to print documents using
   * the default Android print functionality. Will make a temporary PDF file from InputStream.
   *
   * @param pdfInputStream an input stream containing a PDF
   * @param context context that should be used for making a temporary file
   */
  public GeckoViewPrintDocumentAdapter(
      @NonNull final InputStream pdfInputStream, @NonNull final Context context) {
    this.mDoDeleteTmpPdf = true;
    this.mGeneratedPdfFile = pdfInputStreamToFile(pdfInputStream, context);
  }

  /**
   * GeckoView PrintDocumentAdapter to be used with a PrintManager to print documents using the
   * default Android print functionality. Will make a temporary PDF file from InputStream.
   *
   * @param pdfInputStream an input stream containing a PDF
   * @param context context that should be used for making a temporary file
   * @param printDialogFinish result to report that the print finished
   */
  public GeckoViewPrintDocumentAdapter(
      @NonNull final InputStream pdfInputStream,
      @NonNull final Context context,
      @Nullable final GeckoResult<Boolean> printDialogFinish) {
    this.mDoDeleteTmpPdf = true;
    this.mGeneratedPdfFile = pdfInputStreamToFile(pdfInputStream, context);
    this.mPrintDialogFinish = printDialogFinish;
  }

  /**
   * Default GeckoView PrintDocumentAdapter to be used with a PrintManager to print documents using
   * the default Android print functionality. Will use existing PDF file for rendering. The filename
   * may be displayed to users.
   *
   * <p>Note: Recommend using other constructor if the PDF file still needs to be created so that
   * the UI reflects progress.
   *
   * @param pdfFile PDF file
   */
  public GeckoViewPrintDocumentAdapter(@NonNull final File pdfFile) {
    this.mPdfFile = pdfFile;
    this.mDoDeleteTmpPdf = false;
    this.mPrintName = mPdfFile.getName();
  }

  /**
   * Writes the PDF InputStream to a file for the PrintDocumentAdapter to use.
   *
   * @param pdfInputStream - InputStream containing a PDF
   * @param context context that should be used for making a temporary file
   * @return temporary PDF file
   */
  @AnyThread
  public static @Nullable File makeTempPdfFile(
      @NonNull final InputStream pdfInputStream, @NonNull final Context context) {
    File file = null;
    try {
      file = File.createTempFile("temp", ".pdf", context.getCacheDir());
    } catch (final IOException ioe) {
      Log.e(LOGTAG, "Could not make a file in the cache dir: ", ioe);
    }
    final int bufferSize = 8192;
    final byte[] buffer = new byte[bufferSize];
    try (final OutputStream out = new BufferedOutputStream(new FileOutputStream(file))) {
      int len;
      while ((len = pdfInputStream.read(buffer)) != -1) {
        out.write(buffer, 0, len);
      }
    } catch (final IOException ioe) {
      Log.e(LOGTAG, "Writing temporary PDF file failed: ", ioe);
    }
    return file;
  }

  /**
   * Utility to make a PDF file from the input stream in the background.
   *
   * @param pdfInputStream - InputStream containing a PDF
   * @param context context that should be used for making a temporary file
   * @return gecko result with the file
   */
  private @NonNull GeckoResult<File> pdfInputStreamToFile(
      final @NonNull InputStream pdfInputStream, final @NonNull Context context) {
    final GeckoResult<File> result = new GeckoResult<>();
    ThreadUtils.postToBackgroundThread(
        () -> {
          result.complete(makeTempPdfFile(pdfInputStream, context));
        });
    return result;
  }

  @Override
  public void onLayout(
      final PrintAttributes oldAttributes,
      final PrintAttributes newAttributes,
      final CancellationSignal cancellationSignal,
      final LayoutResultCallback layoutResultCallback,
      final Bundle bundle) {
    if (cancellationSignal.isCanceled()) {
      layoutResultCallback.onLayoutCancelled();
      return;
    }
    final PrintDocumentInfo pdi =
        new PrintDocumentInfo.Builder(mPrintName)
            .setContentType(PrintDocumentInfo.CONTENT_TYPE_DOCUMENT)
            .build();
    layoutResultCallback.onLayoutFinished(pdi, true);
  }

  /**
   * Handles onWrite functionality. Recommend running on a background thread as onWrite is on the
   * main thread.
   *
   * @param pdfFile - PDF file to generate print preview with.
   * @param parcelFileDescriptor - onWrite parcelFileDescriptor
   * @param writeResultCallback - onWrite writeResultCallback
   */
  private void onWritePdf(
      final @Nullable File pdfFile,
      final @NonNull ParcelFileDescriptor parcelFileDescriptor,
      final @NonNull WriteResultCallback writeResultCallback) {
    InputStream input = null;
    OutputStream output = null;
    try {
      input = new FileInputStream(pdfFile);
      output = new FileOutputStream(parcelFileDescriptor.getFileDescriptor());
      final int bufferSize = 8192;
      final byte[] buffer = new byte[bufferSize];
      int bytesRead;
      while ((bytesRead = input.read(buffer)) > 0) {
        output.write(buffer, 0, bytesRead);
      }
      writeResultCallback.onWriteFinished(new PageRange[] {PageRange.ALL_PAGES});
    } catch (final Exception ex) {
      Log.e(LOGTAG, "Could not complete onWrite for printing: ", ex);
      writeResultCallback.onWriteFailed(null);
    } finally {
      try {
        input.close();
        output.close();
      } catch (final Exception ex) {
        Log.e(LOGTAG, "Could not close i/o stream: ", ex);
      }
    }
  }

  @Override
  public void onWrite(
      final PageRange[] pageRanges,
      final ParcelFileDescriptor parcelFileDescriptor,
      final CancellationSignal cancellationSignal,
      final WriteResultCallback writeResultCallback) {

    ThreadUtils.postToBackgroundThread(
        () -> {
          if (mGeneratedPdfFile != null) {
            mGeneratedPdfFile.then(
                file -> {
                  if (mPrintName == PRINT_NAME_DEFAULT) {
                    mPrintName = file.getName();
                  }
                  onWritePdf(file, parcelFileDescriptor, writeResultCallback);
                  return null;
                });
          } else {
            onWritePdf(mPdfFile, parcelFileDescriptor, writeResultCallback);
          }
        });
  }

  @Override
  public void onFinish() {
    // Remove the temporary file when the printing system is finished.
    try {
      if (mDoDeleteTmpPdf) {
        if (mPdfFile != null) {
          mPdfFile.delete();
        }
        if (mGeneratedPdfFile != null) {
          mGeneratedPdfFile.then(
              file -> {
                file.delete();
                return null;
              });
        }
      }
    } catch (final NullPointerException npe) {
      // Silence the exception. We only want to delete a real file. We don't
      // care if the file doesn't exist.
    }
    if (this.mPrintDialogFinish != null) {
      mPrintDialogFinish.complete(true);
    }
  }
}
