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
import java.awt.image.ImageProducer;
import java.awt.image.MemoryImageSource;
import java.io.*;
import java.net.URL;
import netscape.javascript.JSObject;

/* This class is private to the composer. It manages the plugins.
 */

class PluginManager {
    public PluginManager(){
    }

    public void registerPlugin(String pluginFileName, String iniContents){
        Properties properties = new Properties();
        File pluginFile = new File(pluginFileName);
        try {
            InputStream stream = new StringBufferInputStream(iniContents);
            properties.load(stream);
            stream.close();
        } catch ( IOException e) {
            System.err.println("Caught exception while parsing .ini contents:\n" + iniContents);
            e.printStackTrace();
        }
        registerPlugins(pluginFile, properties);
        registerEncoders(pluginFile, properties);
        registerEvents(properties);
    }

    public void registerPlugins(File pluginFile, Properties properties){
        Enumeration plugins = null;
        String factoryName = null;
        try {
            factoryName = trimWhitespace(properties.getProperty("netscape.plugin.composer.Factory",
                "netscape.plugin.composer.Factory"));
            Factory factory = (Factory) Class.forName(factoryName).newInstance();
            plugins = factory.getPlugins(pluginFile, properties);
        } catch ( Throwable t ) {
            System.err.println("Caught exception while instantiating " + factoryName);
            t.printStackTrace();
        }
        registerPlugins(categories, plugins);
    }

    public static Enumeration instantiateClassList(String classNames){
        Vector result = new Vector();
        if ( classNames != null ) {
            StringTokenizer tokenizer = new StringTokenizer(classNames, ":");
            while( tokenizer.hasMoreTokens() ){
                String className = PluginManager.trimWhitespace(tokenizer.nextToken());
                try {
                    Object object = Class.forName(className).newInstance();
                    result.addElement(object);
                } catch (Throwable t) {
                    System.err.println("Caught exception while instantiating " + className);
                    t.printStackTrace();
                }
            }
        }
        return result.elements();
    }

    protected void registerEvents(Properties properties){
        String eventHandlers =
            properties.getProperty("netscape.plugin.composer.eventHandlers", "");
        // System.err.println("registerEvents " + eventHandlers);
        registerPlugins(events, eventHandlers);
    }

    protected static void registerPlugins(SortedStringTable table, String classNames) {
        registerPlugins(table, instantiateClassList(classNames));
    }

    protected static void registerPlugins(SortedStringTable table, Enumeration plugins){
        if ( plugins != null ) {
            while(plugins.hasMoreElements()){
                Plugin plugin = (Plugin) plugins.nextElement();
                if ( plugin != null ) {
                    try {
                        table.getOrCreateTable(plugin.getCategory()).put(
                            plugin.getName(), plugin);
                    } catch(Throwable t){
                        System.err.println("Trouble registering plugin:" + plugin);
                        t.printStackTrace();
                    }
                }
            }
        }
    }
    public static String trimWhitespace(String string){
        int length = string.length();
        StringBuffer out = new StringBuffer(length);
        for(int i = 0; i < length; i++ ){
            char c = string.charAt(i);
            if ( !Character.isSpace(c) ) {
                out.append(c);
            }
        }
        return out.toString();
    }
    public void registerEncoders(File pluginFile, Properties properties){
        Enumeration encoderList = null;
        String factoryName = null;
        try {
            factoryName = trimWhitespace(properties.getProperty("netscape.plugin.composer.ImageEncoderFactory",
                "netscape.plugin.composer.ImageEncoderFactory"));
            ImageEncoderFactory factory = (ImageEncoderFactory) Class.forName(factoryName).newInstance();
            encoderList = factory.getImageEncoders(pluginFile, properties);
        } catch ( Throwable t ) {
            System.err.println("Caught exception while instantiating " + factoryName);
            t.printStackTrace();
        }
        if ( encoderList != null ) {
            while(encoderList.hasMoreElements()){
                Object object = null;
                try {
                    object = encoderList.nextElement();
                    ImageEncoder encoder = (ImageEncoder) object;
                    registerEncoderInstance(encoder);
                } catch ( Throwable t ) {
                    System.err.println("Caught exception while processing "
                        + object);
                    t.printStackTrace();
                }
            }
        }
    }

