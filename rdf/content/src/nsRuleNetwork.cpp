/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author(s):
 *   Chris Waterson <waterson@netscape.com>
 *
 * Contributor(s): 
 */

/*

  Implementations for the rule network classes.

  To Do.

  - Constrain() & Propogate() still feel like they are poorly named.
  - As do Instantiation and InstantiationSet.
  - Make InstantiationSet share and do copy-on-write.
  - Make things iterative, instead of recursive.

 */


#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIContent.h"
#include "nsRuleNetwork.h"
#include "plhash.h"


//----------------------------------------------------------------------
//
// nsRuleNetwork
//

nsRuleNetwork::nsRuleNetwork()
    : mNextVariable(0)
{
}

nsRuleNetwork::~nsRuleNetwork()
{
    // We "own" the nodes. So it's up to us to delete 'em
    for (NodeSet::Iterator node = mNodes.First(); node != mNodes.Last(); ++node)
        delete *node;

    Clear();
}

nsresult
nsRuleNetwork::CreateVariable(PRInt32* aVariable)
{
    *aVariable = ++mNextVariable;
    return NS_OK;
}


nsresult
nsRuleNetwork::Clear()
{
    mNodes.Clear();
    mRoot.RemoveAllChildren();
    return NS_OK;
}


//----------------------------------------------------------------------
//
// Value
//

Value::Value(const Value& aValue)
    : mType(aValue.mType)
{
    MOZ_COUNT_CTOR(Value);

    switch (mType) {
    case eUndefined:
        break;

    case eISupports:
        mISupports = aValue.mISupports;
        NS_IF_ADDREF(mISupports);
        break;

    case eString:
        mString = nsCRT::strdup(aValue.mString);
        break;
    }
}

Value::Value(nsISupports* aISupports)
{
    MOZ_COUNT_CTOR(Value);

    mType = eISupports;
    mISupports = aISupports;
    NS_IF_ADDREF(mISupports);
}

Value::Value(const PRUnichar* aString)
{
    MOZ_COUNT_CTOR(Value);

    mType = eString;
    mString = nsCRT::strdup(aString);
}

Value&
Value::operator=(const Value& aValue)
{
    Clear();

    mType = aValue.mType;

    switch (mType) {
    case eUndefined:
        break;

    case eISupports:
        mISupports = aValue.mISupports;
        NS_IF_ADDREF(mISupports);
        break;

    case eString:
        mString = nsCRT::strdup(aValue.mString);
        break;
    }

    return *this;
}

Value&
Value::operator=(nsISupports* aISupports)
{
    Clear();

    mType = eISupports;
    mISupports = aISupports;
    NS_IF_ADDREF(mISupports);

    return *this;
}

Value&
Value::operator=(const PRUnichar* aString)
{
    Clear();

    mType = eString;
    mString = nsCRT::strdup(aString);

    return *this;
}


Value::~Value()
{
    MOZ_COUNT_DTOR(Value);
    Clear();
}


void
Value::Clear()
{
    switch (mType) {
    case eUndefined:
        break;

    case eISupports:
        NS_IF_RELEASE(mISupports);
        break;

    case eString:
        nsCRT::free(mString);
        break;
    }
}


PRBool
Value::Equals(const Value& aValue) const
{
    if (mType == aValue.mType) {
        switch (mType) {
        case eUndefined:
            return PR_FALSE;

        case eISupports:
            return mISupports == aValue.mISupports;

        case eString:
            return nsCRT::strcmp(mString, aValue.mString) == 0;
        }
    }
    return PR_FALSE;
}

PRBool
Value::Equals(nsISupports* aISupports) const
{
    return (mType == eISupports) && (mISupports == aISupports);
}

PRBool
Value::Equals(const PRUnichar* aString) const
{
    return (mType == eString) && (nsCRT::strcmp(aString, mString) == 0);
}


PLHashNumber
Value::Hash() const
{
    PLHashNumber temp = 0;

    switch (mType) {
    case eUndefined:
        break;

    case eISupports:
        temp = PLHashNumber(mISupports) >> 2; // strip alignment bits
        break;

    case eString:
        {
            PRUnichar* p = mString;
            PRUnichar c;
            while ((c = *p) != 0) {
                temp = (temp >> 28) ^ (temp << 4) ^ c;
                ++p;
            }
        }
        break;
    }

    return temp;
}


Value::operator nsISupports*() const
{
    NS_ASSERTION(mType == eISupports, "not an nsISupports");
    return (mType == eISupports) ? mISupports : 0;
}

