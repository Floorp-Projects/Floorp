/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef PROPERTYLIST_H
#define PROPERTYLIST_H


// Property is a name,variant pair held by the browser. In IE, properties
// offer a primitive way for DHTML elements to talk back and forth with
// the host app. The Mozilla app currently just implements them for
// compatibility reasons
struct Property
{
  CComBSTR szName;
  CComVariant vValue;
};

// A list of properties
typedef std::vector<Property> PropertyList;


// DEVNOTE: These operators are required since the unpatched VC++ 5.0
//          generates code even for unreferenced template methods in
//          the file <vector>  and will give compiler errors without
//          them. Service Pack 1 and above fixes this problem

int operator <(const Property&, const Property&);
int operator ==(const Property&, const Property&); 


#endif