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

#include "nsVoidBTree.h"

#ifdef DEBUG
#include <stdio.h>
#endif

// Set this to force the tree to be verified after every insertion and
// removal.
//#define PARANOID 1


//----------------------------------------------------------------------
// nsVoidBTree::Node
//
//   Implementation methods
//

nsresult
nsVoidBTree::Node::Create(Type aType, PRInt32 aCapacity, Node** aResult)
{
    // So we only ever have to do one allocation for a Node, we do a
    // "naked" heap allocation, computing the size of the node and
    // "padding" it out so that it can hold aCapacity slots.
    char* bytes = new char[sizeof(Node) + (aCapacity - 1) * sizeof(void*)];
    if (! bytes)
        return NS_ERROR_OUT_OF_MEMORY;

    Node* result = NS_REINTERPRET_CAST(Node*, bytes);
    result->mBits = 0;
    result->SetType(aType);

    *aResult = result;
    return NS_OK;
}

nsresult
nsVoidBTree::Node::Destroy(Node* aNode)
{
    char* bytes = NS_REINTERPRET_CAST(char*, aNode);
    delete[] bytes;
    return NS_OK;
}

void
nsVoidBTree::Node::InsertElementAt(void* aElement, PRInt32 aIndex)
{
    NS_PRECONDITION(aIndex >= 0 && aIndex <= GetCount(), "bad index");

    PRInt32 count = GetCount();
    SetCount(count + 1);

    while (count > aIndex) {
        mData[count] = mData[count - 1];
        --count;
    }

    mData[aIndex] = aElement;
}

void
nsVoidBTree::Node::RemoveElementAt(PRInt32 aIndex)
{
    NS_PRECONDITION(aIndex >= 0 && aIndex < GetCount(), "bad index");

    PRInt32 count = GetCount();
    SetCount(count - 1);
    
    while (aIndex < count) {
        mData[aIndex] = mData[aIndex + 1];
        ++aIndex;
    }
}


//----------------------------------------------------------------------
//
// nsVoidBTree::Path
//
//   Implementation methods
//

nsVoidBTree::Path::Path(const Path& aOther)
    : mTop(aOther.mTop)
{
    for (PRInt32 i = 0; i < mTop; ++i)
        mLink[i] = aOther.mLink[i];
}

nsVoidBTree::Path&
nsVoidBTree::Path::operator=(const Path& aOther)
{
    mTop = aOther.mTop;
    for (PRInt32 i = 0; i < mTop; ++i)
        mLink[i] = aOther.mLink[i];
    return *this;
}

inline nsresult
nsVoidBTree::Path::Push(Node* aNode, PRInt32 aIndex)
{
    // XXX If you overflow this thing, think about making larger index
    // or data nodes. You can pack a _lot_ of data into a pretty flat
    // tree.
    NS_PRECONDITION(mTop <= kMaxDepth, "overflow");
    if (mTop > kMaxDepth)
        return NS_ERROR_OUT_OF_MEMORY;

    mLink[mTop].mNode  = aNode;
    mLink[mTop].mIndex = aIndex;
    ++mTop;

    return NS_OK;
}


inline void
nsVoidBTree::Path::Pop(Node** aNode, PRInt32* aIndex)
{
    --mTop;
    *aNode  = mLink[mTop].mNode;
    *aIndex = mLink[mTop].mIndex;
}

//----------------------------------------------------------------------
//
//    nsVoidBTree methods
//

nsVoidBTree::nsVoidBTree(const nsVoidBTree& aOther)
{
    ConstIterator last = aOther.Last();
    for (ConstIterator element = aOther.First(); element != last; ++element)
        AppendElement(*element);
}

nsVoidBTree&
nsVoidBTree::operator=(const nsVoidBTree& aOther)
{
    Clear();
    ConstIterator last = aOther.Last();
    for (ConstIterator element = aOther.First(); element != last; ++element)
        AppendElement(*element);
    return *this;
}

PRInt32
nsVoidBTree::Count() const
{
    if (IsEmpty())
        return 0;

    if (IsSingleElement())
        return 1;

    Node* root = NS_REINTERPRET_CAST(Node*, mRoot & kRoot_PointerMask);
    return root->GetSubTreeSize();
}

