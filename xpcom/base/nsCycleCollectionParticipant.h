/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsCycleCollectionParticipant_h__
#define nsCycleCollectionParticipant_h__

#include "nsISupports.h"

#define NS_CYCLECOLLECTIONPARTICIPANT_IID                                      \
{ /* 407bbd61-13b2-4f85-bdc6-23a59d881f80 */                                   \
    0x407bbd61,                                                                \
    0x13b2,                                                                    \
    0x4f85,                                                                    \
    { 0xbd, 0xc6, 0x23, 0xa5, 0x9d, 0x88, 0x1f, 0x80 }                         \
}

#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY NS_VISIBILITY_DEFAULT

struct nsCycleCollectionTraversalCallback
{
    // You must call DescribeNode() with an accurate refcount,
    // otherwise cycle collection will fail, and probably crash.
    // Providing an accurate objsz or objname is optional.
    virtual void DescribeNode(size_t refcount, 
                              size_t objsz, 
                              const char *objname) = 0;
    virtual void NoteScriptChild(PRUint32 langID, void *child) = 0;
    virtual void NoteXPCOMChild(nsISupports *child) = 0;
};

class NS_COM nsCycleCollectionParticipant
    : public nsISupports
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_CYCLECOLLECTIONPARTICIPANT_IID)
    NS_DECL_ISUPPORTS

    NS_IMETHOD Unlink(nsISupports *p);
    NS_IMETHOD Traverse(nsISupports *p, 
                        nsCycleCollectionTraversalCallback &cb);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsCycleCollectionParticipant, 
                              NS_CYCLECOLLECTIONPARTICIPANT_IID)

#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY NS_VISIBILITY_HIDDEN


///////////////////////////////////////////////////////////////////////////////
// Helpers for implementing a QI to nsCycleCollectionParticipant
///////////////////////////////////////////////////////////////////////////////

#define NS_CYCLE_COLLECTION_INNERCLASS                                         \
        cycleCollection

#define NS_CYCLE_COLLECTION_CLASSNAME(_class)                                  \
        _class::NS_CYCLE_COLLECTION_INNERCLASS

#define NS_CYCLE_COLLECTION_NAME(_class)                                       \
        _class##_cycleCollectorGlobal

#define NS_IMPL_QUERY_CYCLE_COLLECTION(_class)                                 \
  if ( aIID.Equals(NS_GET_IID(nsCycleCollectionParticipant)) ) {               \
    foundInterface = & NS_CYCLE_COLLECTION_NAME(_class);                       \
  } else

#define NS_INTERFACE_MAP_ENTRY_CYCLE_COLLECTION(_class)                        \
  NS_IMPL_QUERY_CYCLE_COLLECTION(_class)


///////////////////////////////////////////////////////////////////////////////
// Helpers for implementing nsCycleCollectionParticipant::Unlink
///////////////////////////////////////////////////////////////////////////////

#define NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class, _base)                   \
  NS_IMETHODIMP                                                                \
  NS_CYCLE_COLLECTION_CLASSNAME(_class)::Unlink(nsISupports *s)                \
  {                                                                            \
    NS_ASSERTION(nsCOMPtr<nsISupports>(do_QueryInterface(s)) == s,             \
                 "not canonical nsISupports pointer");                         \
    _class *tmp = NS_STATIC_CAST(_class*, NS_STATIC_CAST(_base*, s));

#define NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(_field)                       \
    tmp->_field = NULL;    

#define NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(_field)                     \
    tmp->_field.Clear();

#define NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                    \
    return NS_OK;                                                              \
  }


///////////////////////////////////////////////////////////////////////////////
// Helpers for implementing nsCycleCollectionParticipant::Traverse
///////////////////////////////////////////////////////////////////////////////

