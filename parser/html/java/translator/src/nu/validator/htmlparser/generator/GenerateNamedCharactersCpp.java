/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is HTML Parser C++ Translator code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henri Sivonen <hsivonen@iki.fi>
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

package nu.validator.htmlparser.generator;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.util.Map;
import java.util.TreeMap;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import nu.validator.htmlparser.cpptranslate.CppTypes;

public class GenerateNamedCharactersCpp {

    private static final int LEAD_OFFSET = 0xD800 - (0x10000 >> 10);

    private static final Pattern LINE_PATTERN = Pattern.compile("^\\s*<tr> <td> <code title=\"\">([^<]*)</code> </td> <td> U\\+(\\S*) </td> </tr>.*$");

    private static String toHexString(int c) {
        String hexString = Integer.toHexString(c);
        switch (hexString.length()) {
            case 1:
                return "0x000" + hexString;
            case 2:
                return "0x00" + hexString;
            case 3:
                return "0x0" + hexString;
            case 4:
                return "0x" + hexString;
            default:
                throw new RuntimeException("Unreachable.");
        }
    }

    /**
     * @param args
     * @throws IOException
     */
    public static void main(String[] args) throws IOException {
        TreeMap<String, String> entities = new TreeMap<String, String>();
        BufferedReader reader = new BufferedReader(new InputStreamReader(
                new FileInputStream(args[0]), "utf-8"));
        String line;
        while ((line = reader.readLine()) != null) {
            Matcher m = LINE_PATTERN.matcher(line);
            if (m.matches()) {
                entities.put(m.group(1), m.group(2));
            }
        }

        CppTypes cppTypes = new CppTypes(null);
        File targetDirectory = new File(args[1]);

        generateH(targetDirectory, cppTypes, entities);
        generateCpp(targetDirectory, cppTypes, entities);
    }

    private static void generateH(File targetDirectory, CppTypes cppTypes,
            Map<String, String> entities) throws IOException {
        File hFile = new File(targetDirectory, cppTypes.classPrefix()
                + "NamedCharacters.h");
        Writer out = new OutputStreamWriter(new FileOutputStream(hFile),
                "utf-8");
        out.write("#ifndef " + cppTypes.classPrefix() + "NamedCharacters_h__\n");
        out.write("#define " + cppTypes.classPrefix() + "NamedCharacters_h__\n");
        out.write('\n');

        String[] includes = cppTypes.namedCharactersIncludes();
        for (int i = 0; i < includes.length; i++) {
            String include = includes[i];
            out.write("#include \"" + include + ".h\"\n");
        }

        out.write('\n');

        out.write("class " + cppTypes.classPrefix() + "NamedCharacters\n");
        out.write("{\n");
        out.write("  public:\n");
        out.write("    static " + cppTypes.arrayTemplate() + "<"
                + cppTypes.arrayTemplate() + "<" + cppTypes.charType() + ","
                + cppTypes.intType() + ">," + cppTypes.intType() + "> NAMES;\n");
        out.write("    static " + cppTypes.arrayTemplate() + "<"
                + cppTypes.charType() + "," + cppTypes.intType()
                + ">* VALUES;\n");
        out.write("    static " + cppTypes.charType() + "** WINDOWS_1252;\n");
        out.write("    static void initializeStatics();\n");
        out.write("    static void releaseStatics();\n");
        out.write("};\n");

        out.write("\n#endif // " + cppTypes.classPrefix()
                + "NamedCharacters_h__\n");
        out.flush();
        out.close();
    }

