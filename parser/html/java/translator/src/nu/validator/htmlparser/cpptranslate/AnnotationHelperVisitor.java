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

import java.util.List;

import japa.parser.ast.expr.AnnotationExpr;
import japa.parser.ast.expr.MarkerAnnotationExpr;
import japa.parser.ast.type.ReferenceType;
import japa.parser.ast.visitor.VoidVisitorAdapter;

public class AnnotationHelperVisitor<T> extends VoidVisitorAdapter<T> {

    protected List<AnnotationExpr> currentAnnotations;

    protected boolean nsUri() {
        return hasAnnotation("NsUri");
    }

    protected boolean prefix() {
        return hasAnnotation("Prefix");
    }

    protected boolean local() {
        return hasAnnotation("Local");
    }

    protected boolean literal() {
        return hasAnnotation("Literal");
    }

    protected boolean inline() {
        return hasAnnotation("Inline");
    }

    protected boolean noLength() {
        return hasAnnotation("NoLength");
    }

    protected boolean virtual() {
        return hasAnnotation("Virtual");
    }

    private boolean hasAnnotation(String anno) {
        if (currentAnnotations == null) {
            return false;
        }
        for (AnnotationExpr ann : currentAnnotations) {
            if (ann instanceof MarkerAnnotationExpr) {
                MarkerAnnotationExpr marker = (MarkerAnnotationExpr) ann;
                if (marker.getName().getName().equals(anno)) {
                    return true;
                }
            }
        }
        return false;
    }

    protected Type convertType(japa.parser.ast.type.Type type, int modifiers) {
        if (type instanceof ReferenceType) {
            ReferenceType referenceType = (ReferenceType) type;
            return new Type(convertTypeName(referenceType.getType().toString()), referenceType.getArrayCount(), noLength(), modifiers);
        } else {
            return new Type(convertTypeName(type.toString()), 0, false, modifiers);
        }
    }

    private String convertTypeName(String name) {
        if ("String".equals(name)) {
            if (local()) {
                return "@Local";
            }
            if (nsUri()) {
                return "@NsUri";
            }
            if (prefix()) {
                return "@Prefix";
            }
            if (literal()) {
                return "@Literal";
            }
        }
        return name;
    }

}
