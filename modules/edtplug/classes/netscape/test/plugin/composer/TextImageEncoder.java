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

import netscape.plugin.composer.*;
import java.io.*;
import java.awt.image.*;
import java.util.*;

public class TextImageEncoder extends ImageEncoder {
    public static void main(String[] args){
        ImageEncoderTest.Test(args, new TextImageEncoder());
    }
    public String getName(){
        return "Text Image Encoder";
    }
    public void getFileType(byte[] type){
        type[0] = 'T';
        type[1] = 'E';
        type[2] = 'X';
        type[3] = 'T';
    }
    public String getFileExtension(){
        return "txt";
    }
    public String getFormatName(){
        return "Text";
    }
    public String getHint(){
        return "Text Image Format";
    }
    public boolean encode(ImageProducer input, OutputStream output)
        throws IOException {
        PrintStream s = new PrintStream(output);
        s.println("I am a test. The image is:");
        ImageConsumer consumer = new TextImageConsumer(s);
        input.addConsumer(consumer);
        input.requestTopDownLeftRightResend(consumer);
        input.removeConsumer(consumer);
        return true;
    }
}


class TextImageConsumer implements ImageConsumer {
    public TextImageConsumer(PrintStream s) {
        stream = s;
    }
    public void imageComplete(int status){
        stream.println("imageComplete(" + status + ")");
    }
    public void setColorModel(ColorModel model){
        stream.println("setColorModel(" + model + ")");
    }
    public void setDimensions(int x , int y) {
        stream.println("setDimensions(" + x + ", " + y + ")");
    }
    public void setHints(int hint) {
        stream.println("setHints(" + hint + ")");
    }
    public void setPixels(int x, int y, int w, int h,
        ColorModel model, byte pixels[], int offset, int scansize) {
        stream.println("setPixels(" + x + ", " + y + ", "
            + w + ", " + h + ", " + model + ", " + pixels
            + ", " + offset + ", " + scansize + ")");
        for(int j = 0; j < h; j++ ) {
            stream.print("[" + j + "]");
            for (int i = 0; i < w; i++ ) {
                stream.print(" " + Integer.toHexString(pixels[scansize * j + i + offset]));
            }
            stream.println();
        }
    }
    public void setPixels(int x, int y, int w, int h,
        ColorModel model, int pixels[], int offset, int scansize) {
        stream.println("setPixels(" + x + ", " + y + ", "
            + w + ", " + h + ", " + model + ", " + pixels
            + ", " + offset + ", " + scansize + ")");
        for(int j = 0; j < h; j++ ) {
            stream.print("[" + j + "]");
            for (int i = 0; i < w; i++ ) {
                stream.print(" " + Integer.toHexString(pixels[scansize * j + i + offset]));
            }
            stream.println();
        }
    }
    public void setProperties(Hashtable properties ) {
    stream.println("setProperties(" + properties + ")");
  }
    private PrintStream stream;
}
