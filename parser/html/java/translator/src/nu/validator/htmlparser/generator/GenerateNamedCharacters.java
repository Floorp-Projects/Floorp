/*
 * Copyright (c) 2008 Mozilla Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

package nu.validator.htmlparser.generator;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.Map;
import java.util.TreeMap;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class GenerateNamedCharacters {

    private static final int LEAD_OFFSET = 0xD800 - (0x10000 >> 10);
    
    private static final Pattern LINE_PATTERN = Pattern.compile("^\\s*<tr> <td> <code title=\"\">([^<]*)</code> </td> <td> U\\+(\\S*) </td> </tr>.*$");
    
    private static String toUString(int c) {
        String hexString = Integer.toHexString(c);
        switch (hexString.length()) {
            case 1:
                return "\\u000" + hexString;
            case 2:
                return "\\u00" + hexString;
            case 3:
                return "\\u0" + hexString;
            case 4:
                return "\\u" + hexString;
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
        BufferedReader reader = new BufferedReader(new InputStreamReader(System.in, "utf-8"));
        String line;
        while ((line = reader.readLine()) != null) {
            Matcher m = LINE_PATTERN.matcher(line);
            if (m.matches()) {
                entities.put(m.group(1), m.group(2));
            }
        }
        System.out.println("static final char[][] NAMES = {");
        for (Map.Entry<String, String> entity : entities.entrySet()) {
            String name = entity.getKey();
            System.out.print("\"");
            System.out.print(name);
            System.out.println("\".toCharArray(),");
        }
        System.out.println("};");

        System.out.println("static final @NoLength char[][] VALUES = {");
        for (Map.Entry<String, String> entity : entities.entrySet()) {
            String value = entity.getValue();
            int intVal = Integer.parseInt(value, 16);
            System.out.print("{");
            if (intVal == '\'') {
                System.out.print("\'\\\'\'");                
            } else if (intVal == '\n') {
                System.out.print("\'\\n\'");                
            } else if (intVal == '\\') {
                System.out.print("\'\\\\\'");                
            } else if (intVal <= 0xFFFF) {
                System.out.print("\'");                
                System.out.print(toUString(intVal));                                
                System.out.print("\'");                
            } else {
                int hi = (LEAD_OFFSET + (intVal >> 10));
                int lo = (0xDC00 + (intVal & 0x3FF));
                System.out.print("\'");                
                System.out.print(toUString(hi));                                
                System.out.print("\', \'");                
                System.out.print(toUString(lo));                                
                System.out.print("\'");                
            }
            System.out.println("},");
        }
        System.out.println("};");

    }

}
