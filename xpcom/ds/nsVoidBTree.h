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

    void SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;

protected:
    // This is as deep as a tree can ever grow, mostly because we use an
    // automatic variable to keep track of the path we take through the
    // tree while inserting and removing stuff.
    enum { kMaxDepth = 7 };

    //----------------------------------------

    /**
     * A node in the b-tree, either data or index
     */
    class Node {
    public:
        enum Type { eType_Data = 0, eType_Index = 1 };

        enum {
            kTypeBit = 1 << 31,
            kCountMask = ~kTypeBit,
            kMaxCapacity = kCountMask
        };

    protected:
        /**
         * High bit is the type (data or index), low bit is the
         * number of elements in the node
         */
        PRInt32 mBits;

        /**
         * Cached size of this node's subtree
         */
        PRInt32 mSubTreeSize;

        /**
         * This node's data; when a Node allocated, this is actually
         * filled in to contain kDataCapacity or kIndexCapacity slots.
         */
        void* mData[1];

    public:
        static nsresult Create(Type aType, PRInt32 aCapacity, Node** aResult);
        static nsresult Destroy(Node* aNode);

        Type GetType() {
            return (mBits & kTypeBit) ? eType_Index : eType_Data; }

        void SetType(Type aType) {
            if (aType == eType_Data)
                mBits &= ~kTypeBit;
            else
                mBits |= kTypeBit;
        }

        PRInt32 GetSubTreeSize() { return mSubTreeSize; }
        void SetSubTreeSize(PRInt32 aSubTreeSize) { mSubTreeSize = aSubTreeSize; }

        PRInt32 GetCount() { return PRInt32(mBits & kCountMask); }
        void SetCount(PRInt32 aCount) {
            NS_PRECONDITION(aCount < kMaxCapacity, "overflow");
            mBits &= ~kCountMask;
            mBits |= aCount & kCountMask; }

        void* GetElementAt(PRInt32 aIndex) {
            NS_PRECONDITION(aIndex >= 0 && aIndex < GetCount(), "bad index");
            return mData[aIndex];
        }

        void SetElementAt(void* aElement, PRInt32 aIndex) {
            NS_PRECONDITION(aIndex >= 0 && aIndex < GetCount(), "bad index");
            mData[aIndex] = aElement;
        }

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

    /**
     * A path through the b-tree, used to avoid recursion and
     * maintain state in iterators.
     */
    class NS_COM Path {
    public:
        // XXXwaterson Kinda gross this is all public, but I couldn't
        // figure out a better way to detect termination in
        // ConstIterator::Next(). It's a s3kret class anyway...
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
        // XXX Tune? If changed, update kMaxDepth appropriately so
        // that we can fit 2^31 elements in here.
        kDataCapacity = 16,
        kIndexCapacity = 16
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

    /**
     * A "const" bidirectional iterator over the nsVoidBTree. Supports
     * the usual iteration interface.
     */
    class NS_COM ConstIterator {
    protected:
        PRBool mIsSingleton;
        PRWord mElement;
        Path   mPath;

        void Next();
        void Prev();

    public:
        ConstIterator() : mIsSingleton(PR_TRUE), mElement(nsnull) {}

        ConstIterator(const ConstIterator& aOther) : mIsSingleton(aOther.mIsSingleton) {
            if (mIsSingleton)
                mElement = aOther.mElement;
            else
                mPath = aOther.mPath; }

        ConstIterator&
        operator=(const ConstIterator& aOther) {
            mIsSingleton = aOther.mIsSingleton;
            if (mIsSingleton)
                mElement = aOther.mElement;
            else
                mPath = aOther.mPath;
            return *this; }

        void* operator*() const {
            return mIsSingleton
                ? NS_REINTERPRET_CAST(void*, mElement)
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
            return mIsSingleton ? mElement == aOther.mElement
                : mPath == aOther.mPath; }

        PRBool operator!=(const ConstIterator& aOther) const {
            return !aOther.operator==(*this); }

    protected:
        friend class nsVoidBTree;
        ConstIterator(PRWord aElement) : mIsSingleton(PR_TRUE), mElement(aElement) {}
        ConstIterator(const Path& aPath) : mIsSingleton(PR_FALSE), mPath(aPath) {}
    };

    /**
     * The first element in the nsVoidBTree
     */
    ConstIterator First() const;

    /**
     * "One past" the last element in the nsVoidBTree
     */
    ConstIterator Last() const;
};


#endif // nsVoidBTree_h__
