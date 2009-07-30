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
 * Portions created by the Initial Developer are Copyright (C) 2009
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

import java.util.HashMap;
import java.util.Map;

public class LocalSymbolTable {

    private final Map<String, Type> locals = new HashMap<String, Type>();

    private final String javaClassName;
    
    private final SymbolTable delegate;    

    /**
     * @param javaClassName
     * @param delegate
     */
    public LocalSymbolTable(String javaClassName, SymbolTable delegate) {
        this.javaClassName = javaClassName;
        this.delegate = delegate;
    }

    public void putLocalType(String name, Type type) {
        locals.put(name, type);
    }

    /**
     * @param klazz
     * @param variable
     * @return
     * @see nu.validator.htmlparser.cpptranslate.SymbolTable#getFieldType(java.lang.String, java.lang.String)
     */
    public Type getVariableType(String klazz, String variable) {
        if (klazz == null) {
            Type type = locals.get(variable);
            if (type != null) {
                return type;
            }
        }
        return delegate.getFieldType(((klazz == null || "this".equals(klazz)) ? javaClassName : klazz), variable);
    }

    /**
     * @param klazz may be <code>null</code> or "this"
     * @param method 
     * @return
     * @see nu.validator.htmlparser.cpptranslate.SymbolTable#getMethodReturnType(java.lang.String, java.lang.String)
     */
    public Type getMethodReturnType(String klazz, String method) {
        return delegate.getMethodReturnType(((klazz == null || "this".equals(klazz)) ? javaClassName : klazz), method);
    }
}