void*
nsVoidBTree::ElementAt(PRInt32 aIndex) const
{
    if (aIndex < 0 || aIndex >= Count())
        return nsnull;

    if (IsSingleElement())
        return NS_REINTERPRET_CAST(void*, mRoot & kRoot_PointerMask);

    Node* current = NS_REINTERPRET_CAST(Node*, mRoot & kRoot_PointerMask);
    while (current->GetType() != Node::eType_Data) {
        // We're still in the index. Find the right leaf.
        Node* next = nsnull;

        PRInt32 count = current->GetCount();
        for (PRInt32 i = 0; i < count; ++i) {
            Node* child = NS_REINTERPRET_CAST(Node*, current->GetElementAt(i));

            PRInt32 childcount = child->GetSubTreeSize();
            if (PRInt32(aIndex) < childcount) {
                next = child;
                break;
            }

            aIndex -= childcount;
        }

        if (! next) {
            NS_ERROR("corrupted");
            return nsnull;
        }

        current = next;
    }

    return current->GetElementAt(aIndex);
}


PRInt32
nsVoidBTree::IndexOf(void* aPossibleElement) const
{
    NS_PRECONDITION((PRWord(aPossibleElement) & ~kRoot_PointerMask) == 0,
                    "uh oh, someone wants to use the pointer bits");

    NS_PRECONDITION(aPossibleElement != nsnull, "nsVoidBTree can't handle null elements");
    if (aPossibleElement == nsnull)
        return -1;

    PRInt32 result = 0;
    ConstIterator last = Last();
    for (ConstIterator element = First(); element != last; ++element, ++result) {
        if (aPossibleElement == *element)
            return result;
    }

    return -1;
}

  
PRBool
nsVoidBTree::InsertElementAt(void* aElement, PRInt32 aIndex)
{
    NS_PRECONDITION((PRWord(aElement) & ~kRoot_PointerMask) == 0,
                    "uh oh, someone wants to use the pointer bits");

    if ((PRWord(aElement) & ~kRoot_PointerMask) != 0)
        return PR_FALSE;

    NS_PRECONDITION(aElement != nsnull, "nsVoidBTree can't handle null elements");
    if (aElement == nsnull)
        return PR_FALSE;

    PRInt32 count = Count();

    if (aIndex < 0 || aIndex > count)
        return PR_FALSE;

    nsresult rv;

    if (IsSingleElement()) {
        // We're only a single element holder, and haven't yet
        // "faulted" to create the btree.

        if (count == 0) {
            // If we have *no* elements, then just set the root
            // pointer and we're done.
            mRoot = PRWord(aElement);
            return PR_TRUE;
        }

        // If we already had an element, and now we're adding
        // another. Fault and start creating the btree.
        void* element = NS_REINTERPRET_CAST(void*, mRoot & kRoot_PointerMask);

        Node* newroot;
        rv = Node::Create(Node::eType_Data, kDataCapacity, &newroot);
        if (NS_FAILED(rv)) return PR_FALSE;

        newroot->InsertElementAt(element, 0);
        newroot->SetSubTreeSize(1);
        SetRoot(newroot);
    }

    Path path;

    Node* current = NS_REINTERPRET_CAST(Node*, mRoot & kRoot_PointerMask);
    while (current->GetType() != Node::eType_Data) {
        // We're still in the index. Find the right leaf.
        Node* next = nsnull;

        count = current->GetCount();
        for (PRInt32 i = 0; i < count; ++i) {
            Node* child = NS_REINTERPRET_CAST(Node*, current->GetElementAt(i));

            PRInt32 childcount = child->GetSubTreeSize();
            if (PRInt32(aIndex) <= childcount) {
                rv = path.Push(current, i + 1);
                if (NS_FAILED(rv)) return PR_FALSE;

                next = child;
                break;
            }

            aIndex -= childcount;
        }

        if (! next) {
            NS_ERROR("corrupted");
            return PR_FALSE;
        }

        current = next;
    }

    if (current->GetCount() >= kDataCapacity) {
        // We just blew the data node's buffer. Create another
        // datanode and split.
        rv = Split(path, current, aElement, aIndex);
        if (NS_FAILED(rv)) return PR_FALSE;
    }
    else {
        current->InsertElementAt(aElement, aIndex);
        current->SetSubTreeSize(current->GetSubTreeSize() + 1);
    }

    while (path.Length() > 0) {
        PRInt32 index;
        path.Pop(&current, &index);
        current->SetSubTreeSize(current->GetSubTreeSize() + 1);
    }

#ifdef PARANOID
    Verify(NS_REINTERPRET_CAST(Node*, mRoot & kRoot_PointerMask));
#endif

    return PR_TRUE;
}