    private void registerEncoderInstance(ImageEncoder encoder){
        ImageEncoder.register(encoder);
        String name = encoder.getName();
        encoders.put(name, encoder);
    }

    public int getNumberOfCategories(){
        return categories.length();
    }

    public int getNumberOfPlugins(int category){
        SortedStringTable categoryTable = (SortedStringTable) categories.get(category);
        if ( categoryTable == null ) {
            return 0;
        }
        return categoryTable.length();
    }
    public String getCategoryName(int category){
        return categories.getKey(category);
    }
    public String getPluginName(int type, int index){
        return getPlugin(type, index).getName();
    }
    public String getPluginHint(int type, int index){
        return getPlugin(type, index).getHint();
    }

    /** Start a plugin operation.
     */
    public boolean performPlugin(Composer composer, int category, int index,
        String in, String base, String workDirectory, String workDirectoryURL,
        JSObject jsobject){
        return performPlugin2(composer, getPlugin(category, index), in, base,
            workDirectory, workDirectoryURL, null, null,jsobject);
    }

    public boolean performPluginByName(Composer composer, String pluginName,
        String in, String base, String workDirectory, String workDirectoryURL,
        JSObject jsobject){
        Plugin plugin = null;
        String destinationURL = null;
        String eventName = null;
        try {
            int eventMarker = pluginName.indexOf(':');
            if ( eventMarker >= 0 ) {
                eventName = pluginName.substring(0,eventMarker);
                destinationURL = pluginName.substring(eventMarker + 1);
                if ( destinationURL.length() == 0 ) {
                    destinationURL = null;
                }
                SortedStringTable table = events;
                // System.err.println("Executing plug-ins for event " + eventName + " " + table);
                if ( table != null ) {
                    plugin = new GroupPlugin(table);
                }
            }
            else {
                plugin = (Plugin) Class.forName(pluginName).newInstance();
            }
            if ( plugin != null ) {
                return performPlugin2(composer, plugin, in, base,
                    workDirectory, workDirectoryURL, eventName, destinationURL, jsobject);
            }
        }
        catch(Throwable t){
            t.printStackTrace();
        }
        return false;
    }

    public boolean performPlugin2(Composer composer, Plugin plugin,
        String in, String base, String workDirectory, String workDirectoryURL,
        String eventName, String destinationURL, JSObject jsobject){
        String result = in;
        if ( plugin != null ){
            try {
                URL baseURL = base != null ? new URL(base) : null;
                URL workDirectoryURLURL = null;
                URL destinationURLURL = null;
                if ( workDirectoryURL != null ) {
                    String slash = "/";
                    // The workDirectoryURL needs to end in a "/" so that
                    // relative URLs work.
                    if ( ! workDirectoryURL.endsWith(slash) ){
                        workDirectoryURL = workDirectoryURL + slash;
                    }
                    // Netscape on Windows likes urls of the form
                    // file:///C|/temp/
                    // But this freaks out the URL parser in java.
                    // Try to work around this by matching the file:
                    // part.
                    String prefix = "file:";
                    if ( workDirectoryURL.indexOf(prefix) == 0 ){
                        workDirectoryURLURL = new URL("file","",
                            workDirectoryURL.substring(prefix.length()));
                    }
                    else {
                       workDirectoryURLURL = new URL(workDirectoryURL);
                    }
                    //System.err.println("Source: " + workDirectoryURL);
                    //System.err.println("Simple result: " + workDirectoryURLURL );
                    //System.err.println("URL(file,,///C|/temp) = "+ new URL("file","","///C|/temp"));
                    //System.err.println("Relative URL: " + new URL(workDirectoryURLURL, "Floop.jpg"));
                }

                if ( destinationURL != null ) {
                    // Netscape on Windows likes urls of the form
                    // file:///C|/temp/
                    // But this freaks out the URL parser in java.
                    // Try to work around this by matching the file:
                    // part.
                    String prefix = "file:";
                    if ( destinationURL.indexOf(prefix) == 0 ){
                        destinationURLURL = new URL("file","",
                            destinationURL.substring(prefix.length()));
                    }
                    else {
                       destinationURLURL = new URL(destinationURL);
                    }
                }
                ComposerDocument document = new ComposerDocument(composer, in, baseURL,
                    workDirectory, workDirectoryURLURL, eventName, destinationURLURL,
                    jsobject);
                SecurityManager.enablePrivilege("SuperUser");
                ThreadGroup threadGroup = new ThreadGroup(
                    Thread.currentThread().getThreadGroup(),
                    plugin.getName());
                Thread thread = new Thread(threadGroup, new PluginRunner(plugin, document, this),
                    plugin.getName());
                thread.start();
                pluginThreads.put(composer, threadGroup);
            } catch (IOException e) {
              System.err.println("Composer plugin runner threw this exception:");
               e.printStackTrace();
                return false;
            }
        }
        else {
            return false;
        }
        return true;
    }

