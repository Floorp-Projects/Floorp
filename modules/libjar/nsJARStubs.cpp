/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *     Mitch Stoltz <mstoltz@netscape.com>
 */

/* This file contains stubs for signature verification functions used by nsJAR.
   These functions are declared in nsJAR.h
 */

#include "nsJAR.h"

PRBool nsJAR::SupportsRSAVerification()
{
  return PR_FALSE;
}

nsresult nsJAR::DigestBegin(PRUint32* id, PRInt32 alg)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsJAR::DigestData(PRUint32 id, const char* data, PRUint32 length)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsJAR::CalculateDigest(PRUint32 id, char** digest)
{ 
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsJAR::VerifySignature(const char* sfBuf, const char* rsaBuf, 
                                PRUint32 rsaBufLen, nsIPrincipal** aPrincipal)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
