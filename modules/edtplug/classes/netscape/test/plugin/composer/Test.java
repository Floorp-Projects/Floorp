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

import netscape.plugin.composer.*;
import java.io.*;
import java.util.Properties;
import java.net.*;
import java.util.*;

/** The composer plugin testbed. Allows you to test your plugin
 * seperately from the Netscape Composer.
 *
 * <pre>
 *
 * You run Test from the command line by doing this:
 *
 * java -classpath classPath netscape.test.plugin.composer.Test ..args..
 *
 * Where ..args.. are one or more of:
 *
 *  -plugin plugin    Use the named plugin. Defaults to
 *                    netscape.plugin.composer.Plugin
 *  -in file          Use the named file as the input text document.
 *  -out file         Use the named file as the output text document.
 *  -string string    Use the given string as the test document.
 *                    Defaults to a small built-in test document.
 *  -workDir dir      Use the given dir as the work directory.
 *                    Defaults to java's user.dir property.
 *  -eventName event  Simulate an event of the given name.
 * </pre>
 *
 * Normally you don't derive from or instantiate Test. You just run it from the command
 * line. However, for special test purposes, it might make sense to derive from Test and
 * override methods like createDocument and createPlugin.
 */
public class Test {
    /** Hook for plugins to test themselves. If you have a plugin MyPlugin, add this
     * code to it to enable it to be tested in a stand-alone situation.
     * <pre>
     * /** Test the plugin. Not required for normal operation of the plugin.
     *  * You can use this to run the plugin from the command line:
     *  * java -classpath <your-class-path> <your-plugin-name> args...
     *  * where args... are passed on to the Test class.
     *  * You can remove this code before shipping your plugin.
     *  * /
     * static public void main(String[] args) {
     *   Test.perform(args, new MyPlugin());
     * }
     * </pre>
     * Note: This method calls System.exit() at the end of the test.
     *       It does so because otherwise any AWT threads that may have been
     *       started will not be closed down, and as a result, the application will
     *       never finish running.
     * <p>
     * The system exit code will be 0 if the test succeded.
     * 1 if the test failed.
     * -1 if an uncaught exception was thrown.
     */

    public static void perform(String[] args, Plugin plugin){
        int status = 0;
        try {
            Test test = new Test(args);
            test.setPlugin(plugin);
            status = test.test();
        } catch (Throwable t) {
            System.err.println("Exception caught while running test.");
            t.printStackTrace();
            status = -1;
        }
        System.exit(status);
    }

    /** Public main function. Called from outside. Creates and runs the test.
     * If you derive a new class from Test, you can make your own version of
     * main that looks like this:
     * <pre>
     * public static void main(String[] args){
     *     int status = 0;
     *     try {
     *         MyTest test = new MyTest(args);
     *         status = test.test();
     *     } catch (Throwable t) {
     *         t.printStackTrace();
     *         status = -1;
     *     }
     *     System.exit(status);
     * }
     * </pre>
     */
    public static void main(String[] args){
        int status = 0;
        try {
            Test test = new Test(args);
            status = test.test();
        } catch (Throwable t) {
            t.printStackTrace();
            status = -1;
        }
        System.exit(status);
    }

    /** Parses the command-line arguments.
     * @param args The command-line arguments.
     */

    protected Test(String[] args){

        int length = args.length;
        for(int i = 0; i < length; i++ ){
            String s = args[i];
            if ( s.equalsIgnoreCase("-plugin") ){
                argCheck(args, i, 1);
                duplicateCheck(args, i, pluginArg);
                pluginArg = args[++i];
            }
            else if ( s.equalsIgnoreCase("-in") ){
                argCheck(args, i, 1);
                duplicateCheck(args, i, inArg);
                inArg = args[++i];
            }
            else if ( s.equalsIgnoreCase("-out") ){
                argCheck(args, i, 1);
                duplicateCheck(args, i, outArg);
                outArg = args[++i];
            }
            else if ( s.equalsIgnoreCase("-string") ){
                argCheck(args, i, 1);
                duplicateCheck(args, i, stringArg);
                stringArg = args[++i];
            }
            else if ( s.equalsIgnoreCase("-workDir") ){
                argCheck(args, i, 1);
                duplicateCheck(args, i, workDirArg);
                workDirArg = args[++i];
            }
            else if ( s.equalsIgnoreCase("-eventName") ){
                argCheck(args, i, 1);
                duplicateCheck(args, i, eventNameArg);
                eventNameArg = args[++i];
            }
            else {
                usage("Don't understand argument " + s);
            }
        }
        if ( inArg != null && stringArg != null ) {
            usage("Can't specify both -in and -string. Choose one.");
        }
    }

    private void duplicateCheck(String[] args, int index, String arg){
        if ( arg != null ) {
            usage("duplicate flag not allowed. Only specify one " + args[index]);
        }
    }