Value::operator const PRUnichar*() const
{
    NS_ASSERTION(mType == eString, "not a string");
    return (mType == eString) ? mString : 0;
}


//----------------------------------------------------------------------
//
// VariableSet
//


VariableSet::VariableSet()
    : mVariables(nsnull), mCount(0), mCapacity(0)
{
}

VariableSet::~VariableSet()
{
    delete[] mVariables;
}

nsresult
VariableSet::Add(PRInt32 aVariable)
{
    if (Contains(aVariable))
        return NS_OK;

    if (mCount >= mCapacity) {
        PRInt32 capacity = mCapacity + 4;
        PRInt32* variables = new PRInt32[capacity];
        if (! variables)
            return NS_ERROR_OUT_OF_MEMORY;

        for (PRInt32 i = mCount - 1; i >= 0; --i)
            variables[i] = mVariables[i];

        delete[] mVariables;

        mVariables = variables;
        mCapacity = capacity;
    }

    mVariables[mCount++] = aVariable;
    return NS_OK;
}

nsresult
VariableSet::Remove(PRInt32 aVariable)
{
    PRInt32 i = 0;
    while (i < mCount) {
        if (aVariable == mVariables[i])
            break;

        ++i;
    }

    if (i >= mCount)
        return NS_OK;

    --mCount;

    while (i < mCount) {
        mVariables[i] = mVariables[i + 1];
        ++i;
    }
        
    return NS_OK;
}

PRBool
VariableSet::Contains(PRInt32 aVariable)
{
    for (PRInt32 i = mCount - 1; i >= 0; --i) {
        if (aVariable == mVariables[i])
            return PR_TRUE;
    }

    return PR_FALSE;
}

//----------------------------------------------------------------------=

nsresult
MemoryElementSet::Add(MemoryElement* aElement)
{
    for (ConstIterator element = First(); element != Last(); ++element) {
        if (*element == *aElement) {
            // We've already got this element covered. Since Add()
            // assumes ownership, and we aren't going to need this,
            // just nuke it.
            delete aElement;
            return NS_OK;
        }
    }

    List* list = new List;
    if (! list)
        return NS_ERROR_OUT_OF_MEMORY;

    list->mElement = aElement;
    list->mRefCnt  = 1;
    list->mNext    = mElements;

    mElements = list;

    return NS_OK;
}


//----------------------------------------------------------------------

nsresult
nsBindingSet::Add(const nsBinding& aBinding)
{
    NS_PRECONDITION(! HasBindingFor(aBinding.mVariable), "variable already bound");
    if (HasBindingFor(aBinding.mVariable))
        return NS_ERROR_UNEXPECTED;

    List* list = new List;
    if (! list)
        return NS_ERROR_OUT_OF_MEMORY;

    list->mBinding = aBinding;
    list->mRefCnt  = 1;
    list->mNext    = mBindings;

    mBindings = list;

    return NS_OK;
}

PRInt32
nsBindingSet::Count() const
{
    PRInt32 count = 0;
    for (ConstIterator binding = First(); binding != Last(); ++binding)
        ++count;

    return count;
}

PRBool
nsBindingSet::HasBinding(PRInt32 aVariable, const Value& aValue) const
{
    for (ConstIterator binding = First(); binding != Last(); ++binding) {
        if (binding->mVariable == aVariable && binding->mValue == aValue)
            return PR_TRUE;
    }

    return PR_FALSE;
}

PRBool
nsBindingSet::HasBindingFor(PRInt32 aVariable) const
{
    for (ConstIterator binding = First(); binding != Last(); ++binding) {
        if (binding->mVariable == aVariable)
            return PR_TRUE;
    }

    return PR_FALSE;
}

PRBool
nsBindingSet::GetBindingFor(PRInt32 aVariable, Value* aValue) const
{
    for (ConstIterator binding = First(); binding != Last(); ++binding) {
        if (binding->mVariable == aVariable) {
            *aValue = binding->mValue;
            return PR_TRUE;
        }
    }

    return PR_FALSE;
}

PRBool
nsBindingSet::Equals(const nsBindingSet& aSet) const
{
    if (aSet.mBindings == mBindings)
        return PR_TRUE;

    // If they have a different number of bindings, then they're different.
    if (Count() != aSet.Count())
        return PR_FALSE;

    // XXX O(n^2)! Ugh!
    for (ConstIterator binding = First(); binding != Last(); ++binding) {
        Value value;
        if (! aSet.GetBindingFor(binding->mVariable, &value))
            return PR_FALSE;

        if (binding->mValue != value)
            return PR_FALSE;
    }

    return PR_TRUE;
}

