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
 * @update  gess 4/8/98
 * 
 *         
 */

#ifndef NS_HTMLDTD__
#define NS_HTMLDTD__

#include "nsIDTD.h"
#include "nsHTMLTokens.h"
#include "nshtmlpars.h"


#define NS_IHTML_DTD_IID      \
  {0x5c5cce40, 0xcfd6,  0x11d1,  \
  {0xaa, 0xda, 0x00,    0x80, 0x5f, 0x8a, 0x3e, 0x14}}



class nsHTMLDTD : public nsIDTD {
            
	public:

    NS_DECL_ISUPPORTS


    /** -------------------------------------------------------
     *  
     *  
     *  @update  gess 4/9/98
     *  @param   
     *  @return  
     */ //------------------------------------------------------
						nsHTMLDTD();

    /** -------------------------------------------------------
     *  
     *  
     *  @update  gess 4/9/98
     *  @param   
     *  @return  
     */ //------------------------------------------------------
    virtual ~nsHTMLDTD();

    /** ------------------------------------------------------
     *  This method is called to determine whether or not a tag
     *  of one type can contain a tag of another type.
     *  
     *  @update  gess 3/25/98
     *  @param   aParent -- tag enum of parent container
     *  @param   aChild -- tag enum of child container
     *  @return  PR_TRUE if parent can contain child
     */ //----------------------------------------------------
    virtual PRBool CanContain(PRInt32 aParent,PRInt32 aChild) const;

    /** ------------------------------------------------------
     *  This method is called to determine whether or not a tag
     *  of one type can contain a tag of another type.
     *  
     *  @update  gess 3/25/98
     *  @param   aParent -- tag enum of parent container
     *  @param   aChild -- tag enum of child container
     *  @return  PR_TRUE if parent can contain child
     */ //----------------------------------------------------
    virtual PRBool CanContainIndirect(PRInt32 aParent,PRInt32 aChild) const;

    /** -------------------------------------------------------
     *  This method gets called to determine whether a given 
     *  tag can contain newlines. Most do not.
     *  
     *  @update  gess 3/25/98
     *  @param   aTag -- tag to test for containership
     *  @return  PR_TRUE if given tag can contain other tags
     */ //----------------------------------------------------
    virtual PRBool CanDisregard(PRInt32 aParent,PRInt32 aChild)const;

    /** -------------------------------------------------------
     *  This method gets called to determine whether a given 
     *  tag is itself a container
     *  
     *  @update  gess 3/25/98
     *  @param   aTag -- tag to test for containership
     *  @return  PR_TRUE if given tag can contain other tags
     */ //----------------------------------------------------
    virtual PRBool IsContainer(PRInt32 aTags) const;

    /** ------------------------------------------------------
     * This method does two things: 1st, help construct
     * our own internal model of the content-stack; and
     * 2nd, pass this message on to the sink.
     * @update	gess4/6/98
     * @param   aNode -- next node to be added to model
     * @return  TRUE if ok, FALSE if error
     */ //----------------------------------------------------
    virtual PRInt32 GetDefaultParentTagFor(PRInt32 aTag) const;


    /** ------------------------------------------------------
     * This method gets called at various times by the parser
     * whenever we want to verify a valid context stack. This
     * method also gives us a hook to add debugging metrics.
     *
     * @update	gess4/6/98
     * @param   aStack[] array of ints (tokens)
     * @param   aCount number of elements in given array
     * @return  TRUE if stack is valid, else FALSE
     */ //-----------------------------------------------------
    virtual PRBool VerifyContextStack(eHTMLTags aStack[],PRInt32 aCount) const;

};


#endif 

