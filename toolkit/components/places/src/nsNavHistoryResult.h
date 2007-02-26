/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Places code.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/**
 * The definitions of objects that make up a history query result set. This file
 * should only be included by nsNavHistory.h, include that if you want these
 * classes.
 */

#ifndef nsNavHistoryResult_h_
#define nsNavHistoryResult_h_

#include "nsTArray.h"
#include "nsInterfaceHashtable.h"
#include "nsDataHashtable.h"

class nsNavHistory;
class nsIDateTimeFormat;
class nsIWritablePropertyBag;
class nsNavHistoryQuery;
class nsNavHistoryQueryOptions;

class nsNavHistoryContainerResultNode;
class nsNavHistoryFolderResultNode;
class nsNavHistoryQueryResultNode;
class nsNavHistoryVisitResultNode;

/**
 * hashkey wrapper using PRInt64 KeyType
 *
 * @see nsTHashtable::EntryType for specification
 *
 * This just truncates the 64-bit int to a 32-bit one for using a hash number.
 * It is used for bookmark folder IDs, which should be way less than 2^32.
 */
class nsTrimInt64HashKey : public PLDHashEntryHdr
{
public:
  typedef const PRInt64& KeyType;
  typedef const PRInt64* KeyTypePointer;

  nsTrimInt64HashKey(KeyTypePointer aKey) : mValue(*aKey) { }
  nsTrimInt64HashKey(const nsTrimInt64HashKey& toCopy) : mValue(toCopy.mValue) { }
  ~nsTrimInt64HashKey() { }

  KeyType GetKey() const { return mValue; }
  KeyTypePointer GetKeyPointer() const { return &mValue; }
  PRBool KeyEquals(KeyTypePointer aKey) const { return *aKey == mValue; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
    { return NS_STATIC_CAST(PRUint32, (*aKey) & PR_UINT32_MAX); }
  enum { ALLOW_MEMMOVE = PR_TRUE };

private:
  const PRInt64 mValue;
};


// Declare methods for implementing nsINavBookmarkObserver
// and nsINavHistoryObserver (some methods, such as BeginUpdateBatch overlap)
#define NS_DECL_BOOKMARK_HISTORY_OBSERVER                               \
  NS_DECL_NSINAVBOOKMARKOBSERVER                                        \
  NS_IMETHOD OnVisit(nsIURI* aURI, PRInt64 aVisitId, PRTime aTime,      \
                     PRInt64 aSessionId, PRInt64 aReferringId,          \
                     PRUint32 aTransitionType);                         \
  NS_IMETHOD OnTitleChanged(nsIURI* aURI, const nsAString& aPageTitle,  \
                            const nsAString& aUserTitle,                \
                            PRBool aIsUserTitleChanged);                \
  NS_IMETHOD OnDeleteURI(nsIURI *aURI);                                 \
  NS_IMETHOD OnClearHistory();                                          \
  NS_IMETHOD OnPageChanged(nsIURI *aURI, PRUint32 aWhat,                \
                           const nsAString &aValue);                    \
  NS_IMETHOD OnPageExpired(nsIURI* aURI, PRTime aVisitTime,             \
                           PRBool aWholeEntry);


// nsNavHistoryResult
//
//    nsNavHistory creates this object and fills in mChildren (by getting
//    it through GetTopLevel()). Then FilledAllResults() is called to finish
//    object initialization.
//
//    This object implements nsITreeView so you can just set it to a tree
//    view and it will work. This object also observes the necessary history
//    and bookmark events to keep itself up-to-date.

#define NS_NAVHISTORYRESULT_IID \
  { 0x455d1d40, 0x1b9b, 0x40e6, { 0xa6, 0x41, 0x8b, 0xb7, 0xe8, 0x82, 0x23, 0x87 } }

class nsNavHistoryResult : public nsSupportsWeakReference,
                           public nsINavHistoryResult,
                           public nsINavBookmarkObserver,
                           public nsINavHistoryObserver
{
public:
  static nsresult NewHistoryResult(nsINavHistoryQuery** aQueries,
                                   PRUint32 aQueryCount,
                                   nsNavHistoryQueryOptions* aOptions,
                                   nsNavHistoryContainerResultNode* aRoot,
                                   nsNavHistoryResult** result);

  // the tree viewer can go faster if it can bypass XPCOM
  friend class nsNavHistoryResultTreeViewer;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NAVHISTORYRESULT_IID)

