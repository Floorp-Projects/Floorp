/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsIRDFContentSink_h___
#define nsIRDFContentSink_h___

#include "nsIXMLContentSink.h"

class nsIDocument;
class nsIURL;
class nsIWebShell;

// {751843E2-8309-11d2-8EAC-00805F29F370}
#define NS_IRDFCONTENTSINK_IID \
{ 0x751843e2, 0x8309, 0x11d2, { 0x8e, 0xac, 0x0, 0x80, 0x5f, 0x29, 0xf3, 0x70 } }

/**
 * This interface represents a content sink for generic XML files.
 * The goal of this sink is to deal with XML documents that do not
 * have pre-built semantics, though it may also be implemented for
 * cases in which semantics are hard-wired.
 *
 * The expectation is that the parser has already performed
 * well-formedness and validity checking.
 *
 * XXX The expectation is that entity expansion will be done by the sink
 * itself. This would require, however, that the sink has the ability
 * to query the parser for entity replacement text.
 *
 * XXX This interface does not contain a mechanism for the sink to
 * get specific schema/DTD information from the parser. This information
 * may be necessary for entity expansion. It is also necessary for 
 * building the DOM portions that relate to the schema.
 *
 * XXX This interface does not deal with the presence of an external
 * subset. It seems possible that this could be dealt with completely
 * at the parser level.
 */

class nsIRDFContentSink : public nsIXMLContentSink {
public:
};

extern nsresult NS_NewRDFContentSink(nsIRDFContentSink** aInstancePtrResult,
                                     nsIDocument* aDoc,
                                     nsIURL* aURL,
                                     nsIWebShell* aWebShell);

#endif // nsIRDFContentSink_h___