    private void argCheck(String[] args, int index, int needed){
        if ( args.length <= index + needed ) {
            usage("flag " + args[index] + " needs " + needed + " arguments.");
        }
    }

    /** Prints usage information, and then exits.
     * @param error A string to print to System.err just before printing the usage info.
     */

    protected void usage(String error){
        System.err.println(error);
        System.err.println("usage: [-plugin plugin][-in file | -string string][-out file][-workDir dir]\n"
            + "  -plugin plugin    Use the named plugin. Defaults to\n"
            + "                    netscape.plugin.composer.Plugin\n"
            + "  -in file          Use the named file as the input document.\n"
            + "  -out file         Use the named file as the output document.\n"
            + "  -string string    Use the given string as the test document.\n"
            + "                    Defaults to a small built-in test document.\n"
            + "  -workDir dir      Use the given dir as the work directory.\n"
            + "                    Defaults to java's user.dir property.\n"
            + "  -eventName event  Simulate an event of the given name.\n"
        );
        System.exit(-1);
    }

    /** Perform the test. Override to implement an alternative test
     * technique.
     * @return 0 if the plug-in returned false, or 1 if the plug-in returned true.
     */
    protected int test() throws IOException {
        int status = 0;
        TestDocument document = createDocument();
        Plugin plugin = createPlugin();
        // plugin.setProperties(createProperties());
        System.out.println("Plugin class: " + plugin.getClass().toString());
        System.out.println("    category: " + plugin.getCategory());
        System.out.println("        name: " + plugin.getName());
        System.out.println("        hint: " + plugin.getHint());
        System.out.println("-------------------------------");
        System.out.println("Original Document:\n");
        System.out.println("             base: " + document.getBase());
        System.out.println("    workDirectory: " + document.getWorkDirectory());
        System.out.println(" workDirectoryURL: " + document.getWorkDirectoryURL());
        System.out.println(" documentURL: " + document.getDocumentURL());
        System.out.println(" eventName: " + document.getEventName());
        System.out.println("Text:\n" + document.getText());
        System.out.println("-------------------------------");
        System.out.println("Calling plugin...\n");
        String startText = document.getText();
        boolean result = plugin.perform(document);
        System.out.println("-------------------------------");
        System.out.println("Plugin.perform returned: " + result);
        System.out.println("-------------------------------");
        if ( result && outArg != null ) {
            writeDoc(document.getText(), new File(outArg));
        }
        if ( result ) {
            status = 0;
        }
        else {
            status = 1;
        }
        return status;
    }

    /** Writes the document text to the supplied file.
     */
    protected void writeDoc(String doc, File out){
        try {
            FileWriter s = new FileWriter(out);
            s.write(doc);
            s.close();
        } catch(IOException e){
            e.printStackTrace();
        }
    }

    /** Called when the plug-in that's being tested has set some new text into the test document.
     * The default implementation just prints the text to System.out.
     * Override this if you want to do something more sophisiticated,
     * like comparing the old text with the new text.
     * @param text the new text of the test document.
     */

    protected void newText(String text){
        System.out.println("-------------------------------");
        System.out.println("New Document text:\n");
        System.out.println(text);
    }

    /** Called when the plug-in that's being tested has called redirectDocumentOpen.
     * The default implementation just prints the new URL to System.out.
     * Override this if you want to do something more sophisticated.
     * @param eventName the current event. (Should always be "edit", or else the plug-in
     * behaving incorrectly.)
     * @param newURL the new URL that the open should be redirected to.
     */
    public void reportRedirectDocumentOpen(String eventName, String newURL){
        System.out.println("-------------------------------");
        System.out.println("redirectDocumentOpen: " + newURL);
        if ( eventName == null || !eventName.equals("edit") ) {
            System.out.println("Warning!!! This call has no effect, because the eventName is "
                + (eventName == null ? "null" : eventName));
        }
    }

