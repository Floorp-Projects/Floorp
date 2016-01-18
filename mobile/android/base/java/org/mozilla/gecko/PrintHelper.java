/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.util.GeckoRequest;
import org.mozilla.gecko.util.IOUtils;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;

import android.content.Context;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.print.PrintAttributes;
import android.print.PrintDocumentAdapter;
import android.print.PrintDocumentAdapter.LayoutResultCallback;
import android.print.PrintDocumentAdapter.WriteResultCallback;
import android.print.PrintDocumentInfo;
import android.print.PrintManager;
import android.print.PageRange;
import android.util.Log;

public class PrintHelper {
    private static final String LOGTAG = "GeckoPrintUtils";

    public static void printPDF(final Context context) {
        GeckoAppShell.sendRequestToGecko(new GeckoRequest("Print:PDF", new JSONObject()) {
            @Override
            public void onResponse(NativeJSObject nativeJSObject) {
                final String filePath = nativeJSObject.getString("file");
                final String title = nativeJSObject.getString("title");
                finish(context, filePath, title);
            }

            @Override
            public void onError(NativeJSObject error) {
                // Gecko didn't respond due to state change, javascript error, etc.
                Log.d(LOGTAG, "No response from Gecko on request to generate a PDF");
            }

            private void finish(final Context context, final String filePath, final String title) {
                PrintManager printManager = (PrintManager) context.getSystemService(Context.PRINT_SERVICE);
                String jobName = title;

                // The adapter methods are all called on the UI thread by the PrintManager. Put the heavyweight code
                // in onWrite on the background thread.
                PrintDocumentAdapter pda = new PrintDocumentAdapter() {
                    @Override
                    public void onWrite(final PageRange[] pages, final ParcelFileDescriptor destination, final CancellationSignal cancellationSignal, final WriteResultCallback callback) {
                        ThreadUtils.postToBackgroundThread(new Runnable() {
                            @Override
                            public void run() {
                                InputStream input = null;
                                OutputStream output = null;

                                try {
                                    File pdfFile = new File(filePath);
                                    input = new FileInputStream(pdfFile);
                                    output = new FileOutputStream(destination.getFileDescriptor());

                                    byte[] buf = new byte[8192];
                                    int bytesRead;
                                    while ((bytesRead = input.read(buf)) > 0) {
                                        output.write(buf, 0, bytesRead);
                                    }

                                    callback.onWriteFinished(new PageRange[] { PageRange.ALL_PAGES });
                                } catch (FileNotFoundException ee) {
                                    Log.d(LOGTAG, "Unable to find the temporary PDF file.");
                                } catch (IOException ioe) {
                                    Log.e(LOGTAG, "IOException while transferring temporary PDF file: ", ioe);
                                } finally {
                                    IOUtils.safeStreamClose(input);
                                    IOUtils.safeStreamClose(output);
                                }
                            }
                        });
                    }

                    @Override
                    public void onLayout(PrintAttributes oldAttributes, PrintAttributes newAttributes, CancellationSignal cancellationSignal, LayoutResultCallback callback, Bundle extras) {
                        if (cancellationSignal.isCanceled()) {
                            callback.onLayoutCancelled();
                            return;
                        }

                        PrintDocumentInfo pdi = new PrintDocumentInfo.Builder(filePath).setContentType(PrintDocumentInfo.CONTENT_TYPE_DOCUMENT).build();
                        callback.onLayoutFinished(pdi, true);
                    }

                    @Override
                    public void onFinish() {
                        // Remove the temporary file when the printing system is finished.
                        try {
                            File pdfFile = new File(filePath);
                            pdfFile.delete();
                        } catch (NullPointerException npe) {
                            // Silence the exception. We only want to delete a real file. We don't
                            // care if the file doesn't exist.
                        }
                    }
                };

                printManager.print(jobName, pda, null);
            }
        });
    }
}
