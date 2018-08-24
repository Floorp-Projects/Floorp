/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl.ast

enum class AstType {
    LITERAL,
    OBJECT_LITERAL,
    BINARY_EXPRESSION,
    IDENTIFIER,
    UNARY_EXPRESSION,
    ARRAY_LITERAL,
    TRANSFORM,
    FILTER_EXPRESSION,
    CONDITIONAL_EXPRESSION
}