PRBool
nsVoidBTree::ReplaceElementAt(void* aElement, PRInt32 aIndex)
{
    NS_PRECONDITION((PRWord(aElement) & ~kRoot_PointerMask) == 0,
                    "uh oh, someone wants to use the pointer bits");

    if ((PRWord(aElement) & ~kRoot_PointerMask) != 0)
        return PR_FALSE;

    NS_PRECONDITION(aElement != nsnull, "nsVoidBTree can't handle null elements");
    if (aElement == nsnull)
        return PR_FALSE;

    if (aIndex < 0 || aIndex >= Count())
        return PR_FALSE;

    if (IsSingleElement()) {
        mRoot = PRWord(aElement);
        return PR_TRUE;
    }

    Node* current = NS_REINTERPRET_CAST(Node*, mRoot & kRoot_PointerMask);
    while (current->GetType() != Node::eType_Data) {
        // We're still in the index. Find the right leaf.
        Node* next = nsnull;

        PRInt32 count = current->GetCount();
        for (PRInt32 i = 0; i < count; ++i) {
            Node* child = NS_REINTERPRET_CAST(Node*, current->GetElementAt(i));

            PRInt32 childcount = child->GetSubTreeSize();
            if (PRInt32(aIndex) < childcount) {
                next = child;
                break;
            }

            aIndex -= childcount;
        }

        if (! next) {
            NS_ERROR("corrupted");
            return PR_FALSE;
        }

        current = next;
    }

    current->SetElementAt(aElement, aIndex);
    return PR_TRUE;
}

PRBool
nsVoidBTree::RemoveElement(void* aElement)
{
    PRInt32 index = IndexOf(aElement);
    return (index >= 0) ? RemoveElementAt(index) : PR_FALSE;
}

PRBool
nsVoidBTree::RemoveElementAt(PRInt32 aIndex)
{
    PRInt32 count = Count();

    if (aIndex < 0 || aIndex >= count)
        return PR_FALSE;

    if (IsSingleElement()) {
        // We're removing the one and only element
        mRoot = 0;
        return PR_TRUE;
    }

    // We've got more than one element, and we're removing it.
    nsresult rv;
    Path path;

    Node* root = NS_REINTERPRET_CAST(Node*, mRoot & kRoot_PointerMask);

    Node* current = root;
    while (current->GetType() != Node::eType_Data) {
        // We're still in the index. Find the right leaf.
        Node* next = nsnull;

        count = current->GetCount();
        for (PRInt32 i = 0; i < count; ++i) {
            Node* child = NS_REINTERPRET_CAST(Node*, current->GetElementAt(i));

            PRInt32 childcount = child->GetSubTreeSize();
            if (PRInt32(aIndex) < childcount) {
                rv = path.Push(current, i);
                if (NS_FAILED(rv)) return PR_FALSE;

                next = child;
                break;
            }
            
            aIndex -= childcount;
        }

        if (! next) {
            NS_ERROR("corrupted");
            return PR_FALSE;
        }

        current = next;
    }

    current->RemoveElementAt(aIndex);

    while ((current->GetCount() == 0) && (current != root)) {
        Node* doomed = current;

        PRInt32 index;
        path.Pop(&current, &index);
        current->RemoveElementAt(index);

        Node::Destroy(doomed);
    }

    current->SetSubTreeSize(current->GetSubTreeSize() - 1);

    while (path.Length() > 0) {
        PRInt32 index;
        path.Pop(&current, &index);
        current->SetSubTreeSize(current->GetSubTreeSize() - 1);
    }

    while ((root->GetType() == Node::eType_Index) && (root->GetCount() == 1)) {
        Node* doomed = root;
        root = NS_REINTERPRET_CAST(Node*, root->GetElementAt(0));
        SetRoot(root);
        Node::Destroy(doomed);
    }

#ifdef PARANOID
    Verify(root);
#endif

    return PR_TRUE;
}

