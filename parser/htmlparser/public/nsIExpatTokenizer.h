/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


/**
 * MODULE NOTES:
 * @update  nra 2/24/99
 * 
 */

#ifndef __NSIEXPATTOKENIZER__
#define __NSIEXPATTOKENIZER__

#include "nsISupports.h"
#include "prtypes.h"
#include "xmlparse.h"

class CToken;
class nsScanner;

#define NS_IEXPATTOKENIZER_IID      \
  {0xf86a4380, 0xce17, 0x11d2, {0x80, 0x3f, 0x00, 0x60, 0x08, 0x98, 0x28, 0x77}}

class nsIExpatTokenizer : public nsITokenizer {
public:
  /* Methods for setting callbacks on the expat parser */
  virtual void SetElementHandler(XML_StartElementHandler start, XML_EndElementHandler end)=0;
  virtual void SetCharacterDataHandler(XML_CharacterDataHandler handler)=0;
  virtual void SetProcessingInstructionHandler(XML_ProcessingInstructionHandler handler)=0;
  virtual void SetDefaultHandler(XML_DefaultHandler handler)=0;
  virtual void SetUnparsedEntityDeclHandler(XML_UnparsedEntityDeclHandler handler)=0;
  virtual void SetNotationDeclHandler(XML_NotationDeclHandler handler)=0;
  virtual void SetExternalEntityRefHandler(XML_ExternalEntityRefHandler handler)=0;
  virtual void SetUnknownEncodingHandler(XML_UnknownEncodingHandler handler, void *encodingHandlerData)=0;


  virtual void FrontloadMisplacedContent(nsDeque& aDeque)=0;  
};


#endif