    /** Stop a plugin operation
     */

    public void stopPlugin(Composer composer){
        killGroup(composer);
    }

    public int getNumberOfEncoders(){
        return encoders.length();
    }
    public String getEncoderName(int index){
        return getEncoder(index).getName();
    }
    public String getEncoderFileType(int index){
        byte[] fileType = new byte[4];
        getEncoder(index).getFileType(fileType);
        return new String(fileType, 0);
    }
    public boolean getEncoderNeedsQuantizedSource(int index){
        return getEncoder(index).needsQuantizedSource();
    }
    public String getEncoderFileExtension(int index){
        return getEncoder(index).getFileExtension();
    }
    public String getEncoderHint(int index){
        return getEncoder(index).getHint();
    }

    protected ImageEncoder getEncoder(int index) {
        return (ImageEncoder) encoders.get(index);
    }

    /** Start an encoder operation.
     */
    public boolean startEncoder(Composer composer, int index,
        int width, int height, byte[][] pixels,
        String fileName){
        Plugin encoderPlugin = new ImageEncoderPlugin(getEncoder(index),
            width, height, pixels, fileName);
        return performPlugin2(composer, encoderPlugin, null, null, null, null, null, null,null);
    }

    protected Plugin getPlugin(int category, int index){
        Plugin result = null;
        SortedStringTable categoryTable = (SortedStringTable) categories.get(category);
        if ( categoryTable != null ) {
            result = (Plugin) categoryTable.get(index);
        }
        return result;
    }

    void pluginFinished(Composer composer, int status, Object argument){
        composer.pluginFinished(status, argument);
        killGroup(composer);
    }

    void killGroup(Composer composer){
        ThreadGroup group = (ThreadGroup) pluginThreads.remove(composer);
        if ( group != null ) {
            pluginKiller.kill(group);
        }
    }
    private SortedStringTable categories = new SortedStringTable();
    private SortedStringTable encoders = new SortedStringTable();
    private SortedStringTable events = new SortedStringTable();
    private Hashtable pluginThreads = new Hashtable(); // Composer composer, Thread
    private PluginKiller pluginKiller = new PluginKiller();
}

class PluginRunner implements Runnable {
    public PluginRunner(Plugin plugin, ComposerDocument document,
        PluginManager manager){
        this.plugin = plugin;
        this.document = document;
        this.manager = manager;
    }
    public void run() {
        Composer composer = document.getComposer();
        // Take this out someday when security manager works better.
        // SecurityManager.enablePrivilege("SuperUser");
        try {
            boolean result = plugin.perform(document);
            manager.pluginFinished(composer, result ? Composer.PLUGIN_OK : Composer.PLUGIN_CANCEL, null);
        } catch( ThreadDeath t) {
            System.err.println("Composer plugin " + plugin + " was killed.");
        } catch (Throwable t){
            System.err.println("Composer plugin " + plugin + " threw this exception:");
            t.printStackTrace();
            manager.pluginFinished(composer, Composer.PLUGIN_FAIL, t.toString());
        }
    }
    private Plugin plugin;
    private ComposerDocument document;
    private PluginManager manager;
}