    /** Override this to create your own type of document.
     */
    protected TestDocument createDocument() throws IOException {
        String workDir = "";
        URL base = null;
        String text = "";
        if ( workDirArg != null ) {
            workDir = workDirArg;
        }
        workDir = new File(workDir).getAbsolutePath();
        if ( stringArg != null ) {
            text = stringArg;
        }
        else if ( inArg != null ) {
            StringBuffer fileContents = new StringBuffer();
            File file = new File(inArg);
            file = new File(file.getAbsolutePath());
            // System.err.println("file: " + file + " size: " + file.length());
            /* // JDK 1.1 code
            FileReader in = new FileReader(file);
            int c;
            try {
                while ( (c = in.read()) >= 0 ) {
                    fileContents.append( (char) c);
                }
            } catch (Throwable t){
                t.printStackTrace();
            }
            // System.err.println("file contents: " + fileContents.length() + " '" + fileContents + "'");
            in.close();
            text = fileContents.toString();
            */
            FileInputStream in = new FileInputStream(file);
            int c;
            try {
                while ( (c = in.read()) >= 0 ) {
                    fileContents.append( (char) c);
                }
            } catch (Throwable t){
                t.printStackTrace();
            }
            // System.err.println("file contents: " + fileContents.length() + " '" + fileContents + "'");
            in.close();
            text = fileContents.toString();
            base = toURL(file, true);
        }
        else {
            text = "<html><head><title>A test document</title></head>\n"
                + "<body>\n"
                + "<p><a href=\"http://www.netscape.com\">Netscape</a></p>\n"
                + "<p>The&nbsp;<b>quick <i>brown</i></b> fox "
                + "<!-- selection start -->"
                + "<i>jumped</i> "
                + "<!-- selection end -->"
                + "over the lazy dog.</p>\n"
                + "</body>\n"
                + "</html>\n";
            base = toURL(new File(""), false);
        }
        URL docURL = null;
        if ( inArg != null ) {
            docURL = toURL(new File(inArg), false);
        }
        TestDocument document = new TestDocument(this, text, base, workDir,
            toURL(new File(workDir), false), docURL, eventNameArg );
        return document;
    }

    /** Convert a File to a URL.
     * @param file a file.
     * @param base if true, don't include the file's name in the url, just the path. This
     * is handy for creating a base URL for a file.
     * @return the corresponding URL.
     */

    static URL toURL(File file, boolean getBase) throws MalformedURLException {
        String path = file.getAbsolutePath();
        file = new File(path); /* Absolutize. */
        StringTokenizer tokenizer = new StringTokenizer(path, File.separator);
        StringBuffer urlBuf = new StringBuffer("file:");
        boolean bEncodeIllegalCharacters = false;
        String allowedInURLs = "abcdefghijklmnopqrstuvwxyz"
            + "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            + "0123456789"
            + "$-_@.&+-";
        boolean trimLast = getBase && ! file.isDirectory();
        while(tokenizer.hasMoreTokens()) {
            String token =  tokenizer.nextToken();
            if ( trimLast && ! tokenizer.hasMoreTokens() ) {
                break;
            }
            urlBuf.append("/");
            if ( bEncodeIllegalCharacters ) {
                /* Escape letters not allowed in URLs */
                for(int i = 0; i < token.length(); i++){
                    char c = token.charAt(i);
                    if ( allowedInURLs.indexOf(c) < 0 ){
                        /* Not allowed */
                        urlBuf.append("%" + Integer.toHexString(c));
                    }
                    else {
                        urlBuf.append((char) c);
                    }
                }
            }
            else {
                urlBuf.append(token);
            }
        }
        if ( (getBase && ! file.isDirectory())
            || ( file.isDirectory() && ! urlBuf.toString().endsWith("/"))) {
            urlBuf.append("/");
        }
        URL url = new URL(urlBuf.toString());
        return url;
    }
    /** Used to set the plugin programmaticly. Used by the Test.perform method.
     */
    public void setPlugin(Plugin plugin){
        this.plugin = plugin;
    }
    /** Override this to create your own plugin. (Normally you would
     * use the -plugin command line option. Or supply the argument to
     * the Test.perform method.)
     */
    protected Plugin createPlugin(){
        if ( plugin != null ) return plugin;

        String pluginClassName = new String("netscape.plugin.composer.Plugin");
        if ( pluginArg != null ) {
            pluginClassName = pluginArg;
        }
        plugin = null;
        try {
            plugin = (Plugin) Class.forName(pluginClassName).newInstance();
        } catch (Throwable t){
            t.printStackTrace();
        }
        return plugin;
    }

    /** Override this to create your own properties. The default is to read the
     * propertiesArg
     */
    protected Properties createProperties(){
        String propertiesFile = new String("netscape_plugin_composer.ini");
        if ( propertiesArg != null ) {
            propertiesFile = propertiesArg;
        }
        Properties properties = new Properties();
        try {
            FileInputStream in = new FileInputStream(propertiesFile);
            properties.load(in);
            in.close();
        } catch (Throwable t){
            t.printStackTrace();
        }
        return properties;
    }

    /** The argument the user supplied for -properties. Null if they didn't supply one.
     */
    protected String propertiesArg;

    /** The argument the user supplied for -plugin. Null if they didn't supply one.
     */
    protected String pluginArg;
    /** The argument the user supplied for -in. Null if they didn't supply one.
     */
    protected String inArg;
    /** The argument the user supplied for -out. Null if they didn't supply one.
     */
    protected String outArg;
    /** The argument the user supplied for -string. Null if they didn't supply one.
     */
    protected String stringArg;
    /** The argument the user supplied for -workDir. Null if they didn't supply one.
     */
    protected String workDirArg;
    /** The plugin the user supplied programmaticly. Null if they didn't supply one.
     */
    protected Plugin plugin;

    protected String eventNameArg;
}

