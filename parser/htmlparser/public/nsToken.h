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
 * @update  gess 4/1/98
 * 
 * This class is defines the basic notion of a token
 * within our system. All other tokens are derived from
 * this one. It offers a few basic interfaces, but the
 * most important is consume(). The consume() method gets
 * called during the tokenization process when an instance
 * of that particular token type gets detected in the 
 * input stream.
 *
 * CToken objects that are allocated from the heap _must_ be allocated
 * using the nsTokenAllocator: the nsTokenAllocator object uses an
 * arena to manage the tokens.
 *
 * The nsTokenAllocator object's arena implementation requires
 * object size at destruction time to properly recycle the object;
 * therefore, CToken::operator delete() is not public. Instead,
 * heap-allocated tokens should be destroyed using the static
 * Destroy() method, which accepts a token and the arena from which
 * the token was allocated.
 *
 * Leaf classes (that are actually instantiated from the heap) must
 * implement the SizeOf() method, which Destroy() uses to determine
 * the size of the token in order to properly recycle it.
 */


#ifndef CTOKEN__
#define CTOKEN__

#include "prtypes.h"
#include "nsString.h"
#include "nsError.h"
#include "nsFileSpec.h"
#include "nsFixedSizeAllocator.h"

#define NS_HTMLTOKENS_NOT_AN_ENTITY \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_HTMLPARSER,2000)

class nsScanner;
class nsTokenAllocator;

enum eContainerInfo {
  eWellFormed,
  eMalformed,
  eFormUnknown
};

/**
 * Implement the SizeOf() method; leaf classes derived from CToken
 * must declare this.
 */
#define CTOKEN_IMPL_SIZEOF                                \
protected:                                                \
  virtual size_t SizeOf() const { return sizeof(*this); } \
public:

/**
 *  Token objects represent sequences of characters as they
 *  are consumed from the input stream (URL). While they're
 *  pretty general in nature, we use subclasses (found in
 *  nsHTMLTokens.h) to define <start>, </end>, <text>,
 *  <comment>, <&entity>, <newline>, and <whitespace> tokens.
 *  
 *  @update  gess 3/25/98
 */
class CToken {
  public:

    enum  eTokenOrigin {eSource,eResidualStyle};

  protected:

    // nsTokenAllocator should be the only class that tries to
    // allocate tokens from the heap.
    friend class nsTokenAllocator;

    /**
     * 
     * @update	harishd 08/01/00
     * @param   aSize    - 
     * @param   aArena   - Allocate memory from this pool.
     */
    static void * operator new (size_t aSize,nsFixedSizeAllocator& anArena)
    {
      return anArena.Alloc(aSize);
    }

    /**
     * Hide operator delete; clients should use Destroy() instead.
     */
    static void operator delete (void*,size_t) {}

  public:
    /**
     * destructor
     * @update	gess5/11/98
     */
    virtual ~CToken();

    /**
     * Destroy a token.
     */
    static void Destroy(CToken* aToken,nsFixedSizeAllocator& aArenaPool)
    {
      size_t sz = aToken->SizeOf();
      aToken->~CToken();
      aArenaPool.Free(aToken, sz);
    }

    /**
     * Make a note on number of times you have been referenced
     * @update	harishd 08/02/00
     */
    void AddRef() { mUseCount++; }
    
    /**
     * Free yourself if no one is holding you.
     * @update	harishd 08/02/00
     */
    void Release(nsFixedSizeAllocator& aArenaPool) {
      if(--mUseCount==0)
        Destroy(this, aArenaPool);
    }

    /**
     * Default constructor
     * @update	gess7/21/98
     */
    CToken(PRInt32 aTag=0);

    /**
     * Retrieve string value of the token
     * @update	gess5/11/98
     * @return  reference to string containing string value
     */
    virtual const nsAReadableString& GetStringValue(void) = 0;

    /**
     * Get string of full contents, suitable for debug dump.
     * It should look exactly like the input source.
     * @update	gess5/11/98
     * @return  reference to string containing string value
     */
    virtual void GetSource(nsString& anOutputString);

    /** @update	harishd 03/23/00
     *  @return  reference to string containing string value
     */
    virtual void AppendSource(nsString& anOutputString);

    /**
     * Sets the ordinal value of this token (not currently used)
     * @update	gess5/11/98
     * @param   value is the new ord value for this token
     */
    virtual void SetTypeID(PRInt32 aValue);
    
    /**
     * Getter which retrieves the current ordinal value for this token
     * @update	gess5/11/98
     * @return  current ordinal value 
     */
    virtual PRInt32 GetTypeID(void);

    /**
     * Sets the # of attributes found for this token.
     * @update	gess5/11/98
     * @param   value is the attr count
     */
    virtual void SetAttributeCount(PRInt16 aValue);

    /**
     * Getter which retrieves the current attribute count for this token
     * @update	gess5/11/98
     * @return  current attribute count 
     */
    virtual PRInt16 GetAttributeCount(void);

    /**
     * Causes token to consume data from given scanner.
     * Note that behavior varies wildly between CToken subclasses.
     * @update	gess5/11/98
     * @param   aChar -- most recent char consumed
     * @param   aScanner -- input source where token should get data
     * @return  error code (0 means ok)
     */
    virtual nsresult Consume(PRUnichar aChar,nsScanner& aScanner,PRInt32 aMode);

#ifdef DEBUG
    /**
     * Causes token to dump itself in debug form to given output stream
     * @update	gess5/11/98
     * @param   out is the output stream where token should write itself
     */
    virtual void DebugDumpToken(nsOutputStream& out);

    /**
     * Causes token to dump itself in source form to given output stream
     * @update	gess5/11/98
     * @param   out is the output stream where token should write itself
     */
    virtual void DebugDumpSource(nsOutputStream& out);
#endif

    /**
     * Getter which retrieves type of token
     * @update	gess5/11/98
     * @return  int containing token type
     */
    virtual PRInt32 GetTokenType(void);

    /**
     * Getter which retrieves the class name for this token 
     * This method is only used for debug purposes.
     * @update	gess5/11/98
     * @return  const char* containing class name
     */
    virtual const char* GetClassName(void);


    /**
     * For tokens who care, this can tell us whether the token is 
     * well formed or not.
     *
     * @update	gess 8/30/00
     * @return  PR_FALSE; subclasses MUST override if they care.
     */
    virtual PRBool IsWellFormed(void) const {return PR_FALSE;}


    /**
     * perform self test.
     * @update	gess5/11/98
     */
    virtual void SelfTest(void);

    static int GetTokenCount();

    eTokenOrigin  mOrigin;
    PRInt32       mNewlineCount;

protected:
    /**
     * Returns the size of the token object.
     */
    virtual size_t SizeOf() const = 0;

    PRInt32				mTypeID;
    PRInt16				mAttrCount;
    PRInt32       mUseCount;
};



#endif


