/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import android.graphics.Bitmap;

import org.mozilla.gecko.icons.decoders.ICODecoder;
import org.mozilla.gecko.icons.decoders.IconDirectoryEntry;
import org.mozilla.gecko.icons.decoders.LoadFaviconResult;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertEquals;
import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertNull;
import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertNotNull;
import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertTrue;

public class testICODecoder extends UITest {

    private int mGolemNumIconDirEntries;

    public void testICODecoder() throws IOException {
        testMicrosoftFavicon();
        testNvidiaFavicon();
        testGolemFavicon();
        testMissingHeader();
        testCorruptIconDirectory();
    }

    /**
     * Decode and verify a Microsoft favicon with six different sizes:
     * 128x128, 72x72, 48x48, 32x32, 24x24, 16x16
     * Each of the six BMPs supposedly has zero colour depth.
     */
    private void testMicrosoftFavicon() throws IOException {
        byte[] icoBytes = readICO("microsoft_favicon.ico");
        fAssertEquals("Expecting Microsoft favicon to be 17174 bytes.", 17174, icoBytes.length);

        ICODecoder decoder = new ICODecoder(getInstrumentation().getTargetContext(), icoBytes, 0,
                                            icoBytes.length);
        LoadFaviconResult result = decoder.decode();
        fAssertNotNull("Expecting Microsoft favicon to not fail decoding.", result);

        int largestBitmap = Integer.MAX_VALUE;

        int[] possibleSizes = {16, 24, 32, 48, 72, 128};
        for (int i = 0; i < possibleSizes.length; i++) {
            if (possibleSizes[i] > decoder.getLargestFaviconSize()) {
                largestBitmap = possibleSizes[i];

                // Verify that all bitmaps but the smallest larger than Favicons.largestFaviconSize
                // have been discarded.
                for (int j = i + 1; j < possibleSizes.length; j++) {
                    Bitmap selectedBitmap = result.getBestBitmap(possibleSizes[j]);
                    fAssertNotNull("Expecting a best bitmap to be found for " +
                                   possibleSizes[j] + "x" + possibleSizes[j], selectedBitmap);

                    fAssertEquals("Expecting best bitmap to have width " + possibleSizes[i],
                                  possibleSizes[i], selectedBitmap.getWidth());
                    fAssertEquals("Expecting best bitmap to have height " + possibleSizes[i],
                            possibleSizes[i], selectedBitmap.getHeight());

                    // Reset the result's bitmap iterator.
                    result = decoder.decode();
                }

                break;
            }
        }

        int[] expectedSizes = {
                // If we request a 33x33 we should get a 48x48.
                33, 48,
                // If we request a 24x24 we should get a 24x24.
                24, 24,
                // If we request a 8x8 we should get a 16x16.
                8, 16,
        };

        for (int i = 0; i < expectedSizes.length - 1; i += 2) {
            if (expectedSizes[i + 1] > largestBitmap) {
                // This bitmap has been discarded.
                continue;
            }

            Bitmap selectedBitmap = result.getBestBitmap(expectedSizes[i]);
            fAssertNotNull("Expecting a best bitmap to have been found for " +
                           expectedSizes[i] + "x" + expectedSizes[i], selectedBitmap);

            fAssertEquals("Expecting best bitmap to have width " + expectedSizes[i + 1],
                          expectedSizes[i + 1], selectedBitmap.getWidth());
            fAssertEquals("Expecting best bitmap to have height " + expectedSizes[i + 1],
                          expectedSizes[i + 1], selectedBitmap.getHeight());

            // Reset the result's bitmap iterator.
            result = decoder.decode();
        }
    }

    /**
     * Decode and verify a NVIDIA favicon with three different colour depths,
     * and three different sizes for each colour depth. All payloads are BMP.
     */
    private void testNvidiaFavicon() throws IOException {
        byte[] icoBytes = readICO("nvidia_favicon.ico");
        fAssertEquals("Expecting NVIDIA favicon to be 25214 bytes.", 25214, icoBytes.length);

        ICODecoder decoder = new ICODecoder(getInstrumentation().getTargetContext(), icoBytes, 0,
                                            icoBytes.length);
        fAssertNotNull("Expecting NVIDIA favicon to not fail decoding.", decoder.decode());

        // Verify the best entry is correctly chosen for each width.
        // We expect 32 bpp in all cases even if 32 bpp exceeds IconDirectoryEntry.maxBPP.
        // This is okay because IconDirectoryEntry.maxBPP is a "desired bpp" not the absolute max.
        // This was chosen because we think it gives better results to select a higher bpp and let
        // Android downscale the bpp, rather than showing a bitmap of potentially significantly
        // lower color depth.
        IconDirectoryEntry[] expectedEntries = {
                new IconDirectoryEntry(16, 16, 0, 32, 1128, 24086, false),
                new IconDirectoryEntry(32, 32, 0, 32, 4264, 19822, false),
                new IconDirectoryEntry(48, 48, 0, 32, 9640, 10182, false)
        };

        IconDirectoryEntry[] directory = decoder.getIconDirectory();
        fAssertTrue("NVIDIA icon directory must contain at least one entry.", directory.length > 0);
        for (int i = 0; i < directory.length; i++) {
            if (expectedEntries[i].getWidth() > directory[directory.length - 1].getWidth()) {
                // This test-case has been discarded due to being over-sized. Next.
                // All subsequent cases will be too.
                fAssertTrue("At least one test-case should not have been discarded.", i > 0);
                break;
            }

            // Verify the actual Icon Directory entry was as expected.
            fAssertEquals(directory[i] + " is expected to be equal to " + expectedEntries[i],
                          0, directory[i].compareTo(expectedEntries[i]));
        }
    }

