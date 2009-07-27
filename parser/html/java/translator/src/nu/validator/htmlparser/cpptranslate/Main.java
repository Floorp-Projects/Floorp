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

package nu.validator.htmlparser.cpptranslate;

import japa.parser.JavaParser;
import japa.parser.ParseException;
import japa.parser.ast.CompilationUnit;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.UnsupportedEncodingException;

public class Main {

    static final String[] H_LIST = {
        "Tokenizer",
        "TreeBuilder",
        "MetaScanner",
        "AttributeName",
        "ElementName",
        "HtmlAttributes",
        "StackNode",
        "UTF16Buffer",
        "StateSnapshot",
        "Portability",
    };
    
    private static final String[] CPP_LIST = {
        "Tokenizer",
        "TreeBuilder",
        "MetaScanner",
        "AttributeName",
        "ElementName",
        "HtmlAttributes",
        "StackNode",
        "UTF16Buffer",
        "StateSnapshot",
    };
    
    /**
     * @param args
     * @throws ParseException 
     * @throws IOException 
     */
    public static void main(String[] args) throws ParseException, IOException {
        CppTypes cppTypes = new CppTypes(new File(args[2]));
        SymbolTable symbolTable = new SymbolTable();
        
        File javaDirectory = new File(args[0]);
        File targetDirectory = new File(args[1]);
        File cppDirectory = targetDirectory;
        File javaCopyDirectory = new File(targetDirectory, "javasrc");
        
        for (int i = 0; i < H_LIST.length; i++) {
            parseFile(cppTypes, javaDirectory, cppDirectory, H_LIST[i], ".h", new HVisitor(cppTypes, symbolTable));
//            copyFile(new File(javaDirectory, H_LIST[i] + ".java"), new File(javaCopyDirectory, H_LIST[i] + ".java"));
        }
        for (int i = 0; i < CPP_LIST.length; i++) {
            parseFile(cppTypes, javaDirectory, cppDirectory, CPP_LIST[i], ".cpp", new CppVisitor(cppTypes, symbolTable));
        }
        cppTypes.finished();
    }

    private static void copyFile(File input, File output) throws IOException {
        if (input.getCanonicalFile().equals(output.getCanonicalFile())) {
            return; // files are the same!
        }
        // This is horribly inefficient, but perf is not really much of a concern here.
        FileInputStream in = new FileInputStream(input);
        FileOutputStream out = new FileOutputStream(output);
        int b;
        while ((b = in.read()) != -1) {
            out.write(b);
        }
        out.flush();
        out.close();
        in.close();
    }
    
    private static void parseFile(CppTypes cppTypes, File javaDirectory, File cppDirectory, String className, String fne, CppVisitor visitor) throws ParseException,
            FileNotFoundException, UnsupportedEncodingException, IOException {
        File file = new File(javaDirectory, className + ".java");
        String license = new LicenseExtractor(file).extract();
        CompilationUnit cu = JavaParser.parse(new NoCppInputStream(new FileInputStream(file)), "utf-8");
        LabelVisitor labelVisitor = new LabelVisitor();
        cu.accept(labelVisitor, null);
        visitor.setLabels(labelVisitor.getLabels());
        cu.accept(visitor, null);
        FileOutputStream out = new FileOutputStream(new File(cppDirectory, cppTypes.classPrefix() + className + fne));
        OutputStreamWriter w = new OutputStreamWriter(out, "utf-8");
        w.write(license);
        w.write("\n\n/*\n * THIS IS A GENERATED FILE. PLEASE DO NOT EDIT.\n * Please edit " + className + ".java instead and regenerate.\n */\n\n");
        w.write(visitor.getSource());
        w.close();
    }

}