//----------------------------------------------------------------------

PLHashNumber
Instantiation::Hash(const void* aKey)
{
    const Instantiation* inst = NS_STATIC_CAST(const Instantiation*, aKey);

    PLHashNumber result = 0;

    nsBindingSet::ConstIterator last = inst->mBindings.Last();
    for (nsBindingSet::ConstIterator binding = inst->mBindings.First(); binding != last; ++binding)
        result ^= binding->Hash();

    return result;
}


PRIntn
Instantiation::Compare(const void* aLeft, const void* aRight)
{
    const Instantiation* left  = NS_STATIC_CAST(const Instantiation*, aLeft);
    const Instantiation* right = NS_STATIC_CAST(const Instantiation*, aRight);

    return *left == *right;
}


//----------------------------------------------------------------------
//
// InstantiationSet
//

InstantiationSet::InstantiationSet()
{
    mHead.mPrev = mHead.mNext = &mHead;
    MOZ_COUNT_CTOR(InstantiationSet);
}


InstantiationSet::InstantiationSet(const InstantiationSet& aInstantiationSet)
{
    mHead.mPrev = mHead.mNext = &mHead;

    // XXX replace with copy-on-write foo
    ConstIterator last = aInstantiationSet.Last();
    for (ConstIterator inst = aInstantiationSet.First(); inst != last; ++inst)
        Append(*inst);

    MOZ_COUNT_CTOR(InstantiationSet);
}

InstantiationSet&
InstantiationSet::operator=(const InstantiationSet& aInstantiationSet)
{
    // XXX replace with copy-on-write foo
    Clear();

    ConstIterator last = aInstantiationSet.Last();
    for (ConstIterator inst = aInstantiationSet.First(); inst != last; ++inst)
        Append(*inst);

    return *this;
}


void
InstantiationSet::Clear()
{
    Iterator inst = First();
    while (inst != Last())
        Erase(inst++);
}


InstantiationSet::Iterator
InstantiationSet::Insert(Iterator aIterator, const Instantiation& aInstantiation)
{
    List* newelement = new List();
    if (newelement) {
        newelement->mInstantiation = aInstantiation;

        aIterator.mCurrent->mPrev->mNext = newelement;

        newelement->mNext = aIterator.mCurrent;
        newelement->mPrev = aIterator.mCurrent->mPrev;

        aIterator.mCurrent->mPrev = newelement;
    }
    return aIterator;
}

InstantiationSet::Iterator
InstantiationSet::Erase(Iterator aIterator)
{
    Iterator result = aIterator;
    ++result;
    aIterator.mCurrent->mNext->mPrev = aIterator.mCurrent->mPrev;
    aIterator.mCurrent->mPrev->mNext = aIterator.mCurrent->mNext;
    delete aIterator.mCurrent;
    return result;
}


PRBool
InstantiationSet::HasBindingFor(PRInt32 aVariable) const
{
    return !Empty() ? First()->mBindings.HasBindingFor(aVariable) : PR_FALSE;
}

//----------------------------------------------------------------------
//
// ReteNode
//
//   The basic node in the network.
//



//----------------------------------------------------------------------
//
// RootNode
//

nsresult
RootNode::Propogate(const InstantiationSet& aInstantiations, void* aClosure)
{
    NodeSet::Iterator last = mKids.Last();
    for (NodeSet::Iterator kid = mKids.First(); kid != last; ++kid)
        kid->Propogate(aInstantiations, aClosure);

    return NS_OK;
}

nsresult
RootNode::Constrain(InstantiationSet& aInstantiations)
{
    return NS_OK;
}


nsresult
RootNode::GetAncestorVariables(VariableSet& aVariables) const
{
    return NS_OK;
}


PRBool
RootNode::HasAncestor(const ReteNode* aNode) const
{
    return aNode == this;
}

//----------------------------------------------------------------------
//
// JoinNode
//
//   A node that performs a join in the network.
//

JoinNode::JoinNode(InnerNode* aLeftParent,
                   PRInt32 aLeftVariable,
                   InnerNode* aRightParent,
                   PRInt32 aRightVariable,
                   Operator aOperator)
    : mLeftParent(aLeftParent),
      mLeftVariable(aLeftVariable),
      mRightParent(aRightParent),
      mRightVariable(aRightVariable),
      mOperator(aOperator)
{
}