    /**
     * Decode and verify a Golem.de favicon with five bitmaps: 256x256, 48x48, 32x32, 24x24, 16x16
     * Only the 256x256 is a PNG payload. All others are BMP.
     */
    private void testGolemFavicon() throws IOException {
        byte[] icoBytes = readICO("golem_favicon.ico");
        fAssertEquals("Expecting Golem favicon to be 40648 bytes.", 40648, icoBytes.length);

        ICODecoder decoder = new ICODecoder(getInstrumentation().getTargetContext(), icoBytes, 0,
                                            icoBytes.length);
        fAssertNotNull("Expecting Golem favicon to not fail decoding.", decoder.decode());

        // Verify the five entries were correctly identified.
        IconDirectoryEntry[] expectedEntries = {
                new IconDirectoryEntry(16, 16, 0, 32, 1128, 39250, false),
                new IconDirectoryEntry(24, 24, 0, 32, 2488, 37032, false),
                new IconDirectoryEntry(32, 32, 0, 32, 4392, 32640, false),
                new IconDirectoryEntry(48, 48, 0, 32, 9832, 22808, false),
                new IconDirectoryEntry(256, 256, 0, 32, 22722, 86, true)
        };

        IconDirectoryEntry[] directory = decoder.getIconDirectory();
        fAssertTrue("Golem icon directory must contain at least one entry.", directory.length > 0);
        for (int i = 0; i < directory.length; i++) {
            if (expectedEntries[i].getWidth() > directory[directory.length - 1].getWidth()) {
                // This test-case has been discarded due to being over-sized.
                // All subsequent cases will be too.
                fAssertTrue("At least one test-case should not have been discarded.", i > 0);
                break;
            }

            // Verify the actual Icon Directory entry was as expected.
            fAssertEquals(directory[i] + " is expected to be equal to " + expectedEntries[i],
                          0, directory[i].compareTo(expectedEntries[i]));
        }

        // How many icon directory entries in the non-maimed favicon?
        mGolemNumIconDirEntries = directory.length;
    }

    /**
     * Verify that deleting the header will make decoding fail.
     */
    private void testMissingHeader() throws IOException {
        byte[] icoBytes = readICO("microsoft_favicon.ico");
        fAssertEquals("Expecting Microsoft favicon to be 17174 bytes.", 17174, icoBytes.length);

        int offsetNoHeader = ICODecoder.ICO_HEADER_LENGTH_BYTES;
        int lenNoHeader = icoBytes.length - ICODecoder.ICO_HEADER_LENGTH_BYTES;
        ICODecoder decoder = new ICODecoder(getInstrumentation().getTargetContext(), icoBytes,
                                            offsetNoHeader, lenNoHeader);
        fAssertNull("Expecting Microsoft favicon to fail decoding.", decoder.decode());
    }

    /**
     * Verify that decoding does not fail if the number of icon directory entries is smaller than
     * the number given in the header.
     */
    private void testCorruptIconDirectory() throws IOException {
        byte[] icoBytes = readICO("golem_favicon.ico");
        fAssertEquals("Expecting Golem favicon to be 40648 bytes.", 40648, icoBytes.length);

        byte[] icoMaimed = new byte[icoBytes.length - ICODecoder.ICO_ICONDIRENTRY_LENGTH_BYTES];
        // Copy the header and first four icon directory entries into icoMaimed.
        System.arraycopy(icoBytes, 0, icoMaimed, 0,
                         ICODecoder.ICO_HEADER_LENGTH_BYTES + 4 * ICODecoder.ICO_ICONDIRENTRY_LENGTH_BYTES);
        // Skip the last icon directory entry.
        System.arraycopy(icoBytes,
                         ICODecoder.ICO_HEADER_LENGTH_BYTES + 5 * ICODecoder.ICO_ICONDIRENTRY_LENGTH_BYTES,
                         icoMaimed,
                         ICODecoder.ICO_HEADER_LENGTH_BYTES + 4 * ICODecoder.ICO_ICONDIRENTRY_LENGTH_BYTES,
                         icoBytes.length - ICODecoder.ICO_HEADER_LENGTH_BYTES - 5 * ICODecoder.ICO_ICONDIRENTRY_LENGTH_BYTES);

        ICODecoder decoder = new ICODecoder(getInstrumentation().getTargetContext(), icoMaimed, 0,
                                            icoMaimed.length);
        fAssertNotNull("Expecting Golem favicon to not fail decoding.", decoder.decode());
        fAssertEquals("Expecting Golem favicon icon directory to contain one less bitmap.",
                      mGolemNumIconDirEntries - 1, decoder.getIconDirectory().length);
    }

    private byte[] readICO(String fileName) throws IOException {
        String filePath = "ico_decoder_favicons" + File.separator + fileName;
        InputStream icoStream = getInstrumentation().getContext().getAssets().open(filePath);
        ByteArrayOutputStream byteStream = new ByteArrayOutputStream(icoStream.available());

        int readByte;
        while ((readByte = icoStream.read()) != -1) {
            byteStream.write(readByte);
        }

        return byteStream.toByteArray();
    }
}

