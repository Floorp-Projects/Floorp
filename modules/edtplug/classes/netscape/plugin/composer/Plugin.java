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
import java.util.ResourceBundle;

/** Plugins allow arbitrary transformations of the composer's current document.
 * All composer plugins descend from this class. When the user invokes a plugin,
 * the plugin's perform method is called with the current document. The plugin
 * can then examine the existing document and modify it.
 *
 * Plugins are like Applet or Application, in that they are the main class of your
 * mini appication. This is where the flow of control transfers from the editor to
 * you.
 *
 * Your plugin is instantiated when the first composer window is opened, and is
 * deleted when the application exits.
 *
 * The document can change arbitrarily between calls to perform. You may even be
 * given two different documents on two successive calls to perform.
 *<p>
 * Categories are used in the user interface to collect plugins into groups that
 * are meaningful to users. You can use your own category to put all of your
 * product's plugins into one category. Or, you can place your plugins into
 * several categories. It's up to you.
 *
 * <p>Packageing Plugins
 * <p>To package your plugin, create an uncompressed zip file with the file name
 * cpXXX.zip, where XXX can be anything you want. (It has to be unique, so use
 * your company name, or the plugin name, or something like that.)
 * Put all your classes into that .zip file. At the top level of the .zip archive,
 * add a file called "netscape_plugin_composer.ini".
 * <p> Format of the "netscape_plugin_composer.ini" file.
 * <p> It is in standard java "Properties" file format.
 * The following properties are 'well known', and are used by the composer plugin
 * framework:
 * <pre>
 * netscape.plugin.composer.factory - a single classname of a Factory class.
 * netscape.plugin.composer.classes - a colon-seperated list of the classnames of
 *     the plugins in this archive.
 * netscape.plugin.composer.eventHandlers - a colon-seperated list of the classnames
 *     of the plugins in this archive to execute when an event happens. See Document
 *     for a list of the standard events.
 * </pre>
 * <p> Your plugin's class name should be listed as the value of the netscape.plugin.composer.classes
 * property. You can place several plugins in the same plugin file. All the plugin names should be
 * listed in the netscape.plugin.composer.classes property.
 * <p> Alternatively, you can use a factory class to create your plugins at runtime.
 * <p>Installing the Plugin
 * <p> Put that .zip file in the Netscape Plugins directory (or a subdirectory.) Then restart the
 * Composer application.
 * @see netscape.plugin.composer.Factory
 * @see netscape.plugin.composer.Document
 *
 */
public class Plugin {
    /** The default constructor for a Plugin. If you have a default constructor, it must be public.
     * so that your plugin can be instantiated by name. (Also, your plugin class must be a public
     * class so that it can be instantiated by name.)
     */
    public Plugin() {}

    /** Get the human readable name of the plugin. Defaults to the name of the plugin class. This
     * text will show up as the text of a menu item.
     * @return the human readable name of the plugin.
     */
    public String getName()
    {
        return getClass().getName();
    }

    /** Get the human readable category text of the plugin. Defaults to the name of the plugin class. This
     * text will show up as the title of a menu that contains the plugin. If several plugins use the
     * same category, they will all show up in the same menu.
     * @return the human readable category of the plugin.
     */
    public String getCategory()
    {
        return getClass().getName();
    }

    /** Get the human readable hint text for the plugin. This is a one-sentence description of
     * what the plugin does. Defaults to the name of the plugin class. This text will
     * show up in the status line as the user moves the mouse over the plug-in's menu item.
     * @return the human readable hint for the plugin.
     */
    public String getHint()
    {
        return getClass().getName();
    }

    /** Execute the plugin.
     * Override this method to perform the bulk of your work.
     * This is where your plugin gets the text out of the document, analyzes it, interacts
     * with the user, and modifies the text of the document.
     * <p>
     * The rest of the composer user interface is held in a modal state while your plugin
     * is executing. So either finish quickly, or display a progress dialog, or otherwise
     * let the user know what's going on.
     * <p>
     * By default this method returns false.
     * @param document the current document.
     * @return true if the changes to the document should be permenent. False if the
     * changes should be cancled. (Return false if the user Cancels the operation.)
     */
    public boolean perform(Document document) throws IOException {
        return false;
    }
}
