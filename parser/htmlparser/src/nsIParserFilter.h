/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

/**
 * MODULE NOTES:
 * @update  jevering 6/17/98
 * 
 */

#ifndef  IPARSERFILTER
#define  IPARSERFILTER

#include "nsISupports.h"

class CToken;

#define NS_IPARSERFILTER_IID     \
  {0x14d6ff0,  0x0610,  0x11d2,  \
  {0x8c, 0x3f, 0x00,    0x80, 0x5f, 0x8a, 0x1d, 0xb7}}


class nsIParserFilter : public nsISupports {
  public:
      
   NS_IMETHOD RawBuffer(char * buffer, int * buffer_length) = 0;

   NS_IMETHOD WillAddToken(CToken & token) = 0;

   NS_IMETHOD ProcessTokens( /* dont know what goes here yet */ void ) = 0;
};

extern nsresult NS_NewParserFilter(nsIParserFilter** aInstancePtrResult);


#endif

