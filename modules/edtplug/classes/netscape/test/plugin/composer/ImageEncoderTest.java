/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package netscape.test.plugin.composer;
import netscape.plugin.composer.ImageEncoder;
import java.awt.Image;
import java.awt.image.*;
import java.io.*;

public class ImageEncoderTest {
    public static void Test(String[] args, ImageEncoder encoder){
        ImageEncoderTest test = new ImageEncoderTest(args);
        test.test(encoder);
        System.exit(0);
    }

    public ImageEncoderTest(String[] args){
    }

    public void test(ImageEncoder encoder){
        try {
            // plugin.setProperties(createProperties());
            System.out.println("ImageEncoder class: " + encoder.getClass().toString());
            System.out.println("              name: " + encoder.getName());
            System.out.println("    file extension: " + encoder.getFileExtension());
            byte[] type = new byte[4];
            encoder.getFileType(type);
            System.out.print("    file type: ");
            for(int i = 0; i < 4; i++ ) {
                System.out.print(" " + type[i] + "'" + (char) (type[i]) + "'");
            }
            System.out.println("\n");
            System.out.println("        hint: " + encoder.getHint());
            System.out.println("needsQuantizedSource: " + encoder.needsQuantizedSource());
            System.out.println("-------------------------------");
            File file = new File("Test." + encoder.getFileExtension());
            OutputStream out = new FileOutputStream(file);
            ImageProducer in = createImageProducer(encoder.needsQuantizedSource());
            boolean result = encoder.encode(in, out);
            System.out.println("-------------------------------");
            System.out.println("Encoder returned: " + result);
            out.close();
        } catch ( IOException e) {
            e.printStackTrace();
        }
    }

    protected ImageProducer createImageProducer(boolean needsQuantizedSource){
        int w = 512;
        int h = 512;
        int pix[] = new int[w * h];
        int index = 0;
        int green = 0;
        for (int y = 0; y < h; y++) {
            int red = y & 0xff;
            if ( needsQuantizedSource ) {
                red = ( y & 0xf ) << 4;
            }
            for (int x = 0; x < w; x++) {
                int blue = x & 0xff;
                if ( needsQuantizedSource ) {
                    blue = ( x & 0xf ) << 4;
                }
                if ( ! needsQuantizedSource ) {
                    green = ((x + y) / 4 ) & 0xff;
                }
                pix[index++] = (255 << 24) | (red << 16) | blue;
            }
        }
        return new MemoryImageSource(w, h, pix, 0, w);
    }
}
