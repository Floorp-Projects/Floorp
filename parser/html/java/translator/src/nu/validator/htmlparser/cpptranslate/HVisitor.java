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

import java.util.LinkedList;
import java.util.List;

import japa.parser.ast.body.FieldDeclaration;
import japa.parser.ast.body.MethodDeclaration;
import japa.parser.ast.body.ModifierSet;
import japa.parser.ast.body.VariableDeclarator;
import japa.parser.ast.expr.IntegerLiteralExpr;
import japa.parser.ast.expr.MethodCallExpr;
import japa.parser.ast.stmt.BlockStmt;
import japa.parser.ast.type.PrimitiveType;
import japa.parser.ast.type.ReferenceType;
import japa.parser.ast.type.Type;

public class HVisitor extends CppVisitor {

    private enum Visibility {
        NONE, PRIVATE, PUBLIC, PROTECTED,
    }

    private Visibility previousVisibility = Visibility.NONE;

    private List<String> defines = new LinkedList<String>();

    private SourcePrinter arrayInitPrinter = new SourcePrinter();
    private SourcePrinter mainPrinterHolder;

    /**
     * @see nu.validator.htmlparser.cpptranslate.CppVisitor#printMethodNamespace()
     */
    @Override protected void printMethodNamespace() {
    }

    public HVisitor(CppTypes cppTypes, SymbolTable symbolTable) {
        super(cppTypes, symbolTable);
    }

    /**
     * @see nu.validator.htmlparser.cpptranslate.CppVisitor#startClassDeclaration()
     */
    @Override protected void startClassDeclaration() {
        printer.print("#ifndef ");        
        printer.print(className);
        printer.printLn("_h__");
        printer.print("#define ");        
        printer.print(className);
        printer.printLn("_h__");
        
        printer.printLn();
        
        String[] incs = cppTypes.boilerplateIncludes(javaClassName);
        for (int i = 0; i < incs.length; i++) {
            String inc = incs[i];
            if (className.equals(inc)) {
                continue;
            }
            printer.print("#include \"");
            printer.print(inc);
            printer.printLn(".h\"");
        }

        printer.printLn();

        String[] forwDecls = cppTypes.boilerplateForwardDeclarations();
        for (int i = 0; i < forwDecls.length; i++) {
            String decl = forwDecls[i];
            printer.print("class ");
            printer.print(decl);
            printer.printLn(";");            
        }
        
        printer.printLn();

        for (int i = 0; i < Main.H_LIST.length; i++) {
            String klazz = Main.H_LIST[i];
            if (!(klazz.equals(javaClassName) || klazz.equals("StackNode"))) {
                printer.print("class ");
                printer.print(cppTypes.classPrefix());
                printer.print(klazz);
                printer.printLn(";");
            }
        }
        
        printer.printLn();
        
        String[] otherDecls = cppTypes.boilerplateDeclarations(javaClassName);
        for (int i = 0; i < otherDecls.length; i++) {
            String decl = otherDecls[i];
            printer.printLn(decl);
        }
        
        printer.printLn();

        printer.print("class ");
        printer.printLn(className);
        printer.printLn("{");
        printer.indent();
        printer.indent();
    }

    /**
     * @see nu.validator.htmlparser.cpptranslate.CppVisitor#endClassDeclaration()
     */
    @Override protected void endClassDeclaration() {
        printModifiers(ModifierSet.PUBLIC | ModifierSet.STATIC);
        printer.printLn("void initializeStatics();");        
        printModifiers(ModifierSet.PUBLIC | ModifierSet.STATIC);
        printer.printLn("void releaseStatics();");        
        
        printer.unindent();
        printer.unindent();
        
        if (cppTypes.hasSupplement(javaClassName)) {
            printer.printLn();
            printer.print("#include \"");
            printer.print(className);
            printer.printLn("HSupplement.h\"");
        }
        
        printer.printLn("};");
        printer.printLn();

        // This stuff should probably go into the .cpp anyway. sigh.
        printer.print("#ifdef ");
        printer.print(className);
        printer.printLn("_cpp__");
        printer.print(arrayInitPrinter.getSource());
        printer.printLn("#endif");
        printer.printLn();
        
        for (String define : defines) {
            printer.printLn(define);
        }
        
        printer.printLn();
        printer.printLn();
        printer.printLn("#endif");
    }

    /**
     * @see nu.validator.htmlparser.cpptranslate.CppVisitor#printModifiers(int)
     */
    @Override protected void printModifiers(int modifiers) {
        if (ModifierSet.isPrivate(modifiers)) {
            if (previousVisibility != Visibility.PRIVATE) {
                printer.unindent();
                printer.printLn("private:");
                printer.indent();
                previousVisibility = Visibility.PRIVATE;
            }
        } else if (ModifierSet.isProtected(modifiers)) {
            if (previousVisibility != Visibility.PROTECTED) {
                printer.unindent();
                printer.printLn("protected:");
                printer.indent();
                previousVisibility = Visibility.PROTECTED;
            }
        } else {
            if (previousVisibility != Visibility.PUBLIC) {
                printer.unindent();
                printer.printLn("public:");
                printer.indent();
                previousVisibility = Visibility.PUBLIC;
            }
        }
        if (inline()) {
            printer.print("inline ");            
        }
        if (virtual()) {
            printer.print("virtual ");            
        }
        if (ModifierSet.isStatic(modifiers)) {
            printer.print("static ");
        }
    }