void
nsVoidBTree::Clear(void)
{
    if (IsEmpty())
        return;

    if (! IsSingleElement()) {
        Node* root = NS_REINTERPRET_CAST(Node*, mRoot & kRoot_PointerMask);

#ifdef PARANOID
        Dump(root, 0);
#endif

        DestroySubtree(root);
    }

    mRoot = 0;
}


void
nsVoidBTree::Compact(void)
{
    // XXX We could go through and try to merge datanodes.
}

PRBool
nsVoidBTree::EnumerateForwards(EnumFunc aFunc, void* aData) const
{
    PRBool running = PR_TRUE;

    ConstIterator last = Last();
    for (ConstIterator element = First(); running && element != last; ++element)
        running = (*aFunc)(*element, aData);

    return running;
}

PRBool
nsVoidBTree::EnumerateBackwards(EnumFunc aFunc, void* aData) const
{
    PRBool running = PR_TRUE;

    ConstIterator element = Last();
    ConstIterator first = First();

    if (element != first) {
        do {
            running = (*aFunc)(*--element, aData);
        } while (running && element != first);
    }

    return running;
}


#ifdef DEBUG
void
nsVoidBTree::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
    if (! aResult)
        return;

    *aResult = sizeof(*this);

    if (IsSingleElement())
        return;

    Path path;
    path.Push(NS_REINTERPRET_CAST(Node*, mRoot & kRoot_PointerMask), 0);

    while (path.Length()) {
        Node* current;
        PRInt32 index;
        path.Pop(&current, &index);

        if (current->GetType() == Node::eType_Data) {
            *aResult += sizeof(Node) + (sizeof(void*) * (kDataCapacity - 1));
        }
        else {
            *aResult += sizeof(Node) + (sizeof(void*) * (kIndexCapacity - 1));

            // If we're in an index node, and there are still kids to
            // traverse, well, traverse 'em.
            if (index < current->GetCount()) {
                path.Push(current, index + 1);
                path.Push(NS_STATIC_CAST(Node*, current->GetElementAt(index)), 0);
            }
        }
    }
}
#endif

//----------------------------------------------------------------------