class ImageEncoderPlugin extends Plugin {
    public ImageEncoderPlugin(ImageEncoder encoder,
        int width, int height, byte[][] pixels,
        String fileName) {
        this.encoder = encoder;
        this.width = width;
        this.height = height;
        this.pixels = pixels;
        this.fileName = fileName;
    }
    public boolean perform(Document document) throws IOException {
        // Create the image.
        int length = width * height;
        int[] pixels2 = new int[length];
        int i = 0;
        for(int y = 0; y < height; y++) {
            int j = 0;
            byte[] line = pixels[y];
            for(int x = 0; x < width; x++ ) {
                pixels2[i++] = (0xff << 24)
                    | ((0xff & line[j]) << 16)
                    | ((0xff & line[j+1]) << 8)
                    | ((0xff & line[j+2]));
                j += 3;
            }
        }
        ImageProducer source = new MemoryImageSource(width, height, pixels2, 0, width);
        ByteArrayOutputStream temp = new ByteArrayOutputStream(width*height/2);
        boolean result = encoder.encode(source, temp);
        if ( result ) {
           // Create the output
           SecurityManager.enablePrivilege("SuperUser");
           OutputStream output = new FileOutputStream(new File(fileName));
           output.write(temp.toByteArray());
           output.close();
        }
        return result;
    }
    private ImageEncoder encoder;
    private int width;
    private int height;
    private byte[][] pixels;
    private String fileName;
}

/** Processes a sequence of plugins.
 */

class GroupPlugin extends Plugin {
    public GroupPlugin(SortedStringTable table){
        table_ = table;
    }
    public boolean perform(Document document) throws IOException {
        // Perform each of the plug-ins in order
        boolean result = true;
        // System.err.println("GroupPlugin ready to go! " + table_);
        if ( table_ != null ) {
            Enumeration categories = table_.values();
            while ( categories.hasMoreElements() && result ){
                try {
                    SortedStringTable category =
                        (SortedStringTable) categories.nextElement();
                    Enumeration plugins = category.values();
                    while ( plugins.hasMoreElements() && result ){
                        Plugin plugin = null;
                        try {
                            plugin = (Plugin) plugins.nextElement();
                            result = plugin.perform(document);
                        }
                        catch(Throwable t){
                            System.err.print("Exception while executing " + plugin);
                            t.printStackTrace();
                            result = false;
                        }
                    }

                }
                catch(Throwable t){
                    t.printStackTrace();
                    result = false;
                }
            }
        }
        return result;
    }
    SortedStringTable table_;
}

class PluginKiller implements Runnable {
    PluginKiller() {
        list = new Vector();
        // Temporary security work-around for pre-4.0PR3
        SecurityManager.enablePrivilege("SuperUser");
        Thread killer = new Thread(this, "Composer Plug-in Killer");
        killer.start();
    }

    synchronized void kill(ThreadGroup group) {
        list.addElement(group);
        notifyAll();
    }

    synchronized ThreadGroup getPluginThreadGroup() {
        while (list.isEmpty()) {
            try {
                wait();
            } catch (InterruptedException e) {
            }
        }

        int last = list.size() - 1;
        ThreadGroup threadGroup = (ThreadGroup) list.elementAt(last);
        list.removeElementAt(last);
        return threadGroup;
    }

    public void run() {
        while (true) {

            // this thread cannot die for any reason.
            // Catch any exception and keep killing
            try {
                ThreadGroup group = getPluginThreadGroup();

                synchronized (group) {
                    //System.err.println("killing threadgroup " + group);

                    // Give the thread some time to exit. If it exits
                    // sooner then we will be notified when the group runs
                    // out of threads.
                    try {
                        group.wait(300);
                    } catch (InterruptedException e) {
                    }

                    // Check and see if thread is really gone
                    if (group.activeCount() > 0) {
                        // Gun down the remaining threads.
                        try {
                            SecurityManager.enablePrivilege("SuperUser");
                            group.stop();
                            group.wait(5000);
                        } catch (InterruptedException e) {
                        }
                        finally {
                            SecurityManager.revertPrivilege();
                        }
                    }

                    // Finally, destroy the thread group
                    try {
                        SecurityManager.enablePrivilege("SuperUser");
                        group.destroy();
                    } catch (Exception e) {
                    }
                    finally {
                        SecurityManager.revertPrivilege();
                    }
                }
            } catch (Exception e) {
                System.out.println("Exception occurred while deleting Composer Plug-in thread group: ");
                e.printStackTrace();
            }
        }
    }

    public String toString() {
        return new String("Plugin thread killer! Composer plugins to be disposed of: " + list);
    }

    // the list of composer plug-in groups to finish
    private Vector list; // Of ThreadGroup

}
