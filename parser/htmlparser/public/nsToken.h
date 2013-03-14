/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


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
 * CToken objects that are allocated from the heap are allocated
 * using the nsTokenAllocator object.  nsTokenAllocator used to use
 * an arena-style allocator, but that is no longer necessary or helpful;
 * it now uses a trivial drop-in replacement for the arena-style
 * allocator called nsDummyAllocator, which just wraps malloc/free.
 */


#ifndef CTOKEN__
#define CTOKEN__

#include "prtypes.h"
#include "nsString.h"
#include "nsError.h"

class nsScanner;
class nsTokenAllocator;

/**
 * A trivial allocator.  See the comment at the top of this file.
 */
class nsDummyAllocator {
public:
  void* Alloc(size_t aSize) { return malloc(aSize); }
  void Free(void* aPtr) { free(aPtr); }
};

enum eContainerInfo {
  eWellFormed,
  eMalformed,
  eFormUnknown
};

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
    static void * operator new (size_t aSize, nsDummyAllocator& anArena) CPP_THROW_NEW
    {
      return anArena.Alloc(aSize);
    }

    /**
     * Hide operator delete; clients should use Destroy() instead.
     */
    static void operator delete (void*,size_t) {}

  protected:
    /**
     * destructor
     * @update	gess5/11/98
     */
    virtual ~CToken();

  private:
    /**
     * Destroy a token.
     */
    static void Destroy(CToken* aToken, nsDummyAllocator& aArenaPool)
    {
      aToken->~CToken();
      aArenaPool.Free(aToken);
    }

  public:
    /**
     * Make a note on number of times you have been referenced
     * @update	harishd 08/02/00
     */
    void AddRef() {
      ++mUseCount;
      NS_LOG_ADDREF(this, mUseCount, "CToken", sizeof(*this));
    }
    
    /**
     * Free yourself if no one is holding you.
     * @update	harishd 08/02/00
     */
    void Release(nsDummyAllocator& aArenaPool) {
      --mUseCount;
      NS_LOG_RELEASE(this, mUseCount, "CToken");
      if (mUseCount==0)
        Destroy(this, aArenaPool);
    }

    /**
     * Default constructor
     * @update	gess7/21/98
     */
    CToken(int32_t aTag=0);

    /**
     * Retrieve string value of the token
     * @update	gess5/11/98
     * @return  reference to string containing string value
     */
    virtual const nsSubstring& GetStringValue(void) = 0;

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
    virtual void AppendSourceTo(nsAString& anOutputString);

    /**
     * Sets the ordinal value of this token (not currently used)
     * @update	gess5/11/98
     * @param   value is the new ord value for this token
     */
    void SetTypeID(int32_t aValue) {
      mTypeID = aValue;
    }
    
    /**
     * Getter which retrieves the current ordinal value for this token
     * @update	gess5/11/98
     * @return  current ordinal value 
     */
    virtual int32_t GetTypeID(void);

    /**
     * Getter which retrieves the current attribute count for this token
     * @update	gess5/11/98
     * @return  current attribute count 
     */
    virtual int16_t GetAttributeCount(void);

    /**
     * Causes token to consume data from given scanner.
     * Note that behavior varies wildly between CToken subclasses.
     * @update	gess5/11/98
     * @param   aChar -- most recent char consumed
     * @param   aScanner -- input source where token should get data
     * @return  error code (0 means ok)
     */
    virtual nsresult Consume(PRUnichar aChar,nsScanner& aScanner,int32_t aMode);

    /**
     * Getter which retrieves type of token
     * @update	gess5/11/98
     * @return  int containing token type
     */
    virtual int32_t GetTokenType(void);

    /**
     * For tokens who care, this can tell us whether the token is 
     * well formed or not.
     *
     * @update	gess 8/30/00
     * @return  false; subclasses MUST override if they care.
     */
    virtual bool IsWellFormed(void) const {return false;}

    virtual bool IsEmpty(void) { return false; }
    
    /**
     * If aValue is TRUE then the token represents a short-hand tag
     */
    virtual void SetEmpty(bool aValue) { return ; }

    int32_t GetNewlineCount() 
    { 
      return mNewlineCount; 
    }

    void SetNewlineCount(int32_t aCount)
    {
      mNewlineCount = aCount;
    }

    int32_t GetLineNumber() 
    { 
      return mLineNumber;
    }

    void SetLineNumber(int32_t aLineNumber) 
    { 
      mLineNumber = mLineNumber == 0 ? aLineNumber : mLineNumber;
    }

    void SetInError(bool aInError)
    {
      mInError = aInError;
    }

    bool IsInError()
    {
      return mInError;
    }

    void SetAttributeCount(int16_t aValue) {  mAttrCount = aValue; }

    /**
     * perform self test.
     * @update	gess5/11/98
     */
    virtual void SelfTest(void);

    static int GetTokenCount();

    

protected:
    int32_t mTypeID;
    int32_t mUseCount;
    int32_t mNewlineCount;
    uint32_t mLineNumber : 31;
    uint32_t mInError : 1;
    int16_t mAttrCount;
};



#endif