nsresult
nsVoidBTree::Split(Path& path, Node* aOldNode, void* aElementToInsert, PRInt32 aSplitIndex)
{
    nsresult rv;

    PRInt32 capacity = (aOldNode->GetType() == Node::eType_Data) ? kDataCapacity : kIndexCapacity;
    PRInt32 delta = 0;


    Node* newnode;
    rv = Node::Create(aOldNode->GetType(), capacity, &newnode);
    if (NS_FAILED(rv)) return rv;

    if (aSplitIndex == capacity) {
        // If aSplitIndex is the same as the capacity of the node,
        // then there'll be nothing to copy from the old node to the
        // new node, and the element is really meant to be inserted in
        // the newnode. In that case, do it _now_ so that newnode's
        // subtree size will be correct.
        newnode->InsertElementAt(aElementToInsert, 0);

        if (newnode->GetType() == Node::eType_Data) {
            newnode->SetSubTreeSize(1);
        }
        else {
            Node* child = NS_REINTERPRET_CAST(Node*, aElementToInsert);
            newnode->SetSubTreeSize(child->GetSubTreeSize());
        }
    }
    else {
        // We're meant to insert the element into the oldnode at
        // aSplitIndex. Copy data from aOldNode to the newnode but
        // _don't_ insert newnode yet. We may need to recursively
        // split parents, an operation that allocs, and hence, may
        // fail. If it does fail, we wan't to not screw up the
        // existing datastructure.
        //
        // Note that it should be the case that count == capacity, but
        // who knows, we may decide at some point to prematurely split
        // nodes for some reason or another.
        PRInt32 count = aOldNode->GetCount();
        PRInt32 i = aSplitIndex;
        PRInt32 j = 0;

        newnode->SetCount(count - aSplitIndex);
        while (i < count) {
            if (aOldNode->GetType() == Node::eType_Data) {
                ++delta;
            }
            else {
                Node* migrating = NS_REINTERPRET_CAST(Node*, aOldNode->GetElementAt(i));
                delta += migrating->GetSubTreeSize();
            }

            newnode->SetElementAt(aOldNode->GetElementAt(i), j);
            ++i;
            ++j;
        }
        newnode->SetSubTreeSize(delta);
    }

    // Now we split the node.

    if (path.Length() == 0) {
        // We made it all the way up to the root! Ok, so, create a new
        // root
        Node* newroot;
        rv = Node::Create(Node::eType_Index, kIndexCapacity, &newroot);
        if (NS_FAILED(rv)) return rv;

        newroot->SetCount(2);
        newroot->SetElementAt(aOldNode, 0);
        newroot->SetElementAt(newnode, 1);
        newroot->SetSubTreeSize(aOldNode->GetSubTreeSize() + 1);
        SetRoot(newroot);
    }
    else {
        // Otherwise, use the "path" to pop off the next thing above us.
        Node* parent;
        PRInt32 indx;
        path.Pop(&parent, &indx);

        if (parent->GetCount() >= kIndexCapacity) {
            // Parent is full, too. Recursively split it.
            rv = Split(path, parent, newnode, indx);
            if (NS_FAILED(rv)) {
                Node::Destroy(newnode);
                return rv;
            }
        }
        else {
            // Room in the parent, so just smack it on up there.
            parent->InsertElementAt(newnode, indx);
            parent->SetSubTreeSize(parent->GetSubTreeSize() + 1);
        }
    }

    // Now, since all our operations that might fail have finished, we
    // can go ahead and monkey with the old node.

    if (aSplitIndex == capacity) {
        PRInt32 nodeslost = newnode->GetSubTreeSize() - 1;
        PRInt32 subtreesize = aOldNode->GetSubTreeSize() - nodeslost;
        aOldNode->SetSubTreeSize(subtreesize);
    }
    else {
        aOldNode->SetCount(aSplitIndex);
        aOldNode->InsertElementAt(aElementToInsert, aSplitIndex);
        PRInt32 subtreesize = aOldNode->GetSubTreeSize() - delta + 1;
        aOldNode->SetSubTreeSize(subtreesize);
    }

    return NS_OK;
}


PRInt32
nsVoidBTree::Verify(Node* aNode)
{
    // Sanity check the tree by verifying that the subtree sizes all
    // add up correctly.
    if (aNode->GetType() == Node::eType_Data) {
        NS_ASSERTION(aNode->GetCount() == aNode->GetSubTreeSize(), "corrupted");
        return aNode->GetCount();
    }

    PRInt32 childcount = 0;
    for (PRInt32 i = 0; i < aNode->GetCount(); ++i) {
        Node* child = NS_REINTERPRET_CAST(Node*, aNode->GetElementAt(i));
        childcount += Verify(child);
    }

    NS_ASSERTION(childcount == aNode->GetSubTreeSize(), "corrupted");
    return childcount;
}


void
nsVoidBTree::DestroySubtree(Node* aNode)
{
    PRInt32 count = aNode->GetCount() - 1;
    while (count >= 0) {
        if (aNode->GetType() == Node::eType_Index)
            DestroySubtree(NS_REINTERPRET_CAST(Node*, aNode->GetElementAt(count)));
        
        --count;
    }

    Node::Destroy(aNode);
}

#ifdef DEBUG
void
nsVoidBTree::Dump(Node* aNode, PRInt32 aIndent)
{
    for (PRInt32 i = 0; i < aIndent; ++i)
        printf("  ");

    if (aNode->GetType() == Node::eType_Data) {
        printf("data(%d/%d)\n", aNode->GetCount(), aNode->GetSubTreeSize());
    }
    else {
        printf("index(%d/%d)\n", aNode->GetCount(), aNode->GetSubTreeSize());
        for (PRInt32 j = 0; j < aNode->GetCount(); ++j)
            Dump(NS_REINTERPRET_CAST(Node*, aNode->GetElementAt(j)), aIndent + 1);
    }
}
#endif

