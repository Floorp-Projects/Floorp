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

import java.io.*;
import java.net.*;
import netscape.plugin.composer.*;
import netscape.plugin.composer.io.*;

/** Sample Plugin that adds a "Nervous Text" applet to the document.
 * Shows how to add an applet to the document.
 */

public class AddApplet extends Plugin {
    /** Test the plugin. Not required for normal operation of the plugin.
     * You can use this to run the plugin from the command line:
     * java -classpath <your-class-path> <your-plugin-name> args...
     * where args... are passed on to the Test class.
     * You can remove this code before shipping your plugin.
     */
    static public void main(String[] args) {
        Test.perform(args, new AddApplet());
    }
    /** Get the human readable name of the plugin. Defaults to the name of the plugin class.
     * @return the human readable name of the plugin.
     */
    public String getName()
    {
        return "Nervous Text";
    }

    /** Get the human readable category of the plugin. Defaults to the name of the plugin class.
     * @return the human readable category of the plugin.
     */
    public String getCategory()
    {
        return "Insert";
    }

    /** Get the human readable hint for the plugin. This is a one-sentence description of
     * what the plugin does. Defaults to the name of the plugin class.
     * @return the human readable hint for the plugin.
     */
    public String getHint()
    {
        return "Inserts a 'Nervous Text' java applet.";
    }

    /** Perform the action of the plugin.
     *
     * @param document the current document.
     */
    public boolean perform(Document document) throws IOException{
        // Create a print stream for the new document text.
        PrintWriter out = new PrintWriter(document.getOutput());
        // Create a lexical stream to tokenize the old document text.
        LexicalStream in = new LexicalStream(new SelectedHTMLReader(document.getInput(), out));
        // Keep track of whether or not we are in the selected text.
        // At the beginning of the document we're outside the selection.
        // After we encounter the start-selection comment, we're inside
        // the selection.
        // After we encounter the end-selection comment, we're outside
        // the selection again.
        boolean inSelection = false;
        Comment selectionStart = null;
        for(;;){
            // Get the next token of the document.
            Token token = in.next();
            if ( token == null ) break; //  Null means we've finished the document.
            else if (token instanceof Comment ) {
                Comment comment = (Comment) token;
                if ( comment.isSelectionStart() ){
                    selectionStart = comment;
                    inSelection = true;
                    continue; // Don't print the selection start yet.
                }
                else if (comment.isSelectionEnd() ){
                    inSelection = false;
                    out.print(selectionStart);
                    insertApplet(document, out);
                }
            }
            out.print(token);
        }
        out.close();
        return true;
    }

    void insertApplet(Document document, PrintWriter out){
        String appletClassFile = "NervousText.class";
        writeDataToDisk(document, nervousTextClassData, appletClassFile);
        try {
            // Write the HTML for the applet. Note the "LOCALDATA" property.
            // This property tells the Composer to publish the class file along
            // with the document.
            out.print("<APPLET CODE=\"" + appletClassFile + "\"");
            out.print(" LOCALDATA=\"application/java-vm " + appletClassFile + "\"");
            out.print(" WIDTH=250 HEIGHT=50>");
            out.print("<PARAM NAME=text VALUE=\"Composer Plug-ins\">");
            out.print("</APPLET>");
        } catch (Throwable t){
            t.printStackTrace();
        }
    }