nsresult
JoinNode::Propogate(const InstantiationSet& aInstantiations, void* aClosure)
{
    // the add will have been propogated down from one of the parent
    // nodes: either the left or the right. Test the other node for
    // matches.
    nsresult rv;

    PRBool hasLeftBinding = aInstantiations.HasBindingFor(mLeftVariable);
    PRBool hasRightBinding = aInstantiations.HasBindingFor(mRightVariable);

    NS_ASSERTION(hasLeftBinding ^ hasRightBinding, "there isn't exactly one binding specified");
    if (! (hasLeftBinding ^ hasRightBinding))
        return NS_ERROR_UNEXPECTED;

    InstantiationSet instantiations = aInstantiations;
    InnerNode* test = hasLeftBinding ? mRightParent : mLeftParent;

    // extend the bindings
    InstantiationSet::Iterator last = instantiations.Last();
    for (InstantiationSet::Iterator inst = instantiations.First(); inst != last; ++inst) {
        if (hasLeftBinding) {
            // the left is bound
            Value leftValue;
            inst->mBindings.GetBindingFor(mLeftVariable, &leftValue);
            rv = inst->AddBinding(mRightVariable, leftValue);
        }
        else {
            // the right is bound
            Value rightValue;
            inst->mBindings.GetBindingFor(mRightVariable, &rightValue);
            rv = inst->AddBinding(mLeftVariable, rightValue);
        }

        if (NS_FAILED(rv)) return rv;
    }

    if (! instantiations.Empty()) {
        // propogate consistency checking back up the tree
        rv = test->Constrain(instantiations);
        if (NS_FAILED(rv)) return rv;

        NodeSet::Iterator last = mKids.Last();
        for (NodeSet::Iterator kid = mKids.First(); kid != last; ++kid)
            kid->Propogate(instantiations, aClosure);
    }

    return NS_OK;
}


nsresult
JoinNode::GetNumBound(InnerNode* aAncestor, const InstantiationSet& aInstantiations, PRInt32* aBoundCount)
{
    // Compute the number of variables for an ancestor that are bound
    // in the current instantiation set.
    nsresult rv;

    VariableSet vars;
    rv = aAncestor->GetAncestorVariables(vars);
    if (NS_FAILED(rv)) return rv;

    PRInt32 count = 0;
    for (PRInt32 i = vars.GetCount() - 1; i >= 0; --i) {
        if (aInstantiations.HasBindingFor(vars.GetVariableAt(i)))
            ++count;
    }

    *aBoundCount = count;
    return NS_OK;
}


nsresult
JoinNode::Bind(InstantiationSet& aInstantiations, PRBool* aDidBind)
{
    // Try to use the instantiation set to bind the unbound join
    // variable. If successful, aDidBind <= PR_TRUE.
    nsresult rv;

    PRBool hasLeftBinding = aInstantiations.HasBindingFor(mLeftVariable);
    PRBool hasRightBinding = aInstantiations.HasBindingFor(mRightVariable);

    NS_ASSERTION(! (hasLeftBinding && hasRightBinding), "there is more than one binding specified");
    if (hasLeftBinding && hasRightBinding)
        return NS_ERROR_UNEXPECTED;

    if (hasLeftBinding || hasRightBinding) {
        InstantiationSet::Iterator last = aInstantiations.Last();
        for (InstantiationSet::Iterator inst = aInstantiations.First(); inst != last; ++inst) {
            if (hasLeftBinding) {
                // the left is bound
                Value leftValue;
                inst->mBindings.GetBindingFor(mLeftVariable, &leftValue);
                rv = inst->AddBinding(mRightVariable, leftValue);
            }
            else {
                // the right is bound
                Value rightValue;
                inst->mBindings.GetBindingFor(mRightVariable, &rightValue);
                rv = inst->AddBinding(mLeftVariable, rightValue);
            }

            if (NS_FAILED(rv)) return rv;
        }

        *aDidBind = PR_TRUE;
    }
    else {
        *aDidBind = PR_FALSE;
    }

    return NS_OK;
}