    /**
     * @see nu.validator.htmlparser.cpptranslate.CppVisitor#fieldDeclaration(japa.parser.ast.body.FieldDeclaration, java.lang.LocalSymbolTable)
     */
    @Override protected void fieldDeclaration(FieldDeclaration n, LocalSymbolTable arg) {
        int modifiers = n.getModifiers();
        List<VariableDeclarator> variables = n.getVariables();
        VariableDeclarator declarator = variables.get(0);
        if (ModifierSet.isStatic(modifiers) && ModifierSet.isFinal(modifiers)
                && n.getType() instanceof PrimitiveType) {
            PrimitiveType type = (PrimitiveType) n.getType();
            if (type.getType() != PrimitiveType.Primitive.Int) {
                throw new IllegalStateException(
                        "Only int constant #defines supported.");
            }
            if (variables.size() != 1) {
                throw new IllegalStateException(
                        "More than one variable declared by one declarator.");
            }
            String name = javaClassName + "." + declarator.getId().getName();
            String value = declarator.getInit().toString();
            if ("Integer.MAX_VALUE".equals(value)) {
                value = cppTypes.maxInteger();
            }
            String longName = definePrefix + declarator.getId().getName();
            if (symbolTable.cppDefinesByJavaNames.containsKey(name)) {
                throw new IllegalStateException(
                        "Duplicate #define constant local name: " + name);
            }
            symbolTable.cppDefinesByJavaNames.put(name, longName);
            defines.add("#define " + longName + " " + value);
        } else {
            if (n.getType() instanceof ReferenceType) {
                ReferenceType rt = (ReferenceType) n.getType();
                currentArrayCount = rt.getArrayCount();
                if (currentArrayCount > 0
                        && (rt.getType() instanceof PrimitiveType) && declarator.getInit() != null) {
                    if (!ModifierSet.isStatic(modifiers)) {
                        throw new IllegalStateException(
                                "Non-static array case not supported here." + declarator);
                    }
                    if (noLength()) {
                        inPrimitiveNoLengthFieldDeclarator = true;
                        
                        mainPrinterHolder = printer;
                        printer = arrayInitPrinter;
                        n.getType().accept(this, arg);
                        printer.print(" ");
                        printer.print(className);
                        printer.print("::");
                        declarator.getId().accept(this, arg);

                        printer.print(" = ");                    
                        
                        declarator.getInit().accept(this, arg);
                       
                        printer.printLn(";");                    
                        printer = mainPrinterHolder;                        
                    } else if (!isNonToCharArrayMethodCall(declarator.getInit())) {
                        mainPrinterHolder = printer;
                        printer = arrayInitPrinter;
                        
                        printer.print(cppTypes.arrayTemplate());
                        printer.print("<");
                        suppressPointer = true;
                        rt.getType().accept(this, arg);
                        suppressPointer = false;
                        printer.print(",");
                        printer.print(cppTypes.intType());
                        printer.print("> ");
                        printer.print(className);
                        printer.print("::");
                        declarator.getId().accept(this, arg);
                        printer.printLn(" = 0;");                    
                        
                        printer = mainPrinterHolder;    
                                            }
                } else if (ModifierSet.isStatic(modifiers)) {
                    mainPrinterHolder = printer;
                    printer = arrayInitPrinter;

                    n.getType().accept(this, arg);
                    printer.print(" ");
                    printer.print(className);
                    printer.print("::");
                    if ("AttributeName".equals(n.getType().toString())) {
                        printer.print("ATTR_");
                    } else if ("ElementName".equals(n.getType().toString())) {
                        printer.print("ELT_");
                    }
                    declarator.getId().accept(this, arg);
                    printer.print(" = ");
                    printer.print(cppTypes.nullLiteral());
                    printer.printLn(";");
                    
                    printer = mainPrinterHolder;    
                }
            }
            printModifiers(modifiers);
            n.getType().accept(this, arg);
            printer.print(" ");
            if (ModifierSet.isStatic(modifiers)) {
                if ("AttributeName".equals(n.getType().toString())) {
                    printer.print("ATTR_");
                } else if ("ElementName".equals(n.getType().toString())) {
                    printer.print("ELT_");
                }
            }
            declarator.getId().accept(this, arg);
            printer.printLn(";");
            currentArrayCount = 0;
            inPrimitiveNoLengthFieldDeclarator = false;
        }
    }

    /**
     * @see nu.validator.htmlparser.cpptranslate.CppVisitor#printConstructorBody(japa.parser.ast.stmt.BlockStmt, java.lang.LocalSymbolTable)
     */
    @Override protected void printConstructorBody(BlockStmt block, LocalSymbolTable arg) {
        printer.printLn(";");
    }

    /**
     * @see nu.validator.htmlparser.cpptranslate.CppVisitor#visit(japa.parser.ast.body.MethodDeclaration, java.lang.LocalSymbolTable)
     */
    @Override public void visit(MethodDeclaration n, LocalSymbolTable arg) {
        arg = new LocalSymbolTable(javaClassName, symbolTable);
        printMethodDeclaration(n, arg);
    }

    /**
     * @see nu.validator.htmlparser.cpptranslate.CppVisitor#inHeader()
     */
    @Override protected boolean inHeader() {
        return true;
    }

}