  nsresult PropertyBagFor(nsISupports* aObject,
                          nsIWritablePropertyBag** aBag);

  NS_DECL_ISUPPORTS
  NS_DECL_NSINAVHISTORYRESULT
  NS_DECL_BOOKMARK_HISTORY_OBSERVER

  void AddEverythingObserver(nsNavHistoryQueryResultNode* aNode);
  void AddBookmarkObserver(nsNavHistoryFolderResultNode* aNode, PRInt64 aFolder);
  void RemoveEverythingObserver(nsNavHistoryQueryResultNode* aNode);
  void RemoveBookmarkObserver(nsNavHistoryFolderResultNode* aNode, PRInt64 aFolder);

  // returns the view. NOT-ADDREFED. May be NULL if there is no view
  nsINavHistoryResultViewer* GetView() const
    { return mView; }

public:
  // two-stage init, use NewHistoryResult to construct
  nsNavHistoryResult(nsNavHistoryContainerResultNode* mRoot);
  ~nsNavHistoryResult();
  nsresult Init(nsINavHistoryQuery** aQueries,
                PRUint32 aQueryCount,
                nsNavHistoryQueryOptions *aOptions);

  nsRefPtr<nsNavHistoryContainerResultNode> mRootNode;

  nsCOMArray<nsINavHistoryQuery> mQueries;
  nsCOMPtr<nsNavHistoryQueryOptions> mOptions;

  // One of nsNavHistoryQueryOptions.SORY_BY_* This is initialized to mOptions.sortingMode,
  // but may be overridden if the user clicks on one of the columns.
  PRUint32 mSortingMode;

  nsCOMPtr<nsINavHistoryResultViewer> mView;

  // property bags for all result nodes, see PropertyBagFor
  nsInterfaceHashtable<nsISupportsHashKey, nsIWritablePropertyBag> mPropertyBags;

  // node observers
  PRBool mIsHistoryObserver;
  PRBool mIsBookmarksObserver;
  nsTArray<nsNavHistoryQueryResultNode*> mEverythingObservers;
  typedef nsTArray<nsNavHistoryFolderResultNode*> FolderObserverList;
  nsDataHashtable<nsTrimInt64HashKey, FolderObserverList* > mBookmarkObservers;
  FolderObserverList* BookmarkObserversForId(PRInt64 aFolderId, PRBool aCreate);

  void RecursiveExpandCollapse(nsNavHistoryContainerResultNode* aContainer,
                               PRBool aExpand);

  void InvalidateTree();
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsNavHistoryResult, NS_NAVHISTORYRESULT_IID)

// nsNavHistoryResultNode
//
//    This is the base class for every node in a result set. The result itself
//    is a node (nsNavHistoryResult inherits from this), as well as every
//    leaf and branch on the tree.

#define NS_NAVHISTORYRESULTNODE_IID \
  {0x54b61d38, 0x57c1, 0x11da, {0x95, 0xb8, 0x00, 0x13, 0x21, 0xc9, 0xf6, 0x9e}}

// These are all the simple getters, they can be used for the result node
// implementation and all subclasses. More complex are GetIcon, GetParent
// (which depends on the definition of container result node), and GetUri
// (which is overridded for lazy construction for some containers).
#define NS_IMPLEMENT_SIMPLE_RESULTNODE \
  NS_IMETHOD GetTitle(nsACString& aTitle) \
    { aTitle = mTitle; return NS_OK; } \
  NS_IMETHOD GetAccessCount(PRUint32* aAccessCount) \
    { *aAccessCount = mAccessCount; return NS_OK; } \
  NS_IMETHOD GetTime(PRTime* aTime) \
    { *aTime = mTime; return NS_OK; } \
  NS_IMETHOD GetIndentLevel(PRUint32* aIndentLevel) \
    { *aIndentLevel = mIndentLevel; return NS_OK; } \
  NS_IMETHOD GetViewIndex(PRInt32* aViewIndex) \
    { *aViewIndex = mViewIndex; return NS_OK; } \
  NS_IMETHOD SetViewIndex(PRInt32 aViewIndex) \
    { mViewIndex = aViewIndex; return NS_OK; } \
  NS_IMETHOD GetBookmarkIndex(PRInt32* aIndex) \
    { *aIndex = mBookmarkIndex; return NS_OK; } \
  NS_IMETHOD GetBookmarkId(PRInt64* aId) \
    { *aId= mBookmarkId; return NS_OK; }

