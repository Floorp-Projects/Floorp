/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 * The Initial Developer of this code under the MPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Original Author:
 *   Chris Waterson <waterson@netscape.com>
 */

#ifndef nsVoidBTree_h__
#define nsVoidBTree_h__

#include "nscore.h"
#include "nsDebug.h"
#include "nsError.h"
class nsISizeOfHandler;

/**
 * An nsVoidArray-compatible class that is implemented as a B-tree
 * rather than an array.
 */
class NS_COM nsVoidBTree
{
public:
    nsVoidBTree() : mRoot(0) {}

    virtual ~nsVoidBTree() { Clear(); }

    nsVoidBTree(const nsVoidBTree& aOther);

    nsVoidBTree& operator=(const nsVoidBTree& aOther);

    PRInt32 Count() const;

    void* ElementAt(PRInt32 aIndex) const;

    void* operator[](PRInt32 aIndex) const {
        return ElementAt(aIndex); }

    PRInt32 IndexOf(void* aPossibleElement) const;

    PRBool InsertElementAt(void* aElement, PRInt32 aIndex);

    PRBool ReplaceElementAt(void* aElement, PRInt32 aIndex);

    PRBool AppendElement(void* aElement) {
        return InsertElementAt(aElement, Count()); }

    PRBool RemoveElement(void* aElement);

    PRBool RemoveElementAt(PRInt32 aIndex);

    void Clear();

    void Compact();

    /**
     * Enumerator callback function. Return PR_FALSE to stop
     */
    typedef PRBool (*PR_CALLBACK EnumFunc)(void* aElement, void *aData);

    PRBool EnumerateForwards(EnumFunc aFunc, void* aData) const;
    PRBool EnumerateBackwards(EnumFunc aFunc, void* aData) const;

#ifdef DEBUG
    void SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif

protected:
    // This is as deep as a tree can ever grow, mostly because we use an
    // automatic variable to keep track of the path we take through the
    // tree while inserting and removing stuff.
    enum { kMaxDepth = 8 };

    //----------------------------------------

    /**
     * A node in the b-tree, either data or index
     */
    class Node {
    public:
        enum Type { eType_Data = 0, eType_Index = 1 };

        enum {
            kTypeBit = 0x80000000,
            kCountShift = 24,
            kCountMask = 0x7f000000,
            kMaxCapacity = kCountMask,
            kSubTreeSizeMask = 0x00ffffff,
            kMaxSubTreeSize = kSubTreeSizeMask
        };

    protected:
        /**
         * High bit is the type (data or index), next 7 bits are the
         * number of elements in the node, low 24 bits is the subtree
         * size.
         */
        PRUint32 mBits;

        /**
         * This node's data; when a Node allocated, this is actually
         * filled in to contain kDataCapacity or kIndexCapacity slots.
         */
        void* mData[1];

    public:
        static nsresult Create(Type aType, PRInt32 aCapacity, Node** aResult);
        static nsresult Destroy(Node* aNode);

        Type GetType() const {
            return (mBits & kTypeBit) ? eType_Index : eType_Data; }

        void SetType(Type aType) {
            if (aType == eType_Data)
                mBits &= ~kTypeBit;
            else
                mBits |= kTypeBit;
        }

        PRInt32 GetSubTreeSize() const {
            return PRInt32(mBits & kSubTreeSizeMask); }

        void SetSubTreeSize(PRInt32 aSubTreeSize) {
            mBits &= ~kSubTreeSizeMask;
            mBits |= PRUint32(aSubTreeSize) & kSubTreeSizeMask; }

        PRInt32 GetCount() const { return PRInt32((mBits & kCountMask) >> kCountShift); }

        void SetCount(PRInt32 aCount) {
            NS_PRECONDITION(aCount < PRInt32(kMaxCapacity), "overflow");
            mBits &= ~kCountMask;
            mBits |= (PRUint32(aCount) << kCountShift) & kCountMask; }

        void* GetElementAt(PRInt32 aIndex) const {
            NS_PRECONDITION(aIndex >= 0 && aIndex < GetCount(), "bad index");
            return mData[aIndex]; }

        void*& GetElementAt(PRInt32 aIndex) {
            NS_PRECONDITION(aIndex >= 0 && aIndex < GetCount(), "bad index");
            return mData[aIndex]; }

