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

package netscape.plugin.composer;
import java.util.*;
import java.io.*;

/** Used to create image encoders at runtime. When the composer starts up, it examins
 * each plugin file's netscape_plugin_composer.ini file to see if it defines the property
 * "netscape.plugin.composer.ImageEncoderFactory".
 * If it does, the value of that property is taken to be a classname of a class
 * derived from ImageEncoderFactory. That class is instantiated, and it's getImageEncoders method
 * is called. This method is responsible for creating and returning an enumerator of
 * ImageEncoders.
 *
 * <p>Most developers don't need to use this class -- they can simply use the
 * "netscape.plugin.composer.ImageEncoder.classes" property to name their
 * ImageEncoder classes
 * directly. However, the ImageEncoderFactory class provides additional flexability. Some
 * possible uses of a ImageEncoderFactory class include:
 * <ol><li>A wrapper for another application's image encoders. The wrapper would
 * search the other application's data base, find the other application's image encoders,
 * and synthasize ImageEncoder instances that wrapped the other application's
 * image encoders.
 * <li>A remote repository of image encoders that's searched at run time.
 * </ol>
 */
public class ImageEncoderFactory {
    /** If you have a default constructor, it must be public, or else you class cannot be
     * instantiated by name. (And your class should be public, too.)
     */
    public ImageEncoderFactory() {}
    /** Return an Enumeration of Plugins.
     * By default, searches the properties for the property "netscape.plugin.composer.ImageEncoder.classes",
     * and creates one instance of each of the plugins names in that string.
     * @param pluginFile The plugin file that's being scanned.
     * @param properties The properties of the plugin file that's being scanned.
     * @return the ImageEncoders to add to the Composer.
     *
    */
    public Enumeration getImageEncoders(File pluginFileName, Properties properties){
        String classNames = properties.getProperty("netscape.plugin.composer.ImageEncoder.classes");
        return PluginManager.instantiateClassList(classNames);
    }
}