// This is used by the base classes instead of
// NS_FORWARD_NSINAVHISTORYRESULTNODE(nsNavHistoryResultNode) because they
// need to redefine GetType and GetUri rather than forwarding them. This
// implements all the simple getters instead of forwarding because they are so
// short and we can save a virtual function call.
//
// (GetUri is redefined only by QueryResultNode and FolderResultNode because
// the queries might not necessarily be parsed. The rest just return the node's
// buffer.)
#define NS_FORWARD_COMMON_RESULTNODE_TO_BASE \
  NS_IMPLEMENT_SIMPLE_RESULTNODE \
  NS_IMETHOD GetIcon(nsIURI** aIcon) \
    { return nsNavHistoryResultNode::GetIcon(aIcon); } \
  NS_IMETHOD GetParent(nsINavHistoryContainerResultNode** aParent) \
    { return nsNavHistoryResultNode::GetParent(aParent); } \
  NS_IMETHOD GetPropertyBag(nsIWritablePropertyBag** aBag) \
    { return nsNavHistoryResultNode::GetPropertyBag(aBag); }

class nsNavHistoryResultNode : public nsINavHistoryResultNode
{
public:
  nsNavHistoryResultNode(const nsACString& aURI, const nsACString& aTitle,
                         PRUint32 aAccessCount, PRTime aTime,
                         const nsACString& aIconURI);
  virtual ~nsNavHistoryResultNode() {}

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NAVHISTORYRESULTNODE_IID)

  NS_DECL_ISUPPORTS
  NS_IMPLEMENT_SIMPLE_RESULTNODE
  NS_IMETHOD GetIcon(nsIURI** aIcon);
  NS_IMETHOD GetParent(nsINavHistoryContainerResultNode** aParent);
  NS_IMETHOD GetPropertyBag(nsIWritablePropertyBag** aBag);
  NS_IMETHOD GetType(PRUint32* type)
    { *type = nsNavHistoryResultNode::RESULT_TYPE_URI; return NS_OK; }
  NS_IMETHOD GetUri(nsACString& aURI)
    { aURI = mURI; return NS_OK; }

  virtual void OnRemoving();

