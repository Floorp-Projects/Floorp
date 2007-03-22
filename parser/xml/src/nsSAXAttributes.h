/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Robert Sayre.
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsSAXAttributes_h__
#define nsSAXAttributes_h__

#include "nsISupports.h"
#include "nsISAXAttributes.h"
#include "nsISAXMutableAttributes.h"
#include "nsTArray.h"
#include "nsString.h"

#define NS_SAXATTRIBUTES_CONTRACTID "@mozilla.org/saxparser/attributes;1"
#define NS_SAXATTRIBUTES_CLASSNAME "SAX Attributes"
#define NS_SAXATTRIBUTES_CID  \
{/* {7bb40992-77eb-43db-9a4e-39d3bcc483ae}*/ \
0x7bb40992, 0x77eb, 0x43db, \
{ 0x9a, 0x4e, 0x39, 0xd3, 0xbc, 0xc3, 0x83, 0xae} }

struct SAXAttr
{
  nsString uri;
  nsString localName;
  nsString qName;
  nsString type;
  nsString value;
};

class nsSAXAttributes : public nsISAXMutableAttributes
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISAXATTRIBUTES
  NS_DECL_NSISAXMUTABLEATTRIBUTES

private:
  nsTArray<SAXAttr> mAttrs;
};

#endif // nsSAXAttributes_h__
