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