public:

  nsNavHistoryResult* GetResult();
  nsNavHistoryQueryOptions* GetGeneratingOptions();

  // These functions test the type. We don't use a virtual function since that
  // would take a vtable slot for every one of (potentially very many) nodes.
  // Note that GetType() already has a vtable slot because its on the iface.
  PRBool IsTypeContainer(PRUint32 type) {
    return (type == nsINavHistoryResultNode::RESULT_TYPE_HOST ||
            type == nsINavHistoryResultNode::RESULT_TYPE_REMOTE_CONTAINER ||
            type == nsINavHistoryResultNode::RESULT_TYPE_QUERY ||
            type == nsINavHistoryResultNode::RESULT_TYPE_FOLDER ||
            type == nsINavHistoryResultNode::RESULT_TYPE_DAY);
  }
  PRBool IsContainer() {
    PRUint32 type;
    GetType(&type);
    return IsTypeContainer(type);
  }
  static PRBool IsTypeQuerySubcontainer(PRUint32 type) {
    // Tests containers that are inside queries that really belong to the query
    // itself, and is used when recursively updating a query. This currently
    // includes only host containers, but may be extended to support things
    // like days or other criteria. It doesn't include other queries and folders.
    return (type == nsINavHistoryResultNode::RESULT_TYPE_HOST ||
            type == nsINavHistoryResultNode::RESULT_TYPE_DAY);
  }
  PRBool IsQuerySubcontainer() {
    PRUint32 type;
    GetType(&type);
    return IsTypeQuerySubcontainer(type);
  }
  static PRBool IsTypeURI(PRUint32 type) {
    return (type == nsINavHistoryResultNode::RESULT_TYPE_URI ||
            type == nsINavHistoryResultNode::RESULT_TYPE_VISIT ||
            type == nsINavHistoryResultNode::RESULT_TYPE_FULL_VISIT);
  }
  PRBool IsURI() {
    PRUint32 type;
    GetType(&type);
    return IsTypeURI(type);
  }
  static PRBool IsTypeVisit(PRUint32 type) {
    return (type == nsINavHistoryResultNode::RESULT_TYPE_VISIT ||
            type == nsINavHistoryResultNode::RESULT_TYPE_FULL_VISIT);
  }
  PRBool IsVisit() {
    PRUint32 type;
    GetType(&type);
    return IsTypeVisit(type);
  }
  static PRBool IsTypeFolder(PRUint32 type) {
    return (type == nsINavHistoryResultNode::RESULT_TYPE_FOLDER);
  }
  PRBool IsFolder() {
    PRUint32 type;
    GetType(&type);
    return IsTypeFolder(type);
  }
  static PRBool IsTypeQuery(PRUint32 type) {
    return (type == nsINavHistoryResultNode::RESULT_TYPE_QUERY);
  }
  PRBool IsQuery() {
    PRUint32 type;
    GetType(&type);
    return IsTypeQuery(type);
  }
  PRBool IsSeparator() {
    PRUint32 type;
    GetType(&type);
    return (type == nsINavHistoryResultNode::RESULT_TYPE_SEPARATOR);
  }
  nsNavHistoryContainerResultNode* GetAsContainer() {
    NS_ASSERTION(IsContainer(), "Not a container");
    return NS_REINTERPRET_CAST(nsNavHistoryContainerResultNode*, this);
  }
  nsNavHistoryVisitResultNode* GetAsVisit() {
    NS_ASSERTION(IsVisit(), "Not a visit");
    return NS_REINTERPRET_CAST(nsNavHistoryVisitResultNode*, this);
  }
  nsNavHistoryFolderResultNode* GetAsFolder() {
    NS_ASSERTION(IsFolder(), "Not a folder");
    return NS_REINTERPRET_CAST(nsNavHistoryFolderResultNode*, this);
  }
  nsNavHistoryQueryResultNode* GetAsQuery() {
    NS_ASSERTION(IsQuery(), "Not a query");
    return NS_REINTERPRET_CAST(nsNavHistoryQueryResultNode*, this);
  }

  nsNavHistoryContainerResultNode* mParent;
  nsCString mURI; // not necessarily valid for containers, call GetUri
  nsCString mTitle;
  PRUint32 mAccessCount;
  PRInt64 mTime;
  nsCString mFaviconURI;
  PRInt32 mBookmarkIndex;
  PRInt64 mBookmarkId;

  // The indent level of this node. The root node will have a value of -1.  The
  // root's children will have a value of 0, and so on.
  PRInt32 mIndentLevel;

  // Value used by the view for whatever it wants. For the built-in tree view,
  // this is the index into the result's mVisibleElements list of this element.
  // This is -1 if it is invalid. For items, >= 0 can be used to determine if
  // the node is visible in the list or not. For folders, call IsVisible, since
  // they can be the root node which is not itself visible, but its children
  // are.
  PRInt32 mViewIndex;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsNavHistoryResultNode, NS_NAVHISTORYRESULTNODE_IID)

// nsNavHistoryVisitResultNode

#define NS_IMPLEMENT_VISITRESULT \
  NS_IMETHOD GetUri(nsACString& aURI) { aURI = mURI; return NS_OK; } \
  NS_IMETHOD GetSessionId(PRInt64* aSessionId) \
    { *aSessionId = mSessionId; return NS_OK; }

class nsNavHistoryVisitResultNode : public nsNavHistoryResultNode,
                                    public nsINavHistoryVisitResultNode
{
public:
  nsNavHistoryVisitResultNode(const nsACString& aURI, const nsACString& aTitle,
                              PRUint32 aAccessCount, PRTime aTime,
                              const nsACString& aIconURI, PRInt64 aSession);

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_COMMON_RESULTNODE_TO_BASE
  NS_IMETHOD GetType(PRUint32* type)
    { *type = nsNavHistoryResultNode::RESULT_TYPE_VISIT; return NS_OK; }
  NS_IMPLEMENT_VISITRESULT

public:

  PRInt64 mSessionId;
};


// nsNavHistoryFullVisitResultNode

#define NS_IMPLEMENT_FULLVISITRESULT \
  NS_IMPLEMENT_VISITRESULT \
  NS_IMETHOD GetVisitId(PRInt64 *aVisitId) \
    { *aVisitId = mVisitId; return NS_OK; } \
  NS_IMETHOD GetReferringVisitId(PRInt64 *aReferringVisitId) \
    { *aReferringVisitId = mReferringVisitId; return NS_OK; } \
  NS_IMETHOD GetTransitionType(PRInt32 *aTransitionType) \
    { *aTransitionType = mTransitionType; return NS_OK; }

