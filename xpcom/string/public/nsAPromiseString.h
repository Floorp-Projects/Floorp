/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Scott Collins <scc@mozilla.org> (original author)
 */

/* nsAPromiseString.h --- abstract base class for strings don't actually own their own characters, but proxy data from other strings  */

#ifndef nsAPromiseString_h___
#define nsAPromiseString_h___

  /**
   * Don't |#include| this file yourself.  You will get it automatically if you need it.
   *
   * Why is it a separate file?  To make it easier to find the classes in your local tree.
   */

class NS_COM nsAPromiseString : public nsAString { };
class NS_COM nsAPromiseCString : public nsACString { };

#endif /* !defined(nsAPromiseString_h___) */