        void SetElementAt(void* aElement, PRInt32 aIndex) {
            NS_PRECONDITION(aIndex >= 0 && aIndex < GetCount(), "bad index");
            mData[aIndex] = aElement; }

        void InsertElementAt(void* aElement, PRInt32 aIndex);
        void RemoveElementAt(PRInt32 aIndex);

    protected:
        // XXX Not to be implemented
        Node();
        ~Node();
    };

    //----------------------------------------
    class Path;
    friend class Path;

    struct Link;
    friend struct Link;

    // XXXwaterson Kinda gross this is all public, but I couldn't
    // figure out a better way to detect termination in
    // ConstIterator::Next(). It's a s3kret class anyway. This isn't
    // contained inside nsVoidBTree::Path because doing so breaks some
    // compilers.
    struct Link {
        Node*   mNode;
        PRInt32 mIndex;

        Link&
        operator=(const Link& aOther) {
            mNode = aOther.mNode;
            mIndex = aOther.mIndex;
            return *this; }

        PRBool operator==(const Link& aOther) const {
            return mNode == aOther.mNode && mIndex == aOther.mIndex; }

        PRBool operator!=(const Link& aOther) const {
            return !aOther.operator==(*this); }
    };

    /**
     * A path through the b-tree, used to avoid recursion and
     * maintain state in iterators.
     */
    class NS_COM Path {
    public:
        Link mLink[kMaxDepth];
        PRInt32 mTop;

        Path() : mTop(0) {}

        Path(const Path& aOther);
        Path& operator=(const Path& aOther);

        PRBool operator==(const Path& aOther) const {
            if (mTop != aOther.mTop)
                return PR_FALSE;
            PRInt32 last = mTop - 1;
            if (last >= 0 && mLink[last] != aOther.mLink[last])
                return PR_FALSE;
            return PR_TRUE; }

        PRBool operator!=(const Path& aOther) const {
            return !aOther.operator==(*this); }

        inline nsresult Push(Node* aNode, PRInt32 aIndex);
        inline void Pop(Node** aNode, PRInt32* aIndex);

        PRInt32 Length() const { return mTop; }

        Node* TopNode() const { return mLink[mTop - 1].mNode; }
        PRInt32 TopIndex() const { return mLink[mTop - 1].mIndex; }
    };

    /**
     * A tagged pointer: if it's null, the b-tree is empty. If it's
     * non-null, and the low-bit is clear, it points to a single
     * element. If it's non-null, and the low-bit is set, it points to
     * a Node object.
     */
    PRWord mRoot;

    enum {
        kRoot_TypeBit = 1,
        kRoot_SingleElement = 0,
        kRoot_Node = 1,
        kRoot_PointerMask = ~kRoot_TypeBit
    };

    // Bit twiddlies
    PRBool IsEmpty() const { return mRoot == 0; }

    PRBool IsSingleElement() const {
        return (mRoot & kRoot_TypeBit) == kRoot_SingleElement; }

    void SetRoot(Node* aNode) {
        mRoot = PRWord(aNode) | kRoot_Node; }

    enum {
        // This is tuned based on distribution data from nsVoidArray
        // that indicated that, during a "normal run" of mozilla, we'd
        // be able to fit about 90% of the nsVoidArray's contents into
        // an eight element array.
        //
        // If you decide to change these values, update kMaxDepth
        // appropriately so that we can fit kMaxSubTreeSize elements
        // in here.
        kDataCapacity = 8,
        kIndexCapacity = 8
    };

    nsresult Split(Path& path, Node* aOldNode, void* aElementToInsert, PRInt32 aSplitIndex);
    PRInt32 Verify(Node* aNode);
    void DestroySubtree(Node* aNode);

#ifdef DEBUG
    void Dump(Node* aNode, PRInt32 aIndent);
#endif

public:

    class ConstIterator;
    friend class ConstIterator;

    class Iterator;
    friend class Iterator;

    /**
     * A "const" bidirectional iterator over the nsVoidBTree. Supports
     * the usual iteration interface.
     */
    class NS_COM ConstIterator {
    protected:
        friend class Iterator; // XXXwaterson broken

        PRPackedBool mIsSingleton;
        PRPackedBool mIsExhausted;
        PRWord mElement;
        Path   mPath;

        void Next();
        void Prev();

    public:
        ConstIterator() : mIsSingleton(PR_TRUE), mElement(nsnull) {}