class nsNavHistoryFullVisitResultNode : public nsNavHistoryVisitResultNode,
                                        public nsINavHistoryFullVisitResultNode
{
public:
  nsNavHistoryFullVisitResultNode(
    const nsACString& aURI, const nsACString& aTitle, PRUint32 aAccessCount,
    PRTime aTime, const nsACString& aIconURI, PRInt64 aSession,
    PRInt64 aVisitId, PRInt64 aReferringVisitId, PRInt32 aTransitionType);

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_COMMON_RESULTNODE_TO_BASE
  NS_IMETHOD GetType(PRUint32* type)
    { *type = nsNavHistoryResultNode::RESULT_TYPE_FULL_VISIT; return NS_OK; }
  NS_IMPLEMENT_FULLVISITRESULT

public:
  PRInt64 mVisitId;
  PRInt64 mReferringVisitId;
  PRInt32 mTransitionType;
};


// nsNavHistoryContainerResultNode
//
//    This is the base class for all nodes that can have children. It is
//    overridden for nodes that are dynamically populated such as queries and
//    folders. It is used directly for simple containers such as host groups
//    in history views.

// derived classes each provide their own implementation of has children and
// forward the rest to us using this macro
#define NS_FORWARD_CONTAINERNODE_EXCEPT_HASCHILDREN_AND_READONLY \
  NS_IMETHOD GetContainerOpen(PRBool *aContainerOpen) \
    { return nsNavHistoryContainerResultNode::GetContainerOpen(aContainerOpen); } \
  NS_IMETHOD SetContainerOpen(PRBool aContainerOpen) \
    { return nsNavHistoryContainerResultNode::SetContainerOpen(aContainerOpen); } \
  NS_IMETHOD GetChildCount(PRUint32 *aChildCount) \
    { return nsNavHistoryContainerResultNode::GetChildCount(aChildCount); } \
  NS_IMETHOD GetChild(PRUint32 index, nsINavHistoryResultNode **_retval) \
    { return nsNavHistoryContainerResultNode::GetChild(index, _retval); } \
  NS_IMETHOD GetRemoteContainerType(nsACString& aRemoteContainerType) \
    { return nsNavHistoryContainerResultNode::GetRemoteContainerType(aRemoteContainerType); }
/* Untested container API functions
  NS_IMETHOD AppendURINode(const nsACString& aURI, const nsACString& aTitle, PRUint32 aAccessCount, PRTime aTime, const nsACString& aIconURI, nsINavHistoryResultNode **_retval) \
    { return nsNavHistoryContainerResultNode::AppendURINode(aURI, aTitle, aAccessCount, aTime, aIconURI, _retval); } \
  NS_IMETHOD AppendVisitNode(const nsACString& aURI, const nsACString & aTitle, PRUint32 aAccessCount, PRTime aTime, const nsACString & aIconURI, PRInt64 aSession, nsINavHistoryVisitResultNode **_retval) \
    { return nsNavHistoryContainerResultNode::AppendVisitNode(aURI, aTitle, aAccessCount, aTime, aIconURI, aSession, _retval); } \
  NS_IMETHOD AppendFullVisitNode(const nsACString& aURI, const nsACString & aTitle, PRUint32 aAccessCount, PRTime aTime, const nsACString & aIconURI, PRInt64 aSession, PRInt64 aVisitId, PRInt64 aReferringVisitId, PRInt32 aTransitionType, nsINavHistoryFullVisitResultNode **_retval) \
    { return nsNavHistoryContainerResultNode::AppendFullVisitNode(aURI, aTitle, aAccessCount, aTime, aIconURI, aSession, aVisitId, aReferringVisitId, aTransitionType, _retval); } \
  NS_IMETHOD AppendContainerNode(const nsACString & aTitle, const nsACString & aIconURI, PRUint32 aContainerType, const nsACString & aRemoteContainerType, nsINavHistoryContainerResultNode **_retval) \
    { return nsNavHistoryContainerResultNode::AppendContainerNode(aTitle, aIconURI, aContainerType, aRemoteContainerType, _retval); } \
  NS_IMETHOD AppendQueryNode(const nsACString& aQueryURI, const nsACString & aTitle, const nsACString & aIconURI, nsINavHistoryQueryResultNode **_retval) \
    { return nsNavHistoryContainerResultNode::AppendQueryNode(aQueryURI, aTitle, aIconURI, _retval); } \
  NS_IMETHOD AppendFolderNode(PRInt64 aFolderId, nsINavHistoryFolderResultNode **_retval) \
    { return nsNavHistoryContainerResultNode::AppendFolderNode(aFolderId, _retval); } \
  NS_IMETHOD ClearContents() \
    { return nsNavHistoryContainerResultNode::ClearContents(); }
*/