    private static void generateCpp(File targetDirectory, CppTypes cppTypes,
            Map<String, String> entities) throws IOException {
        File hFile = new File(targetDirectory, cppTypes.classPrefix()
                + "NamedCharacters.cpp");
        Writer out = new OutputStreamWriter(new FileOutputStream(hFile),
                "utf-8");

        out.write("#define " + cppTypes.classPrefix()
                + "NamedCharacters_cpp__\n");

        String[] includes = cppTypes.namedCharactersIncludes();
        for (int i = 0; i < includes.length; i++) {
            String include = includes[i];
            out.write("#include \"" + include + ".h\"\n");
        }

        out.write('\n');
        out.write("#include \"" + cppTypes.classPrefix()
                + "NamedCharacters.h\"\n");
        out.write("\n");

        out.write("" + cppTypes.arrayTemplate() + "<"
                + cppTypes.arrayTemplate() + "<" + cppTypes.charType() + ","
                + cppTypes.intType() + ">," + cppTypes.intType() + "> "
                + cppTypes.classPrefix() + "NamedCharacters::NAMES;\n");

        out.write("static " + cppTypes.charType() + " const WINDOWS_1252_DATA[] = {\n");
        out.write("  0x20AC,\n");
        out.write("  0xFFFD,\n");
        out.write("  0x201A,\n");
        out.write("  0x0192,\n");
        out.write("  0x201E,\n");
        out.write("  0x2026,\n");
        out.write("  0x2020,\n");
        out.write("  0x2021,\n");
        out.write("  0x02C6,\n");
        out.write("  0x2030,\n");
        out.write("  0x0160,\n");
        out.write("  0x2039,\n");
        out.write("  0x0152,\n");
        out.write("  0xFFFD,\n");
        out.write("  0x017D,\n");
        out.write("  0xFFFD,\n");
        out.write("  0xFFFD,\n");
        out.write("  0x2018,\n");
        out.write("  0x2019,\n");
        out.write("  0x201C,\n");
        out.write("  0x201D,\n");
        out.write("  0x2022,\n");
        out.write("  0x2013,\n");
        out.write("  0x2014,\n");
        out.write("  0x02DC,\n");
        out.write("  0x2122,\n");
        out.write("  0x0161,\n");
        out.write("  0x203A,\n");
        out.write("  0x0153,\n");
        out.write("  0xFFFD,\n");
        out.write("  0x017E,\n");
        out.write("  0x0178\n");
        out.write("};\n");

        int k = 0;
        for (Map.Entry<String, String> entity : entities.entrySet()) {
            String name = entity.getKey();
            int value = Integer.parseInt(entity.getValue(), 16);

            out.write("static " + cppTypes.charType() + " const NAME_" + k
                    + "[] = {\n");
            out.write("  ");

            for (int j = 0; j < name.length(); j++) {
                char c = name.charAt(j);
                if (j != 0) {
                    out.write(", ");
                }
                out.write('\'');
                out.write(c);
                out.write('\'');
            }

            out.write("\n};\n");

            out.write("static " + cppTypes.charType() + " const VALUE_" + k
                    + "[] = {\n");
            out.write("  ");

            if (value <= 0xFFFF) {
                out.write(toHexString(value));
            } else {
                int hi = (LEAD_OFFSET + (value >> 10));
                int lo = (0xDC00 + (value & 0x3FF));
                out.write(toHexString(hi));
                out.write(", ");
                out.write(toHexString(lo));
            }

            out.write("\n};\n");

            k++;
        }

        out.write("\n// XXX bug 501082: for some reason, msvc takes forever to optimize this function\n");
        out.write("#ifdef _MSC_VER\n");
        out.write("#pragma optimize(\"\", off)\n");
        out.write("#endif\n\n");
        
        out.write("void\n");
        out.write(cppTypes.classPrefix()
                + "NamedCharacters::initializeStatics()\n");
        out.write("{\n");
        out.write("  NAMES = " + cppTypes.arrayTemplate() + "<"
                + cppTypes.arrayTemplate() + "<" + cppTypes.charType() + ","
                + cppTypes.intType() + ">," + cppTypes.intType() + ">("
                + entities.size() + ");\n");
        int i = 0;
        for (Map.Entry<String, String> entity : entities.entrySet()) {
            out.write("  NAMES[" + i + "] = " + cppTypes.arrayTemplate() + "<"
                    + cppTypes.charType() + "," + cppTypes.intType() + ">(("
                    + cppTypes.charType() + "*)NAME_" + i + ", "
                    + entity.getKey().length() + ");\n");
            i++;
        }
        out.write("  VALUES = new " + cppTypes.arrayTemplate() + "<"
                + cppTypes.charType() + "," + cppTypes.intType() + ">["
                + entities.size() + "];\n");
        i = 0;
        for (Map.Entry<String, String> entity : entities.entrySet()) {
            int value = Integer.parseInt(entity.getValue(), 16);
            out.write("  VALUES[" + i + "] = " + cppTypes.arrayTemplate() + "<"
                    + cppTypes.charType() + "," + cppTypes.intType() + ">(("
                    + cppTypes.charType() + "*)VALUE_" + i + ", "
                    + ((value <= 0xFFFF) ? "1" : "2") + ");\n");
            i++;
        }
        out.write("\n");
        out.write("  WINDOWS_1252 = new " + cppTypes.charType() + "*[32];\n");
        out.write("  for (" + cppTypes.intType() + " i = 0; i < 32; ++i) {\n");
        out.write("    WINDOWS_1252[i] = (" + cppTypes.charType() + "*)&(WINDOWS_1252_DATA[i]);\n");
        out.write("  }\n");
        out.write("}\n");
        out.write("\n");

        out.write("#ifdef _MSC_VER\n");
        out.write("#pragma optimize(\"\", on)\n");
        out.write("#endif\n\n");
        
        out.write("void\n");
        out.write(cppTypes.classPrefix()
                + "NamedCharacters::releaseStatics()\n");
        out.write("{\n");
        out.write("  NAMES.release();\n");
        out.write("  delete[] VALUES;\n");
        out.write("  delete[] WINDOWS_1252;\n");
        out.write("}\n");
        out.flush();
        out.close();
    }
}