        ConstIterator(const ConstIterator& aOther) : mIsSingleton(aOther.mIsSingleton) {
            if (mIsSingleton) {
                mElement = aOther.mElement;
                mIsExhausted = aOther.mIsExhausted;
            }
            else {
                mPath = aOther.mPath; } }

        ConstIterator&
        operator=(const ConstIterator& aOther) {
            mIsSingleton = aOther.mIsSingleton;
            if (mIsSingleton) {
                mElement = aOther.mElement;
                mIsExhausted = aOther.mIsExhausted;
            }
            else {
                mPath = aOther.mPath; 
            }
            return *this; }

        void* operator*() const {
            return mIsSingleton
                ? NS_REINTERPRET_CAST(void*, !mIsExhausted ? mElement : 0)
                : mPath.TopNode()->GetElementAt(mPath.TopIndex()); }

        ConstIterator& operator++() {
            Next();
            return *this; }

        ConstIterator operator++(int) {
            ConstIterator temp(*this);
            Next();
            return temp; }

        ConstIterator& operator--() {
            Prev();
            return *this; }

        ConstIterator operator--(int) {
            ConstIterator temp(*this);
            Prev();
            return temp; }

        PRBool operator==(const ConstIterator& aOther) const {
            return mIsSingleton
                ? (mElement == aOther.mElement && mIsExhausted == aOther.mIsExhausted)
                : mPath == aOther.mPath; }

        PRBool operator!=(const ConstIterator& aOther) const {
            return !aOther.operator==(*this); }

    protected:
        friend class nsVoidBTree;
        ConstIterator(PRWord aElement, PRBool aIsExhausted)
            : mIsSingleton(PR_TRUE), mIsExhausted(aElement ? aIsExhausted : PR_TRUE), mElement(aElement) {}

        ConstIterator(const Path& aPath)
            : mIsSingleton(PR_FALSE), mPath(aPath) {}
    };

    /**
     * The first element in the nsVoidBTree
     */
    ConstIterator First() const {
        return IsSingleElement() ? ConstIterator(mRoot, PR_FALSE) : ConstIterator(LeftMostPath()); }

    /**
     * "One past" the last element in the nsVoidBTree
     */
    ConstIterator Last() const {
        return IsSingleElement() ? ConstIterator(mRoot, PR_TRUE) : ConstIterator(RightMostPath()); }

    class Iterator : public ConstIterator {
    protected:
        void** mElementRef;

    public:
        Iterator() {}

        Iterator(const Iterator& aOther)
            : ConstIterator(aOther),
              mElementRef(aOther.mElementRef) {}

        Iterator&
        operator=(const Iterator& aOther) {
            ConstIterator::operator=(aOther);
            mElementRef = aOther.mElementRef;
            return *this; }

        Iterator& operator++() {
            Next();
            return *this; }

        Iterator operator++(int) {
            Iterator temp(*this);
            Next();
            return temp; }

        Iterator& operator--() {
            Prev();
            return *this; }

        Iterator operator--(int) {
            Iterator temp(*this);
            Prev();
            return temp; }

        void*& operator*() const {
            return mIsSingleton
                ? (!mIsExhausted ? *mElementRef : kDummyLast)
                : mPath.TopNode()->GetElementAt(mPath.TopIndex()); }

        PRBool operator==(const Iterator& aOther) const {
            return mIsSingleton
                ? (mElement == aOther.mElement && mIsExhausted == aOther.mIsExhausted)
                : mPath == aOther.mPath; }

        PRBool operator!=(const Iterator& aOther) const {
            return !aOther.operator==(*this); }

    protected:
        Iterator(const Path& aPath)
            : ConstIterator(aPath) {}

        Iterator(PRWord* aElementRef, PRBool aIsExhausted)
            : ConstIterator(*aElementRef, aIsExhausted),
              mElementRef(NS_REINTERPRET_CAST(void**, aElementRef)) {}

        friend class nsVoidBTree;
    };

    Iterator First() {
        return IsSingleElement() ? Iterator(&mRoot, PR_FALSE) : Iterator(LeftMostPath()); }

    Iterator Last() {
        return IsSingleElement() ? Iterator(&mRoot, PR_TRUE) : Iterator(RightMostPath()); }

protected:
    const Path LeftMostPath() const;
    const Path RightMostPath() const;

    static void* kDummyLast;
};


#endif // nsVoidBTree_h__