#define NS_NAVHISTORYCONTAINERRESULTNODE_IID \
  { 0x6e3bf8d3, 0x22aa, 0x4065, { 0x86, 0xbc, 0x37, 0x46, 0xb5, 0xb3, 0x2c, 0xe8 } }

class nsNavHistoryContainerResultNode : public nsNavHistoryResultNode,
                                        public nsINavHistoryContainerResultNode
{
public:
  nsNavHistoryContainerResultNode(
    const nsACString& aURI, const nsACString& aTitle,
    const nsACString& aIconURI, PRUint32 aContainerType,
    PRBool aReadOnly, const nsACString& aRemoteContainerType);

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NAVHISTORYCONTAINERRESULTNODE_IID)

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_COMMON_RESULTNODE_TO_BASE
  NS_IMETHOD GetType(PRUint32* type)
    { *type = mContainerType; return NS_OK; }
  NS_IMETHOD GetUri(nsACString& aURI)
    { aURI = mURI; return NS_OK; }
  NS_DECL_NSINAVHISTORYCONTAINERRESULTNODE

public:

  virtual void OnRemoving();

  PRBool AreChildrenVisible();

  // overridded by descendents to populate
  virtual nsresult OpenContainer();
  nsresult CloseContainer(PRBool aUpdateView = PR_TRUE);

  // this points to the result that owns this container. All containers have
  // their result pointer set so we can quickly get to the result without having
  // to walk the tree. Yet, this also saves us from storing a million pointers
  // for every leaf node to the result.
  nsNavHistoryResult* mResult;

  // for example, RESULT_TYPE_HOST. Query and Folder results override GetType
  // so this is not used, but is still kept in sync.
  PRUint32 mContainerType;

  // when there are children, this stores the open state in the tree
  // this is set to the default in the constructor
  PRBool mExpanded;

  // Filled in by the result type generator in nsNavHistory
  nsCOMArray<nsNavHistoryResultNode> mChildren;

  PRBool mChildrenReadOnly;

  // ID of a remote container interface that we can use GetService to get.
  // This is empty to indicate there is no remote container service for this
  // container (the common case).
  nsCString mRemoteContainerType;

  void FillStats();
  void ReverseUpdateStats(PRInt32 aAccessCountChange);

  // sorting
  typedef nsCOMArray<nsNavHistoryResultNode>::nsCOMArrayComparatorFunc SortComparator;
  virtual PRUint32 GetSortType();
  static SortComparator GetSortingComparator(PRUint32 aSortType);
  virtual void RecursiveSort(nsICollation* aCollation,
                             SortComparator aComparator);
  PRUint32 FindInsertionPoint(nsNavHistoryResultNode* aNode, SortComparator aComparator);
  PRBool DoesChildNeedResorting(PRUint32 aIndex, SortComparator aComparator);

  PR_STATIC_CALLBACK(int) SortComparison_Bookmark(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_TitleLess(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_TitleGreater(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_DateLess(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_DateGreater(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_URILess(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_URIGreater(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_VisitCountLess(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_VisitCountGreater(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);

  // finding children: THESE DO NOT ADDREF
  nsNavHistoryResultNode* FindChildURI(nsIURI* aURI, PRUint32* aNodeIndex)
  {
    nsCAutoString spec;
    if (NS_FAILED(aURI->GetSpec(spec)))
      return PR_FALSE;
    return FindChildURI(spec, aNodeIndex);
  }
  nsNavHistoryResultNode* FindChildURI(const nsACString& aSpec,
                                       PRUint32* aNodeIndex);
  nsNavHistoryFolderResultNode* FindChildFolder(PRInt64 aFolderId,
                                                PRUint32* aNodeIndex);
  nsNavHistoryContainerResultNode* FindChildContainerByName(const nsACString& aTitle,
                                                            PRUint32* aNodeIndex);
  // returns the index of the given node, -1 if not found
  PRInt32 FindChild(nsNavHistoryResultNode* aNode)
    { return mChildren.IndexOf(aNode); }

  nsresult InsertChildAt(nsNavHistoryResultNode* aNode, PRInt32 aIndex,
                         PRBool aIsTemporary = PR_FALSE);
  nsresult InsertSortedChild(nsNavHistoryResultNode* aNode,
                             PRBool aIsTemporary = PR_FALSE);
  void MergeResults(nsCOMArray<nsNavHistoryResultNode>* aNodes);
  nsresult ReplaceChildURIAt(PRUint32 aIndex, nsNavHistoryResultNode* aNode);
  nsresult RemoveChildAt(PRInt32 aIndex, PRBool aIsTemporary = PR_FALSE);

  PRBool CanRemoteContainersChange();

  void RecursiveFindURIs(PRBool aOnlyOne,
                         nsNavHistoryContainerResultNode* aContainer,
                         const nsCString& aSpec,
                         nsCOMArray<nsNavHistoryResultNode>* aMatches);
  void UpdateURIs(PRBool aRecursive, PRBool aOnlyOne, PRBool aUpdateSort,
                  const nsCString& aSpec,
                  void (*aCallback)(nsNavHistoryResultNode*,void*),
                  void* aClosure);
  nsresult ChangeTitles(nsIURI* aURI, const nsACString& aNewTitle,
                        PRBool aRecursive, PRBool aOnlyOne);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsNavHistoryContainerResultNode,
                              NS_NAVHISTORYCONTAINERRESULTNODE_IID)

// nsNavHistoryQueryResultNode
//
//    Overridden container type for complex queries over history and/or
//    bookmarks. This keeps itself in sync by listening to history and
//    bookmark notifications.

class nsNavHistoryQueryResultNode : public nsNavHistoryContainerResultNode,
                                    public nsINavHistoryQueryResultNode
{
public:
  nsNavHistoryQueryResultNode(const nsACString& aQueryURI,
                              const nsACString& aTitle,
                              const nsACString& aIconURI);
  nsNavHistoryQueryResultNode(const nsACString& aTitle,
                              const nsACString& aIconURI,
                              const nsCOMArray<nsNavHistoryQuery>& aQueries,
                              nsNavHistoryQueryOptions* aOptions);

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_COMMON_RESULTNODE_TO_BASE
  NS_IMETHOD GetType(PRUint32* type)
    { *type = nsNavHistoryResultNode::RESULT_TYPE_QUERY; return NS_OK; }
  NS_IMETHOD GetUri(nsACString& aURI); // does special lazy creation
  NS_FORWARD_CONTAINERNODE_EXCEPT_HASCHILDREN_AND_READONLY
  NS_IMETHOD GetHasChildren(PRBool* aHasChildren);
  NS_IMETHOD GetChildrenReadOnly(PRBool *aChildrenReadOnly)
    { return nsNavHistoryContainerResultNode::GetChildrenReadOnly(aChildrenReadOnly); }
  NS_DECL_NSINAVHISTORYQUERYRESULTNODE

  PRBool CanExpand();

  virtual nsresult OpenContainer();

  NS_DECL_BOOKMARK_HISTORY_OBSERVER
  virtual void OnRemoving();

public:
  // this constructs lazily mURI from mQueries and mOptions, call
  // VerifyQueriesSerialized either this or mQueries/mOptions should be valid
  nsresult VerifyQueriesSerialized();

  // these may be constructed lazily from mURI, call VerifyQueriesParsed
  // either this or mURI should be valid
  nsCOMArray<nsNavHistoryQuery> mQueries;
  nsCOMPtr<nsNavHistoryQueryOptions> mOptions;
  PRUint32 mLiveUpdate; // one of QUERYUPDATE_* in nsNavHistory.h
  PRBool mHasSearchTerms;
  nsresult VerifyQueriesParsed();

  // this indicates whether the query contents are valid, they don't go away
  // after the container is closed until a notification comes in
  PRBool mContentsValid;

  PRBool mBatchInProgress;

  nsresult FillChildren();
  void ClearChildren(PRBool unregister);
  nsresult Refresh();

  virtual PRUint32 GetSortType();
};


// nsNavHistoryFolderResultNode
//
//    Overridden container type for bookmark folders. It will keep the contents
//    of the folder in sync with the bookmark service.

class nsNavHistoryFolderResultNode : public nsNavHistoryContainerResultNode,
                                     public nsINavHistoryFolderResultNode
{
public:
  nsNavHistoryFolderResultNode(const nsACString& aTitle,
                               nsNavHistoryQueryOptions* options,
                               PRInt64 aFolderId,
                               const nsACString& aRemoteContainerType);

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_COMMON_RESULTNODE_TO_BASE
  NS_IMETHOD GetType(PRUint32* type)
    { *type = nsNavHistoryResultNode::RESULT_TYPE_FOLDER; return NS_OK; }
  NS_IMETHOD GetUri(nsACString& aURI);
  NS_FORWARD_CONTAINERNODE_EXCEPT_HASCHILDREN_AND_READONLY
  NS_IMETHOD GetHasChildren(PRBool* aHasChildren);
  NS_IMETHOD GetChildrenReadOnly(PRBool *aChildrenReadOnly);
  NS_DECL_NSINAVHISTORYQUERYRESULTNODE

  NS_IMETHOD GetFolderId(PRInt64* aFolderId)
    { *aFolderId = mFolderId; return NS_OK; }

  virtual nsresult OpenContainer();

  // This object implements a bookmark observer interface without deriving from
  // the bookmark observers. This is called from the result's actual observer
  // and it knows all observers are FolderResultNodes
  NS_DECL_NSINAVBOOKMARKOBSERVER

  virtual void OnRemoving();

public:

  // this indicates whether the folder contents are valid, they don't go away
  // after the container is closed until a notification comes in
  PRBool mContentsValid;

  nsCOMPtr<nsNavHistoryQueryOptions> mOptions;
  PRInt64 mFolderId;

  nsresult FillChildren();
  void ClearChildren(PRBool aUnregister);
  nsresult Refresh();

  PRBool StartIncrementalUpdate();
  void ReindexRange(PRInt32 aStartIndex, PRInt32 aEndIndex, PRInt32 aDelta);

  nsNavHistoryResultNode* FindChildURIById(PRInt64 aBookmarkId,
                                           PRUint32* aNodeIndex);
};

// nsNavHistorySeparatorResultNode
//
// Separator result nodes do not hold any data.
class nsNavHistorySeparatorResultNode : public nsNavHistoryResultNode
{
public:
  nsNavHistorySeparatorResultNode();

  NS_IMETHOD GetType(PRUint32* type)
    { *type = nsNavHistoryResultNode::RESULT_TYPE_SEPARATOR; return NS_OK; }
};


// nsNavHistoryResultTreeViewer
//

class nsNavHistoryResultTreeViewer : public nsINavHistoryResultTreeViewer,
                                     public nsITreeView
{
public:
  nsNavHistoryResultTreeViewer();
  virtual ~nsNavHistoryResultTreeViewer() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSINAVHISTORYRESULTVIEWER
  NS_DECL_NSINAVHISTORYRESULTTREEVIEWER
  NS_DECL_NSITREEVIEW

protected:
  nsRefPtr<nsNavHistoryResult> mResult;
  nsCOMPtr<nsITreeBoxObject> mTree; // will be null when no tree attached
  nsCOMPtr<nsITreeSelection> mSelection; // may be null

  PRBool mCollapseDuplicates;

  // This value indicates whether we should try to compute session boundaries.
  // It is cached so we don't have to compute it every time we want to get a
  // row style.
  PRBool mShowSessions;
  void ComputeShowSessions();
  enum SessionStatus { Session_None, Session_Start, Session_Continue };
  SessionStatus GetRowSessionStatus(PRInt32 row);

  // This list is used to map rows to nodes.
  typedef nsTArray< nsCOMPtr<nsNavHistoryResultNode> > VisibleList;
  VisibleList mVisibleElements;
  nsresult BuildVisibleList();
  nsresult BuildVisibleSection(nsNavHistoryContainerResultNode* aContainer,
                               VisibleList* aVisible,
                               PRUint32 aVisibleStartIndex);
  PRUint32 CountVisibleRowsForItem(nsNavHistoryResultNode* aNode);
  nsresult RefreshVisibleSection(nsNavHistoryContainerResultNode* aContainer);

  PRBool CanCollapseDuplicates(nsNavHistoryResultNode* aTop,
                               nsNavHistoryResultNode* aNext,
                               PRUint32* aShowThisOne);

  // external observers
  nsMaybeWeakPtrArray<nsINavHistoryResultViewObserver> mObservers;

  nsresult FinishInit();

  // columns
  enum ColumnType { Column_Unknown = -1, Column_Title, Column_URI, Column_Date,
                    Column_VisitCount };
  ColumnType GetColumnType(nsITreeColumn* col);
  ColumnType SortTypeToColumnType(PRUint32 aSortType,
                                  PRBool* aDescending = nsnull);

  nsresult FormatFriendlyTime(PRTime aTime, nsAString& aResult);
};

#endif // nsNavHistoryResult_h_
