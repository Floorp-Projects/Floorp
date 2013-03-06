/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 * 
 * This class is defines the basic interface between the
 * parser and the content sink. The parser will iterate
 * over the collection of tokens that it sees from the
 * tokenizer, coverting each related "group" into one of
 * these. This object gets passed to the sink, and is
 * then immediately reused.
 *
 * If you want to hang onto one of these, you should
 * make your own copy.
 *
 */

#ifndef NS_PARSERNODE__
#define NS_PARSERNODE__

#include "nsIParserNode.h"
#include "nsToken.h"
#include "nsString.h"
#include "nsParserCIID.h"
#include "nsDeque.h"
#include "nsDTDUtils.h"

class nsTokenAllocator;

class nsCParserNode :  public nsIParserNode {

  protected:

    int32_t mRefCnt;

  public:

    void AddRef()
    {
      ++mRefCnt;
    }

    void Release(nsDummyAllocator& aPool)
    {
      if (--mRefCnt == 0)
        Destroy(this, aPool);
    }

#ifndef HEAP_ALLOCATED_NODES
  protected:

    /**
     * Hide operator new; clients should use Create() instead.
     */
    static void* operator new(size_t) CPP_THROW_NEW { return 0; }

    /**
     * Hide operator delete; clients should use Destroy() instead.
     */
    static void operator delete(void*,size_t) {}

#endif

  public:
    static nsCParserNode* Create(CToken* aToken,
                                 nsTokenAllocator* aTokenAllocator,
                                 nsNodeAllocator* aNodeAllocator)
    {
#ifdef HEAP_ALLOCATED_NODES
      return new
#else
      nsDummyAllocator& pool = aNodeAllocator->GetArenaPool();
      void* place = pool.Alloc(sizeof(nsCParserNode));
      NS_ENSURE_TRUE(place, nullptr);
      return ::new (place)
#endif
        nsCParserNode(aToken, aTokenAllocator, aNodeAllocator);
    }

    static void Destroy(nsCParserNode* aNode, nsDummyAllocator& aPool)
    {
#ifdef HEAP_ALLOCATED_NODES
      delete aNode;
#else
      aNode->~nsCParserNode();
      aPool.Free(aNode);
#endif
    }

    /**
     * Default constructor
     */
    nsCParserNode();

    /**
     * Constructor
     * @update	gess5/11/98
     * @param   aToken is the token this node "refers" to
     */
    nsCParserNode(CToken* aToken,
                  nsTokenAllocator* aTokenAllocator,
                  nsNodeAllocator* aNodeAllocator=0);

    /**
     * Destructor
     * @update	gess5/11/98
     */
    virtual ~nsCParserNode();

    /**
     * Init
     * @update	gess5/11/98
     */
    virtual nsresult Init(CToken* aToken,
                          nsTokenAllocator* aTokenAllocator,
                          nsNodeAllocator* aNodeAllocator=0);

    /**
     * Retrieve the name of the node
     * @update	gess5/11/98
     * @return  string containing node name
     */
    virtual const nsAString& GetTagName() const;

    /**
     * Retrieve the text from the given node
     * @update	gess5/11/98
     * @return  string containing node text
     */
    virtual const nsAString& GetText() const;

    /**
     * Retrieve the type of the parser node.
     * @update	gess5/11/98
     * @return  node type.
     */
    virtual int32_t GetNodeType()  const;

    /**
     * Retrieve token type of parser node
     * @update	gess5/11/98
     * @return  token type
     */
    virtual int32_t GetTokenType()  const;


    //***************************************
    //methods for accessing key/value pairs
    //***************************************

    /**
     * Retrieve the number of attributes in this node.
     * @update	gess5/11/98
     * @return  count of attributes (may be 0)
     */
    virtual int32_t GetAttributeCount(bool askToken=false) const;

    /**
     * Retrieve the key (of key/value pair) at given index
     * @update	gess5/11/98
     * @param   anIndex is the index of the key you want
     * @return  string containing key.
     */
    virtual const nsAString& GetKeyAt(uint32_t anIndex) const;