nsresult
JoinNode::Constrain(InstantiationSet& aInstantiations)
{
    if (aInstantiations.Empty())
        return NS_OK;

    nsresult rv;
    PRBool didBind;

    rv = Bind(aInstantiations, &didBind);
    if (NS_FAILED(rv)) return rv;

    PRInt32 numLeftBound;
    rv = GetNumBound(mLeftParent, aInstantiations, &numLeftBound);
    if (NS_FAILED(rv)) return rv;

    PRInt32 numRightBound;
    rv = GetNumBound(mRightParent, aInstantiations, &numRightBound);
    if (NS_FAILED(rv)) return rv;

    InnerNode *first, *last;
    if (numLeftBound > numRightBound) {
        first = mLeftParent;
        last = mRightParent;
    }
    else {
        first = mRightParent;
        last = mLeftParent;
    }

    rv = first->Constrain(aInstantiations);
    if (NS_FAILED(rv)) return rv;

    if (! didBind) {
        rv = Bind(aInstantiations, &didBind);
        if (NS_FAILED(rv)) return rv;

        NS_ASSERTION(didBind, "uh oh, still no binding");
    }

    rv = last->Constrain(aInstantiations);
    if (NS_FAILED(rv)) return rv;

    if (! didBind) {
        // sort throught the full cross product
        NS_NOTYETIMPLEMENTED("write me");
    }

    return NS_OK;
}


nsresult
JoinNode::GetAncestorVariables(VariableSet& aVariables) const
{
    nsresult rv;

    rv = mLeftParent->GetAncestorVariables(aVariables);
    if (NS_FAILED(rv)) return rv;

    rv = mRightParent->GetAncestorVariables(aVariables);
    if (NS_FAILED(rv)) return rv;

    
    if (mLeftVariable) {
        rv = aVariables.Add(mLeftVariable);
        if (NS_FAILED(rv)) return rv;
    }

    if (mRightVariable) {
        rv = aVariables.Add(mRightVariable);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


PRBool
JoinNode::HasAncestor(const ReteNode* aNode) const
{
    if (aNode == this) {
        return PR_TRUE;
    }
    else if (mLeftParent->HasAncestor(aNode) || mRightParent->HasAncestor(aNode)) {
        return PR_TRUE;
    }
    else {
        return PR_FALSE;
    }
}

//----------------------------------------------------------------------
//
// TestNode
//
//   to do:
//     - FilterInstantiations() is poorly named
//


TestNode::TestNode(InnerNode* aParent)
    : mParent(aParent)
{
}


nsresult
TestNode::Propogate(const InstantiationSet& aInstantiations, void* aClosure)
{
    nsresult rv;

    InstantiationSet instantiations = aInstantiations;
    rv = FilterInstantiations(instantiations);
    if (NS_FAILED(rv)) return rv;

    if (! instantiations.Empty()) {
        NodeSet::Iterator last = mKids.Last();
        for (NodeSet::Iterator kid = mKids.First(); kid != last; ++kid)
            kid->Propogate(instantiations, aClosure);
    }

    return NS_OK;
}


nsresult
TestNode::Constrain(InstantiationSet& aInstantiations)
{
    nsresult rv;

    rv = FilterInstantiations(aInstantiations);
    if (NS_FAILED(rv)) return rv;

    if (! aInstantiations.Empty()) {
        // if we still have instantiations, then ride 'em on up to the
        // parent to narrow them.
        rv = mParent->Constrain(aInstantiations);
    }
    else {
        rv = NS_OK;
    }

    return rv;
}


nsresult
TestNode::GetAncestorVariables(VariableSet& aVariables) const
{
    return mParent->GetAncestorVariables(aVariables);
}


PRBool
TestNode::HasAncestor(const ReteNode* aNode) const
{
    return aNode == this ? PR_TRUE : mParent->HasAncestor(aNode);
}


//----------------------------------------------------------------------0

NodeSet::NodeSet()
    : mNodes(nsnull), mCount(0), mCapacity(0)
{
}

NodeSet::~NodeSet()
{
    Clear();
}

nsresult
NodeSet::Add(ReteNode* aNode)
{
    NS_PRECONDITION(aNode != nsnull, "null ptr");
    if (! aNode)
        return NS_ERROR_NULL_POINTER;

    if (mCount >= mCapacity) {
        PRInt32 capacity = mCapacity + 4;
        ReteNode** nodes = new ReteNode*[capacity];
        if (! nodes)
            return NS_ERROR_OUT_OF_MEMORY;

        for (PRInt32 i = mCount - 1; i >= 0; --i)
            nodes[i] = mNodes[i];

        delete[] mNodes;

        mNodes = nodes;
        mCapacity = capacity;
    }

    mNodes[mCount++] = aNode;
    return NS_OK;
}

nsresult
NodeSet::Clear()
{
    delete[] mNodes;
    mNodes = nsnull;
    mCount = mCapacity = 0;
    return NS_OK;
}
