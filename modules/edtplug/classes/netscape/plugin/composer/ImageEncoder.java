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

package netscape.plugin.composer;

import java.io.IOException;
import java.io.OutputStream;
import java.awt.image.ImageProducer;
import java.util.Vector;
import java.util.Enumeration;

/** ImageEncoders allow image data to be encoded to match specific image formats.
 * All composer image encoders descend from this class. When the user asks for
 * an image to be encoded, the image encoder's encode method is called with
 * the image data and an output stream. The encoder examins the input data,
 * communicates with the user as nescessary, and then writes the encoded
 * image to the output stream.
 *
 * ImageEncoders are like Applet or Application, in that they are the main class
 * of your mini appication. This is where the flow of control transfers from the
 * editor to you.
 *
 * Your encoder is instantiated when the first composer window is opened, and is
 * deleted when the application exits.
 *
 * <p>Packageing Image Encoders
 * <p>To package your image encoder, create an uncompressed zip file with the file name
 * cpXXX.zip, where XXX can be anything you want. (It has to be unique, so use
 * your company name, or the plugin name, or something like that.)
 * Put all your classes into that .zip file. At the top level of the .zip archive,
 * add a file called "netscape_plugin_composer.ini".
 * <p> Format of the "netscape_plugin_composer.ini" file.
 * <p> It is in standard java "Properties" file format.
 * The following properties are 'well known', and are used by the composer plugin
 * framework:
 * <pre>
 * netscape.plugin.composer.imageEncoder.factory - a single classname of a Factory class.
 * netscape.plugin.composer.imageEncoder.classes - a colon-seperated list of the classnames of the plugins in
 * this archive.
 * </pre>
 * <p> Your encoder's class name should be listed as the value of the
 * netscape.plugin.composer.imageEncoder.classes
 * property. You can place several encoders in the same plugin file. All the encoder names should be
 * listed in the netscape.plugin.composer.imageEncoder.classes property.
 * <p> Alternatively, you can use a factory class to create your image encoders at runtime.
 * <p>
 * <p>Installing the Image Encoder package.
 * <p> Put that .zip file in the Netscape Plugins directory (or a subdirectory.) Then restart the
 * Composer application.
 * @see netscape.plugin.composer.Plugin
 * @see netscape.plugin.composer.ImageEncoderFactory
 *
 */
public class ImageEncoder {
    /** The default constructor for an image encoder. If you have a default constructor, it must be public.
     * so that your image encoder can be instantiated by name. (Also, your image encoder class must be a public
     * class so that it can be instantiated by name.)
     */
    public ImageEncoder() {}

    /** Get the human readable name of the image encoder. Defaults to the name of the image encoder class. This
     * text will show up as the text of a menu item.
     * @return the human readable name of the image encoder.
     */
    public String getName()
    {
        return getClass().getName();
    }

    /** Get the human readable hint text for the image encoder. This is a one-sentence description of
     * what the image encoder does. Defaults to the name of the image encoder class. This text will
     * show up in the status line as the user moves the mouse over the plug-in's menu item.
     * @return the human readable hint for the image encoder.
     */
    public String getHint()
    {
        return getClass().getName();
    }

    /** Get the standard file extension of the image format. Defaults to the
     * empty string. Does not include the period.
     * @return the human readable category of the image encoder.
     */
    public String getFileExtension()
    {
        return new String();
    }

    /** Get the Macintosh FileType of the image format. Defaults to '????'.
     * @param type - a 4 byte array to be filled in with the FileType of the image format.
     */
    public void getFileType(byte[] type)
    {
        type[0] = '?';
        type[1] = '?';
        type[2] = '?';
        type[3] = '?';
    }

    /** Does the source image have to be quantized? If this method returns true,
     * then the source image will be quantized (by some method) to contain
     * 256 or fewer distinct colors. If you want to use a specific quantization
     * technique, or if you want to interact with the user to optionally quantize,
     * then you will have to return false from this method, and supply your own
     * quantization code.
     * <p> Returns false by default.
     * @return true if the source image must be quantized before it is passed to encode().
     */
    public boolean needsQuantizedSource(){
        return false;
    }

    /** Encode the image.
     * Override this method to perform the bulk of your work.
     * This is where your image encoder reads the image information out of the
     * image source, interacts
     * with the user, and writes the encoded information to the output stream.
     * <p>
     * The rest of the composer user interface is held in a modal state while your image encoder
     * is executing. So either finish quickly, or display a progress dialog, or otherwise
     * let the user know what's going on.
     * <p>
     * By default this method returns false.
     * @param source the image to encode.
     * @param destination the output stream to write the encoded information to.
     * @return true if the encoding should be permenent. False if the
     * encoding should be cancled. (Return false if the user Cancels the operation.)
     */
    public boolean encode(ImageProducer source, OutputStream output) throws IOException {
        return false;
    }

    /** Used to register an encoder with the system. Registered encoders are
     * available for any program to use.
     * @param encoder An encoder to be register.
     */
    public static void register(ImageEncoder encoder) {
        encoders_.addElement(encoder);
    }
    /* Returns the registered encoders.
     */
    public static Enumeration encoders() {
        return encoders_.elements();
    }
    private static Vector encoders_ = new Vector();
}