    /**
     * Retrieve the value (of key/value pair) at given index
     * @update	gess5/11/98
     * @param   anIndex is the index of the value you want
     * @return  string containing value.
     */
    virtual const nsAString& GetValueAt(uint32_t anIndex) const;

    /**
     * NOTE: When the node is an entity, this will translate the entity
     *       to it's unicode value, and store it in aString.
     * @update	gess5/11/98
     * @param   aString will contain the resulting unicode string value
     * @return  int (unicode char or unicode index from table)
     */
    virtual int32_t TranslateToUnicodeStr(nsString& aString) const;

    /**
     * 
     * @update	gess5/11/98
     * @param 
     * @return
     */
    virtual void AddAttribute(CToken* aToken);

    /**
     * This getter retrieves the line number from the input source where
     * the token occurred. Lines are interpreted as occurring between \n characters.
     * @update	gess7/24/98
     * @return  int containing the line number the token was found on
     */
    virtual int32_t GetSourceLineNumber(void) const;

    /** This method pop the attribute token from the given index
     * @update	harishd 03/25/99
     * @return  token at anIndex
     */
    virtual CToken* PopAttributeToken();

    /** Like PopAttributeToken, but pops off the front of the attribute list */
    virtual CToken* PopAttributeTokenFront();

    /** Retrieve a string containing the tag and its attributes in "source" form
     * @update	rickg 06June2000
     * @return  void
     */
    virtual void GetSource(nsString& aString) const;

    /**
     * This pair of methods allows us to set a generic bit (for arbitrary use)
     * on each node stored in the context.
     * @update	gess 11May2000
     */
    virtual bool    GetGenericState(void) const {return mGenericState;}
    virtual void    SetGenericState(bool aState) {mGenericState=aState;}

    /** Release all the objects you're holding
     * @update	harishd 08/02/00
     * @return  void
     */
    virtual nsresult ReleaseAll();

    bool mGenericState;  
    int32_t      mUseCount;
    CToken*      mToken;
   
    nsTokenAllocator* mTokenAllocator;
#ifdef HEAP_ALLOCATED_NODES
   nsNodeAllocator*  mNodeAllocator; // weak 
#endif
};


class nsCParserStartNode :  public nsCParserNode 
{
public:
    static nsCParserNode* Create(CToken* aToken,
                                 nsTokenAllocator* aTokenAllocator,
                                 nsNodeAllocator* aNodeAllocator)
    {
#ifdef HEAP_ALLOCATED_NODES
      return new
#else
      nsDummyAllocator& pool = aNodeAllocator->GetArenaPool();
      void* place = pool.Alloc(sizeof(nsCParserStartNode));
      NS_ENSURE_TRUE(place, nullptr);
      return ::new (place)
#endif
        nsCParserStartNode(aToken, aTokenAllocator, aNodeAllocator);
    }

    nsCParserStartNode() 
      : nsCParserNode(), mAttributes(0) { }

    nsCParserStartNode(CToken* aToken, 
                       nsTokenAllocator* aTokenAllocator, 
                       nsNodeAllocator* aNodeAllocator = 0) 
      : nsCParserNode(aToken, aTokenAllocator, aNodeAllocator), mAttributes(0) { }

    virtual ~nsCParserStartNode() 
    {
      NS_ASSERTION(mTokenAllocator || mAttributes.GetSize() == 0,
                   "Error: no token allocator");
      CToken* theAttrToken = 0;
      while ((theAttrToken = static_cast<CToken*>(mAttributes.Pop()))) {
        IF_FREE(theAttrToken, mTokenAllocator);
      }
    }

    virtual nsresult Init(CToken* aToken,
                          nsTokenAllocator* aTokenAllocator,
                          nsNodeAllocator* aNodeAllocator = 0);
    virtual void     AddAttribute(CToken* aToken);
    virtual int32_t  GetAttributeCount(bool askToken = false) const;
    virtual const    nsAString& GetKeyAt(uint32_t anIndex) const;
    virtual const    nsAString& GetValueAt(uint32_t anIndex) const;
    virtual CToken*  PopAttributeToken();
    virtual CToken*  PopAttributeTokenFront();
    virtual void     GetSource(nsString& aString) const;
    virtual nsresult ReleaseAll();
protected:
    nsDeque  mAttributes;
};

#endif