//----------------------------------------------------------------------
//
// nsVoidBTree::ConstIterator and Iterator methods
//

void* nsVoidBTree::kDummyLast;

void
nsVoidBTree::ConstIterator::Next()
{
    if (mIsSingleton) {
        mIsExhausted = PR_TRUE;
        return;
    }

    // Otherwise we're a real b-tree iterator, and we need to pull and
    // pop our path stack appropriately to gyrate into the right
    // position.
    while (1) {
        Node* current;
        PRInt32 index;
        mPath.Pop(&current, &index);

        PRInt32 count = current->GetCount();

        NS_ASSERTION(index < count, "ran off the end, pal");

        if (++index >= count) {
            // XXXwaterson Oh, this is so ugly. I wish I was smart
            // enough to figure out a prettier way to do it.
            //
            // See if we've just iterated past the last element in the
            // b-tree, and now need to leave ourselves in the magical
            // state that is equal to nsVoidBTree::Last().
            if (current->GetType() == Node::eType_Data) {
                PRBool rightmost = PR_TRUE;
                for (PRInt32 slot = mPath.mTop - 1; slot >= 0; --slot) {
                    const Link& link = mPath.mLink[slot];
                    if (link.mIndex != link.mNode->GetCount() - 1) {
                        rightmost = PR_FALSE;
                        break;
                    }
                }

                if (rightmost) {
                    // It's the last one. Make the path look exactly
                    // like nsVoidBTree::Last().
                    mPath.Push(current, index);
                    return;
                }
            }

            // Otherwise, we just ran off the end of a "middling"
            // node. Loop around, to pop back up the b-tree to its
            // parent.
            continue;
        }

        // We're somewhere in the middle. Push the new location onto
        // the stack.
        mPath.Push(current, index);

        // If we're in a data node, we're done: break out of the loop
        // here leaving the top of the stack pointing to the next data
        // element in the b-tree.
        if (current->GetType() == Node::eType_Data)
            break;

        // Otherwise, we're still in an index node. Push next node
        // down onto the stack, starting "one off" to the left, and
        // continue around.
        mPath.Push(NS_STATIC_CAST(Node*, current->GetElementAt(index)), -1);
    }
}

void
nsVoidBTree::ConstIterator::Prev()
{
    if (mIsSingleton) {
        mIsExhausted = PR_FALSE;
        return;
    }

    // Otherwise we're a real b-tree iterator, and we need to pull and
    // pop our path stack appropriately to gyrate into the right
    // position. This is just like nsVoidBTree::ConstIterator::Next(),
    // but in reverse.
    while (1) {
        Node* current;
        PRInt32 index;
        mPath.Pop(&current, &index);

        NS_ASSERTION(index >= 0, "ran off the front, pal");

        if (--index < 0)
            continue;

        mPath.Push(current, index);

        if (current->GetType() == Node::eType_Data)
            break;

        current = NS_STATIC_CAST(Node*, current->GetElementAt(index));
        mPath.Push(current, current->GetCount());
    }
}

const nsVoidBTree::Path
nsVoidBTree::LeftMostPath() const
{
    Path path;
    Node* current = NS_REINTERPRET_CAST(Node*, mRoot & kRoot_PointerMask);

    while (1) {
        path.Push(current, 0);

        if (current->GetType() == Node::eType_Data)
            break;

        current = NS_STATIC_CAST(Node*, current->GetElementAt(0));
    }

    return path;
}


const nsVoidBTree::Path
nsVoidBTree::RightMostPath() const
{
    Path path;
    Node* current = NS_REINTERPRET_CAST(Node*, mRoot & kRoot_PointerMask);

    while (1) {
        PRInt32 count = current->GetCount();

        if (current->GetType() == Node::eType_Data) {
            path.Push(current, count);
            break;
        }

        path.Push(current, count - 1);
        current = NS_STATIC_CAST(Node*, current->GetElementAt(count - 1));
    }

    return path;
}