#define NS_IMPL_CYCLE_COLLECTION_DESCRIBE(_class)                              \
    cb.DescribeNode(tmp->mRefCnt.get(), sizeof(_class), #_class);

#define NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class, _base)                 \
  NS_IMETHODIMP                                                                \
  NS_CYCLE_COLLECTION_CLASSNAME(_class)::Traverse                              \
                         (nsISupports *s,                                      \
                          nsCycleCollectionTraversalCallback &cb)              \
  {                                                                            \
    NS_ASSERTION(nsCOMPtr<nsISupports>(do_QueryInterface(s)) == s,             \
                 "not canonical nsISupports pointer");                         \
    _class *tmp = NS_STATIC_CAST(_class*, NS_STATIC_CAST(_base*, s));          \
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(_class)

#define NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(_field)                     \
    if (tmp->_field) { cb.NoteXPCOMChild(tmp->_field.get()); }

#define NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(_field)                   \
    {                                                                          \
     PRInt32 i;                                                                \
     for (i = 0; i < tmp->_field.Count(); ++i)                                 \
       if (tmp->_field[i])                                                     \
         cb.NoteXPCOMChild(tmp->_field[i]);                                    \
    }

#define NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END                                  \
    return NS_OK;                                                              \
  }

///////////////////////////////////////////////////////////////////////////////
// Helpers for implementing a concrete nsCycleCollectionParticipant 
///////////////////////////////////////////////////////////////////////////////

#define NS_DECL_CYCLE_COLLECTION_CLASS(_class)                                 \
class NS_CYCLE_COLLECTION_INNERCLASS                                           \
 : public nsCycleCollectionParticipant                                         \
{                                                                              \
  NS_IMETHOD Unlink(nsISupports *n);                                           \
  NS_IMETHOD Traverse(nsISupports *n,                                          \
                      nsCycleCollectionTraversalCallback &cb);                 \
};                                                           

#define NS_IMPL_CYCLE_COLLECTION_CLASS(_class)                                 \
  static NS_CYCLE_COLLECTION_CLASSNAME(_class)                                 \
    NS_CYCLE_COLLECTION_NAME(_class);

// The *_AMBIGUOUS macros are needed when a cast from _class* to
// nsISupports* is ambiguous.  The _base parameter must match the base
// class used to implement QueryInterface to nsISupports.

#define NS_IMPL_CYCLE_COLLECTION_0_AMBIGUOUS(_class, _base)                    \
 NS_IMPL_CYCLE_COLLECTION_CLASS(_class)                                        \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class, _base)                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class, _base)                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_1_AMBIGUOUS(_class, _base, _f)                \
 NS_IMPL_CYCLE_COLLECTION_CLASS(_class)                                        \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class, _base)                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(_f)                                  \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class, _base)                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(_f)                                \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_2_AMBIGUOUS(_class, _base, _f1, _f2)          \
 NS_IMPL_CYCLE_COLLECTION_CLASS(_class)                                        \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class, _base)                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(_f1)                                 \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(_f2)                                 \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class, _base)                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(_f1)                               \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(_f2)                               \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_3_AMBIGUOUS(_class, _base, _f1, _f2, _f3)     \
 NS_IMPL_CYCLE_COLLECTION_CLASS(_class)                                        \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class, _base)                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(_f1)                                 \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(_f2)                                 \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(_f3)                                 \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class, _base)                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(_f1)                               \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(_f2)                               \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(_f3)                               \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_0(_class)                                     \
        NS_IMPL_CYCLE_COLLECTION_0_AMBIGUOUS(_class, _class)

#define NS_IMPL_CYCLE_COLLECTION_1(_class, _f)                                 \
        NS_IMPL_CYCLE_COLLECTION_1_AMBIGUOUS(_class, _class, _f)

#define NS_IMPL_CYCLE_COLLECTION_2(_class, _f1, _f2)                           \
        NS_IMPL_CYCLE_COLLECTION_2_AMBIGUOUS(_class, _class, _f1, _f2)

#define NS_IMPL_CYCLE_COLLECTION_3(_class, _f1, _f2, _f3)                      \
        NS_IMPL_CYCLE_COLLECTION_3_AMBIGUOUS(_class, _class, _f1, _f2, _f3)

#endif // nsCycleCollectionParticipant_h__
