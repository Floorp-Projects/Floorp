# -*- coding: utf-8 -*-
# Copyright JS Foundation and other contributors, https://js.foundation/
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import unicode_literals


# Error messages should be identical to V8.
class Messages:
    ObjectPatternAsRestParameter = "Unexpected token {"
    BadImportCallArity = "Unexpected token"
    BadGetterArity = "Getter must not have any formal parameters"
    BadSetterArity = "Setter must have exactly one formal parameter"
    BadSetterRestParameter = "Setter function argument must not be a rest parameter"
    ConstructorIsAsync = "Class constructor may not be an async method"
    ConstructorSpecialMethod = "Class constructor may not be an accessor"
    DeclarationMissingInitializer = "Missing initializer in %0 declaration"
    DefaultRestParameter = "Unexpected token ="
    DefaultRestProperty = "Unexpected token ="
    DuplicateBinding = "Duplicate binding %0"
    DuplicateConstructor = "A class may only have one constructor"
    DuplicateProtoProperty = "Duplicate __proto__ fields are not allowed in object literals"
    ForInOfLoopInitializer = "%0 loop variable declaration may not have an initializer"
    GeneratorInLegacyContext = "Generator declarations are not allowed in legacy contexts"
    IllegalBreak = "Illegal break statement"
    IllegalContinue = "Illegal continue statement"
    IllegalExportDeclaration = "Unexpected token"
    IllegalImportDeclaration = "Unexpected token"
    IllegalLanguageModeDirective = "Illegal 'use strict' directive in function with non-simple parameter list"
    IllegalReturn = "Illegal return statement"
    InvalidEscapedReservedWord = "Keyword must not contain escaped characters"
    InvalidHexEscapeSequence = "Invalid hexadecimal escape sequence"
    InvalidLHSInAssignment = "Invalid left-hand side in assignment"
    InvalidLHSInForIn = "Invalid left-hand side in for-in"
    InvalidLHSInForLoop = "Invalid left-hand side in for-loop"
    InvalidModuleSpecifier = "Unexpected token"
    InvalidRegExp = "Invalid regular expression"
    LetInLexicalBinding = "let is disallowed as a lexically bound name"
    MissingFromClause = "Unexpected token"
    MultipleDefaultsInSwitch = "More than one default clause in switch statement"
    NewlineAfterThrow = "Illegal newline after throw"
    NoAsAfterImportNamespace = "Unexpected token"
    NoCatchOrFinally = "Missing catch or finally after try"
    ParameterAfterRestParameter = "Rest parameter must be last formal parameter"
    PropertyAfterRestProperty = "Unexpected token"
    Redeclaration = "%0 '%1' has already been declared"
    StaticPrototype = "Classes may not have static property named prototype"
    StrictCatchVariable = "Catch variable may not be eval or arguments in strict mode"
    StrictDelete = "Delete of an unqualified identifier in strict mode."
    StrictFunction = "In strict mode code, functions can only be declared at top level or inside a block"
    StrictFunctionName = "Function name may not be eval or arguments in strict mode"
    StrictLHSAssignment = "Assignment to eval or arguments is not allowed in strict mode"
    StrictLHSPostfix = "Postfix increment/decrement may not have eval or arguments operand in strict mode"
    StrictLHSPrefix = "Prefix increment/decrement may not have eval or arguments operand in strict mode"
    StrictModeWith = "Strict mode code may not include a with statement"
    StrictOctalLiteral = "Octal literals are not allowed in strict mode."
    StrictParamDupe = "Strict mode function may not have duplicate parameter names"
    StrictParamName = "Parameter name eval or arguments is not allowed in strict mode"
    StrictReservedWord = "Use of future reserved word in strict mode"
    StrictVarName = "Variable name may not be eval or arguments in strict mode"
    TemplateOctalLiteral = "Octal literals are not allowed in template strings."
    UnexpectedEOS = "Unexpected end of input"
    UnexpectedIdentifier = "Unexpected identifier"
    UnexpectedNumber = "Unexpected number"
    UnexpectedReserved = "Unexpected reserved word"
    UnexpectedString = "Unexpected string"
    UnexpectedTemplate = "Unexpected quasi %0"
    UnexpectedToken = "Unexpected token %0"
    UnexpectedTokenIllegal = "Unexpected token ILLEGAL"
    UnknownLabel = "Undefined label '%0'"
    UnterminatedRegExp = "Invalid regular expression: missing /"