    void writeDataToDisk(Document document, int[] data, String fileName){
        // Copy the class data to the work directory.
        // In order to do this, we need to have the ability access files.
        netscape.security.PrivilegeManager.enablePrivilege("UniversalFileAccess");
        File outFile = new File(document.getWorkDirectory(), fileName);
        System.err.println("File is: " + outFile);
        if ( ! outFile.exists() ){
            try {
                FileOutputStream outStream = new FileOutputStream(outFile);
                int length = data.length;
                for(int i = 0; i < length; i++ ) {
                    int d = data[i];
                    outStream.write((byte) (d >> 24));
                    outStream.write((byte) (d >> 16));
                    outStream.write((byte) (d >> 8));
                    outStream.write((byte) (d));
                }
                outStream.close();
            } catch (Exception e) {
                System.err.println("Couldn't write class file " + fileName + " to directory " +
                    document.getWorkDirectory() );
                e.printStackTrace();
            }
        }
        else {
            System.err.println("File " + outFile + " already exists.");
        }
    }
    // The data for NervousText.class is stored here to simplify this example.
    int[] nervousTextClassData = {-889275714,  196653, 8783872, 2114453577,
        134248455, 4654848, 1929838662, 117464839,
        8652544, 1694957699, 117471751, 6948608,
        1409941511, 2689280, 100675082, 327730,
        167775232, 638124038, 3934720, 134233865,
        393255, 167773952, 805961733, 3344896,
        134229002, 524346, 167774208, 671678470,
        3410432, 201338122, 852027, 167774208,
        956891142, 3607040, 134234122, 262211,
        150996480, 789118982, 2754816, 100674314,
        327729, 167774720, 940310639, 6163456,
        1275087628, 7471185, 201360640, 1644953682,
        8064000, 1711303436, 5046366, 201358336,
        1208746071, 6884352, 1879075596, 7405646,
        201357056, -2146697086, 6163456, 2130729228,
        6488138, 83886080, 0, 1678508135,
        7015424, 1744862476, 5242974, 201359616,
        1577844866, 7670784, 1845521158, 1078067200,
        0, 201351168, 1577844866, 5768768,
        603979776, 12, 5898340, 16781132,
        1768842574, 1970102885, 1918132578, 1818558720,
        222523246, 1937006958, 1951818092, 1969553664,
        189687154, 1987016051, 1415936116, 16780906,
        1635148079, 1818324583, 793600372, 1744896018,
        676096609, 1986080609, 2004102982, 1869509691,
        693502208, 122187636, 1247901281, 16781900,
        1784772193, 795631982, 1731154804, 1919512167,
        989921282, 1531117824, 158557552, 1634886004,
        1701052672, 91452513, 1920205056, 136857929,
        1531136297, 1442906122, 1165517669, 1886677359,
        1853030656, 108160371, 1970103553, 272458,
        693502208, 107702636, 1819108609, 676719,
        1970430821, 1181314149, 16780650, 1635148079,
        1635218479, 1181707892, 16779107, 1869966964,
        1701970176, 354962538, 1635148079, 1635218479,
        1165387118, 1950042441, 693764352, 259287154,
        1700881491, 1970499685, 1852073316, 16783144,
        1282040182, 1630497889, 1852256082, 1970171489,
        1651270971, 693502208, 640175210, 1635148079,
        1818324583, 793998450, 1768843067, 692873825,
        1986080620, 1634625327, 1400140393, 1852259073,
        422497, 1852075885, 16778094, 1970077952,
        158166901, 1936016495, 2003697920, 57832814,
        16778024, 693502208, 275407222, 1630497889,
        1852256083, 1953655150, 1728118791, 1937077104,
        1701733377, 1452108, 1784772193, 794916724,
        793211489, 1885890915, 1933257046, 16778024,
        692650240, 24314112, 52963652, 16781930,
        1635148079, 1818324583, 793933166, 1851875948,
        1694564359, 2036294511, 1869767681, 92417,
        615538, 1635205992, 1634890497, 88577,
        1206881, 1986080609, 2004102979, 1869443183,
        1852141172, 16777545, 16781390, 1702000239,
        1970492517, 2020879978, 1635148033, 289134,
        1769210112, 125329251, 1869574756, 16779122,
        1701863785, 1853096192, 91451493, 1701052672,
        140993908, 1130914162, 1929445381, 1936483685,
        1879113746, 1784772193, 794914928, 1818588207,
        1097887852, 1702101248, 71528292, 1694564375,
        676096609, 1986080620, 1634625327, 1400140393,
        1852259145, 1227445761, 937071, 1667329110,
        1634888033, 1651271027, 16778866, 1702062458,
        1694564362, 1416195429, 1934782317, 1634599168,
        91251049, 1853096192, 510288246, 1630497889,
        1852256073, 1853121906, 1920299124, 1701070200,
        1667592308, 1768910337, 1199210, 1635148079,
        1818324583, 794060914, 1700881467, 16779123,
        1702119023, 1853096192, 153639747, 1229539657,
        693502208, 74737016, 1946222604, 1734702160,
        1634885997, 1702126962, 16778536, 1229531478,
        16778355, 1953460225, 408681, 1852404798,
        16781674, 1635148079, 1635218479, 1198678384,
        1751737203, 16781418, 1635148079, 1818324583,
        794060914, 1700881409, 420965, 1852273768,
        65542, 327681, 589834, 76,
        4915200, 99, 4849664, 82,
        8060928, 103, 7012352, 110,
        7012352, 102, 7012352, 91,
        4849664, 112, 7012352, 85,
        7012352, 87, 6881280, 458753,
        7143518, 65652, 134, 393217,
        82, 705757334, 271758848, 606780160,
        223941123, 68166839, 1881600, 455748114,
        28704790, -1258284502, -1275061561, 600594,
        45416474, 707441664, 448135182, -1140476672,
        338342912, 436415156, 1750528, 237679616,
        335787520, 363921408, 65604, 34,
        524288, 1638409, 1703961, 1769507,
        1835050, 1900592, 2097213, 2162769,
        1572865, 5046366, 65652, 63,
        262145, 27, 716439586, -956295638,
        -1157625767, 716636191, -1258282454, -1275059530,
        1552640, 256, 1140850688, 301990912,
        9472, 117450496, 318777344, 436216832,
        16810240, 1577058560, 1946157056, 754975232,
        16777216, 220902400, 582352920, 704754944,
        582025216, 65604, 14, 196608,
        2949127, 3014668, 2883585, 6094942,
        65652, 82, 131073, 30,
        -1493167852, 3520512, 430374916, 1462416896,
        288011264, 583532526, 704754944, 582025217,
        196617, 786443, 65604, 26,
        393216, 3276803, 3342349, 3407889,
        3276824, 3538973, 3211265, 7929953,
        65652, 135, 393218, 91,
        704886016, 514261063, 716701728, 335561067,
        269429428, 1992839, 1670296832, 304789504,
        538181697, 1796472893, 1670296832, 590031540,
        1321652, 1967146, -1275063766, -1275059274,
        2435673, -1275060732, 1622474782, 716439582,
        716439578, -1241510239, -5132032, 256,
        1140850688, 436209152, 14848, 134233088,
        486554880, 754990592, 1107311104, 1509964032,
        16800768, 1442840832, 1946157056, 1442841088,
        16777216, 707441664, 261685261, 716439586,
        -1241506393, 666292, 2274816, 321530548,
        1022208, 117679872, 67417344, 251964416,
        256, 1140850688, 436209152, 17408,
        117458176, 234898432, 285231104, 402672128,
        671107840, 16810496, 1577058560, 1946157056,
        872415744, 16777216, 271234816, 271192099,
        -1258282710, 62193679, -1325400064, 16794624,
        4608, 67108864, 201327616, 335546880,
        369102592, 201326848, 1392508928, 33582080
        };

}
