/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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
