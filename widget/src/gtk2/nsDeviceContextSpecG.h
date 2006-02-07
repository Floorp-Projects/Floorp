/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 */

#ifndef nsDeviceContextSpecG_h___
#define nsDeviceContextSpecG_h___

#include "nsIDeviceContextSpec.h"
#include "nsDeviceContextSpecG.h"
#include "nsIDeviceContextSpecPS.h"
#ifdef USE_XPRINT
#include "nsIDeviceContextSpecXPrint.h"
#endif /* USE_XPRINT */
#include "nsPrintdGTK.h"

typedef enum
{
  pmAuto = 0, /* default */
  pmXprint,
  pmPostScript
} PrintMethod;

/* make Xprint the default print system if user/admin has set the XPSERVERLIST"
 * env var. See Xprt config README (/usr/openwin/server/etc/XpConfig/README) 
 * for details.
 */
#define NS_DEFAULT_PRINT_METHOD ((PR_GetEnv("XPSERVERLIST")!=nsnull)?(pmXprint):(pmPostScript))

class nsDeviceContextSpecGTK : public nsIDeviceContextSpec ,
                                      public nsIDeviceContextSpecPS
#ifdef USE_XPRINT
                                    , public nsIDeviceContextSpecXp
#endif
{
public:
/**
 * Construct a nsDeviceContextSpecMac, which is an object which contains and manages a mac printrecord
 * @update  dc 12/02/98
 */
  nsDeviceContextSpecGTK();

  NS_DECL_ISUPPORTS

/**
 * Initialize the nsDeviceContextSpecMac for use.  This will allocate a printrecord for use
 * @update   dc 2/16/98
 * @param aQuiet if PR_TRUE, prevent the need for user intervention
 *        in obtaining device context spec. if nsnull is passed in for
 *        the aOldSpec, this will typically result in getting a device
 *        context spec for the default output device (i.e. default
 *        printer).
 * @return error status
 */
  NS_IMETHOD Init(PRBool	aQuiet);
  
  
/**
 * Closes the printmanager if it is open.
 * @update   dc 2/13/98
 * @update   syd 3/20/99
 * @return error status
 */

  NS_IMETHOD ClosePrintManager();

  NS_IMETHOD GetToPrinter( PRBool &aToPrinter ); 

  NS_IMETHOD GetFirstPageFirst ( PRBool &aFpf );     

  NS_IMETHOD GetGrayscale( PRBool &aGrayscale );   

  NS_IMETHOD GetSize ( int &aSize ); 

  NS_IMETHOD GetTopMargin ( float &value ); 

  NS_IMETHOD GetBottomMargin ( float &value ); 

  NS_IMETHOD GetLeftMargin ( float &value ); 

  NS_IMETHOD GetRightMargin ( float &value ); 

  NS_IMETHOD GetCommand ( char **aCommand );   

  NS_IMETHOD GetPath ( char **aPath );    

  NS_IMETHOD GetPageDimensions (float &aWidth, float &aHeight );

  NS_IMETHOD GetUserCancelled( PRBool &aCancel );      

  NS_IMETHOD GetPrintMethod(PrintMethod &aMethod ); 
protected:
/**
 * Destuct a nsDeviceContextSpecMac, this will release the printrecord
 * @update  dc 2/16/98
 */
  virtual ~nsDeviceContextSpecGTK();

protected:

  UnixPrData mPrData;
	
};

#endif
