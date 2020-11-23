/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The definitions of objects that make up a history query result set. This file
 * should only be included by nsNavHistory.h, include that if you want these
 * classes.
 */

#ifndef nsNavHistoryResult_h_
#define nsNavHistoryResult_h_

#include "INativePlacesEventCallback.h"
#include "nsTArray.h"
#include "nsMaybeWeakPtr.h"
#include "nsInterfaceHashtable.h"
#include "nsDataHashtable.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/storage.h"
#include "Helpers.h"

class nsNavHistory;
class nsNavHistoryQuery;
class nsNavHistoryQueryOptions;

class nsNavHistoryContainerResultNode;
class nsNavHistoryFolderResultNode;
class nsNavHistoryQueryResultNode;

/**
 * hashkey wrapper using int64_t KeyType
 *
 * @see nsTHashtable::EntryType for specification
 *
 * This just truncates the 64-bit int to a 32-bit one for using a hash number.
 * It is used for bookmark folder IDs, which should be way less than 2^32.
 */
class nsTrimInt64HashKey : public PLDHashEntryHdr {
 public:
  typedef const int64_t& KeyType;
  typedef const int64_t* KeyTypePointer;

  explicit nsTrimInt64HashKey(KeyTypePointer aKey) : mValue(*aKey) {}
  nsTrimInt64HashKey(const nsTrimInt64HashKey& toCopy)
      : mValue(toCopy.mValue) {}
  ~nsTrimInt64HashKey() = default;

  KeyType GetKey() const { return mValue; }
  bool KeyEquals(KeyTypePointer aKey) const { return *aKey == mValue; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return static_cast<uint32_t>((*aKey) & UINT32_MAX);
  }
  enum { ALLOW_MEMMOVE = true };

 private:
  const int64_t mValue;
};

// Declare methods for implementing nsINavBookmarkObserver
// and nsINavHistoryObserver (some methods, such as BeginUpdateBatch overlap)
#define NS_DECL_BOOKMARK_HISTORY_OBSERVER_BASE(...)                    \
  NS_DECL_NSINAVBOOKMARKOBSERVER                                       \
  NS_IMETHOD OnTitleChanged(nsIURI* aURI, const nsAString& aPageTitle, \
                            const nsACString& aGUID) __VA_ARGS__;      \
  NS_IMETHOD OnFrecencyChanged(nsIURI* aURI, int32_t aNewFrecency,     \
                               const nsACString& aGUID, bool aHidden,  \
                               PRTime aLastVisitDate) __VA_ARGS__;     \
  NS_IMETHOD OnManyFrecenciesChanged() __VA_ARGS__;                    \
  NS_IMETHOD OnDeleteURI(nsIURI* aURI, const nsACString& aGUID,        \
                         uint16_t aReason) __VA_ARGS__;                \
  NS_IMETHOD OnClearHistory() __VA_ARGS__;                             \
  NS_IMETHOD OnPageChanged(nsIURI* aURI, uint32_t aChangedAttribute,   \
                           const nsAString& aNewValue,                 \
                           const nsACString& aGUID) __VA_ARGS__;       \
  NS_IMETHOD OnDeleteVisits(nsIURI* aURI, bool aPartialRemoval,        \
                            const nsACString& aGUID, uint16_t aReason, \
                            uint32_t aTransitionType) __VA_ARGS__;

// The internal version is used by query nodes.
#define NS_DECL_BOOKMARK_HISTORY_OBSERVER_INTERNAL \
  NS_DECL_BOOKMARK_HISTORY_OBSERVER_BASE()

// The external version is used by results.
#define NS_DECL_BOOKMARK_HISTORY_OBSERVER_EXTERNAL(...) \
  NS_DECL_BOOKMARK_HISTORY_OBSERVER_BASE(__VA_ARGS__)

// nsNavHistoryResult
//
//    nsNavHistory creates this object and fills in mChildren (by getting
//    it through GetTopLevel()). Then FilledAllResults() is called to finish
//    object initialization.

#define NS_NAVHISTORYRESULT_IID                      \
  {                                                  \
    0x455d1d40, 0x1b9b, 0x40e6, {                    \
      0xa6, 0x41, 0x8b, 0xb7, 0xe8, 0x82, 0x23, 0x87 \
    }                                                \
  }

class nsNavHistoryResult final
    : public nsSupportsWeakReference,
      public nsINavHistoryResult,
      public nsINavBookmarkObserver,
      public nsINavHistoryObserver,
      public mozilla::places::INativePlacesEventCallback {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NAVHISTORYRESULT_IID)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSINAVHISTORYRESULT
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsNavHistoryResult,
                                           nsINavHistoryResult)
  NS_DECL_BOOKMARK_HISTORY_OBSERVER_EXTERNAL(override)

  void AddHistoryObserver(nsNavHistoryQueryResultNode* aNode);
  void AddBookmarkFolderObserver(nsNavHistoryFolderResultNode* aNode,
                                 int64_t aFolder);
  void AddAllBookmarksObserver(nsNavHistoryQueryResultNode* aNode);
  void AddMobilePrefsObserver(nsNavHistoryQueryResultNode* aNode);
  void RemoveHistoryObserver(nsNavHistoryQueryResultNode* aNode);
  void RemoveBookmarkFolderObserver(nsNavHistoryFolderResultNode* aNode,
                                    int64_t aFolder);
  void RemoveAllBookmarksObserver(nsNavHistoryQueryResultNode* aNode);
  void RemoveMobilePrefsObserver(nsNavHistoryQueryResultNode* aNode);
  void StopObserving();

  nsresult OnVisit(nsIURI* aURI, int64_t aVisitId, PRTime aTime,
                   uint32_t aTransitionType, const nsACString& aGUID,
                   bool aHidden, uint32_t aVisitCount,
                   const nsAString& aLastKnownTitle);

 public:
  explicit nsNavHistoryResult(nsNavHistoryContainerResultNode* mRoot,
                              const RefPtr<nsNavHistoryQuery>& aQuery,
                              const RefPtr<nsNavHistoryQueryOptions>& aOptions);

  RefPtr<nsNavHistoryContainerResultNode> mRootNode;

  RefPtr<nsNavHistoryQuery> mQuery;
  RefPtr<nsNavHistoryQueryOptions> mOptions;

  // One of nsNavHistoryQueryOptions.SORY_BY_* This is initialized to
  // mOptions.sortingMode, but may be overridden if the user clicks on one of
  // the columns.
  uint16_t mSortingMode;
  // If root node is closed and we try to apply a sortingMode, it would not
  // work.  So we will apply it when the node will be reopened and populated.
  // This var states the fact we need to apply sortingMode in such a situation.
  bool mNeedsToApplySortingMode;

  // node observers
  bool mIsHistoryObserver;
  bool mIsBookmarkFolderObserver;
  bool mIsAllBookmarksObserver;
  bool mIsMobilePrefObserver;

  typedef nsTArray<RefPtr<nsNavHistoryQueryResultNode> > QueryObserverList;
  QueryObserverList mHistoryObservers;
  QueryObserverList mAllBookmarksObservers;
  QueryObserverList mMobilePrefObservers;

  typedef nsTArray<RefPtr<nsNavHistoryFolderResultNode> > FolderObserverList;
  nsDataHashtable<nsTrimInt64HashKey, FolderObserverList*>
      mBookmarkFolderObservers;
  FolderObserverList* BookmarkFolderObserversForId(int64_t aFolderId,
                                                   bool aCreate);

  typedef nsTArray<RefPtr<nsNavHistoryContainerResultNode> >
      ContainerObserverList;

  void RecursiveExpandCollapse(nsNavHistoryContainerResultNode* aContainer,
                               bool aExpand);

  void InvalidateTree();

  bool mBatchInProgress;

  nsMaybeWeakPtrArray<nsINavHistoryResultObserver> mObservers;
  bool mSuppressNotifications;

  ContainerObserverList mRefreshParticipants;
  void requestRefresh(nsNavHistoryContainerResultNode* aContainer);

  void HandlePlacesEvent(const PlacesEventSequence& aEvents) override;

  void OnMobilePrefChanged();

  static void OnMobilePrefChangedCallback(const char* prefName, void* self);

 protected:
  virtual ~nsNavHistoryResult();
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsNavHistoryResult, NS_NAVHISTORYRESULT_IID)

// nsNavHistoryResultNode
//
//    This is the base class for every node in a result set. The result itself
//    is a node (nsNavHistoryResult inherits from this), as well as every
//    leaf and branch on the tree.

#define NS_NAVHISTORYRESULTNODE_IID                  \
  {                                                  \
    0x54b61d38, 0x57c1, 0x11da, {                    \
      0x95, 0xb8, 0x00, 0x13, 0x21, 0xc9, 0xf6, 0x9e \
    }                                                \
  }

// These are all the simple getters, they can be used for the result node
// implementation and all subclasses. More complex are GetIcon, GetParent
// (which depends on the definition of container result node), and GetUri
// (which is overridded for lazy construction for some containers).
#define NS_IMPLEMENT_SIMPLE_RESULTNODE                         \
  NS_IMETHOD GetTitle(nsACString& aTitle) override {           \
    aTitle = mTitle;                                           \
    return NS_OK;                                              \
  }                                                            \
  NS_IMETHOD GetAccessCount(uint32_t* aAccessCount) override { \
    *aAccessCount = mAccessCount;                              \
    return NS_OK;                                              \
  }                                                            \
  NS_IMETHOD GetTime(PRTime* aTime) override {                 \
    *aTime = mTime;                                            \
    return NS_OK;                                              \
  }                                                            \
  NS_IMETHOD GetIndentLevel(int32_t* aIndentLevel) override {  \
    *aIndentLevel = mIndentLevel;                              \
    return NS_OK;                                              \
  }                                                            \
  NS_IMETHOD GetBookmarkIndex(int32_t* aIndex) override {      \
    *aIndex = mBookmarkIndex;                                  \
    return NS_OK;                                              \
  }                                                            \
  NS_IMETHOD GetDateAdded(PRTime* aDateAdded) override {       \
    *aDateAdded = mDateAdded;                                  \
    return NS_OK;                                              \
  }                                                            \
  NS_IMETHOD GetLastModified(PRTime* aLastModified) override { \
    *aLastModified = mLastModified;                            \
    return NS_OK;                                              \
  }                                                            \
  NS_IMETHOD GetItemId(int64_t* aId) override {                \
    *aId = mItemId;                                            \
    return NS_OK;                                              \
  }

// This is used by the base classes instead of
// NS_FORWARD_NSINAVHISTORYRESULTNODE(nsNavHistoryResultNode) because they
// need to redefine GetType and GetUri rather than forwarding them. This
// implements all the simple getters instead of forwarding because they are so
// short and we can save a virtual function call.
//
// (GetUri is redefined only by QueryResultNode and FolderResultNode because
// the query might not necessarily be parsed. The rest just return the node's
// buffer.)
#define NS_FORWARD_COMMON_RESULTNODE_TO_BASE                                  \
  NS_IMPLEMENT_SIMPLE_RESULTNODE                                              \
  NS_IMETHOD GetIcon(nsACString& aIcon) override {                            \
    return nsNavHistoryResultNode::GetIcon(aIcon);                            \
  }                                                                           \
  NS_IMETHOD GetParent(nsINavHistoryContainerResultNode** aParent) override { \
    return nsNavHistoryResultNode::GetParent(aParent);                        \
  }                                                                           \
  NS_IMETHOD GetParentResult(nsINavHistoryResult** aResult) override {        \
    return nsNavHistoryResultNode::GetParentResult(aResult);                  \
  }                                                                           \
  NS_IMETHOD GetTags(nsAString& aTags) override {                             \
    return nsNavHistoryResultNode::GetTags(aTags);                            \
  }                                                                           \
  NS_IMETHOD GetPageGuid(nsACString& aPageGuid) override {                    \
    return nsNavHistoryResultNode::GetPageGuid(aPageGuid);                    \
  }                                                                           \
  NS_IMETHOD GetBookmarkGuid(nsACString& aBookmarkGuid) override {            \
    return nsNavHistoryResultNode::GetBookmarkGuid(aBookmarkGuid);            \
  }                                                                           \
  NS_IMETHOD GetVisitId(int64_t* aVisitId) override {                         \
    return nsNavHistoryResultNode::GetVisitId(aVisitId);                      \
  }                                                                           \
  NS_IMETHOD GetFromVisitId(int64_t* aFromVisitId) override {                 \
    return nsNavHistoryResultNode::GetFromVisitId(aFromVisitId);              \
  }                                                                           \
  NS_IMETHOD GetVisitType(uint32_t* aVisitType) override {                    \
    return nsNavHistoryResultNode::GetVisitType(aVisitType);                  \
  }

class nsNavHistoryResultNode : public nsINavHistoryResultNode {
 public:
  nsNavHistoryResultNode(const nsACString& aURI, const nsACString& aTitle,
                         uint32_t aAccessCount, PRTime aTime);

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NAVHISTORYRESULTNODE_IID)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsNavHistoryResultNode)

  NS_IMPLEMENT_SIMPLE_RESULTNODE
  NS_IMETHOD GetIcon(nsACString& aIcon) override;
  NS_IMETHOD GetParent(nsINavHistoryContainerResultNode** aParent) override;
  NS_IMETHOD GetParentResult(nsINavHistoryResult** aResult) override;
  NS_IMETHOD GetType(uint32_t* type) override {
    *type = nsNavHistoryResultNode::RESULT_TYPE_URI;
    return NS_OK;
  }
  NS_IMETHOD GetUri(nsACString& aURI) override {
    aURI = mURI;
    return NS_OK;
  }
  NS_IMETHOD GetTags(nsAString& aTags) override;
  NS_IMETHOD GetPageGuid(nsACString& aPageGuid) override;
  NS_IMETHOD GetBookmarkGuid(nsACString& aBookmarkGuid) override;
  NS_IMETHOD GetVisitId(int64_t* aVisitId) override;
  NS_IMETHOD GetFromVisitId(int64_t* aFromVisitId) override;
  NS_IMETHOD GetVisitType(uint32_t* aVisitType) override;

  virtual void OnRemoving();

  // Called from result's onItemChanged, see also bookmark observer declaration
  // in nsNavHistoryFolderResultNode
  NS_IMETHOD OnItemChanged(int64_t aItemId, const nsACString& aProperty,
                           bool aIsAnnotationProperty, const nsACString& aValue,
                           PRTime aNewLastModified, uint16_t aItemType,
                           int64_t aParentId, const nsACString& aGUID,
                           const nsACString& aParentGUID,
                           const nsACString& aOldValue, uint16_t aSource);

  virtual nsresult OnMobilePrefChanged(bool newValue) { return NS_OK; };

 protected:
  virtual ~nsNavHistoryResultNode() = default;

 public:
  nsNavHistoryResult* GetResult();

  // These functions test the type. We don't use a virtual function since that
  // would take a vtable slot for every one of (potentially very many) nodes.
  // Note that GetType() already has a vtable slot because its on the iface.
  bool IsTypeContainer(uint32_t type) {
    return type == nsINavHistoryResultNode::RESULT_TYPE_QUERY ||
           type == nsINavHistoryResultNode::RESULT_TYPE_FOLDER ||
           type == nsINavHistoryResultNode::RESULT_TYPE_FOLDER_SHORTCUT;
  }
  bool IsContainer() {
    uint32_t type;
    GetType(&type);
    return IsTypeContainer(type);
  }
  static bool IsTypeURI(uint32_t type) {
    return type == nsINavHistoryResultNode::RESULT_TYPE_URI;
  }
  bool IsURI() {
    uint32_t type;
    GetType(&type);
    return IsTypeURI(type);
  }
  static bool IsTypeFolder(uint32_t type) {
    return type == nsINavHistoryResultNode::RESULT_TYPE_FOLDER ||
           type == nsINavHistoryResultNode::RESULT_TYPE_FOLDER_SHORTCUT;
  }
  bool IsFolder() {
    uint32_t type;
    GetType(&type);
    return IsTypeFolder(type);
  }
  static bool IsTypeQuery(uint32_t type) {
    return type == nsINavHistoryResultNode::RESULT_TYPE_QUERY;
  }
  bool IsQuery() {
    uint32_t type;
    GetType(&type);
    return IsTypeQuery(type);
  }
  bool IsSeparator() {
    uint32_t type;
    GetType(&type);
    return type == nsINavHistoryResultNode::RESULT_TYPE_SEPARATOR;
  }
  nsNavHistoryContainerResultNode* GetAsContainer() {
    NS_ASSERTION(IsContainer(), "Not a container");
    return reinterpret_cast<nsNavHistoryContainerResultNode*>(this);
  }
  nsNavHistoryFolderResultNode* GetAsFolder() {
    NS_ASSERTION(IsFolder(), "Not a folder");
    return reinterpret_cast<nsNavHistoryFolderResultNode*>(this);
  }
  nsNavHistoryQueryResultNode* GetAsQuery() {
    NS_ASSERTION(IsQuery(), "Not a query");
    return reinterpret_cast<nsNavHistoryQueryResultNode*>(this);
  }

  RefPtr<nsNavHistoryContainerResultNode> mParent;
  nsCString mURI;  // not necessarily valid for containers, call GetUri
  nsCString mTitle;
  nsString mTags;
  bool mAreTagsSorted;
  uint32_t mAccessCount;
  int64_t mTime;
  int32_t mBookmarkIndex;
  int64_t mItemId;
  int64_t mFolderId;
  int64_t mVisitId;
  int64_t mFromVisitId;
  PRTime mDateAdded;
  PRTime mLastModified;

  // The indent level of this node. The root node will have a value of -1.  The
  // root's children will have a value of 0, and so on.
  int32_t mIndentLevel;

  // Frecency of the page.  Valid only for URI nodes.
  int32_t mFrecency;

  // Hidden status of the page.  Valid only for URI nodes.
  bool mHidden;

  // Transition type used when this node represents a single visit.
  uint32_t mTransitionType;

  // Unique Id of the page.
  nsCString mPageGuid;

  // Unique Id of the bookmark.
  nsCString mBookmarkGuid;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsNavHistoryResultNode,
                              NS_NAVHISTORYRESULTNODE_IID)

// nsNavHistoryContainerResultNode
//
//    This is the base class for all nodes that can have children. It is
//    overridden for nodes that are dynamically populated such as queries and
//    folders. It is used directly for simple containers such as host groups
//    in history views.

// derived classes each provide their own implementation of has children and
// forward the rest to us using this macro
#define NS_FORWARD_CONTAINERNODE_EXCEPT_HASCHILDREN                           \
  NS_IMETHOD GetState(uint16_t* _state) override {                            \
    return nsNavHistoryContainerResultNode::GetState(_state);                 \
  }                                                                           \
  NS_IMETHOD GetContainerOpen(bool* aContainerOpen) override {                \
    return nsNavHistoryContainerResultNode::GetContainerOpen(aContainerOpen); \
  }                                                                           \
  NS_IMETHOD SetContainerOpen(bool aContainerOpen) override {                 \
    return nsNavHistoryContainerResultNode::SetContainerOpen(aContainerOpen); \
  }                                                                           \
  NS_IMETHOD GetChildCount(uint32_t* aChildCount) override {                  \
    return nsNavHistoryContainerResultNode::GetChildCount(aChildCount);       \
  }                                                                           \
  NS_IMETHOD GetChild(uint32_t index, nsINavHistoryResultNode** _retval)      \
      override {                                                              \
    return nsNavHistoryContainerResultNode::GetChild(index, _retval);         \
  }                                                                           \
  NS_IMETHOD GetChildIndex(nsINavHistoryResultNode* aNode, uint32_t* _retval) \
      override {                                                              \
    return nsNavHistoryContainerResultNode::GetChildIndex(aNode, _retval);    \
  }

#define NS_NAVHISTORYCONTAINERRESULTNODE_IID         \
  {                                                  \
    0x6e3bf8d3, 0x22aa, 0x4065, {                    \
      0x86, 0xbc, 0x37, 0x46, 0xb5, 0xb3, 0x2c, 0xe8 \
    }                                                \
  }

class nsNavHistoryContainerResultNode
    : public nsNavHistoryResultNode,
      public nsINavHistoryContainerResultNode {
 public:
  nsNavHistoryContainerResultNode(const nsACString& aURI,
                                  const nsACString& aTitle, PRTime aTime,
                                  uint32_t aContainerType,
                                  nsNavHistoryQueryOptions* aOptions);

  virtual nsresult Refresh();

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NAVHISTORYCONTAINERRESULTNODE_IID)

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsNavHistoryContainerResultNode,
                                           nsNavHistoryResultNode)
  NS_FORWARD_COMMON_RESULTNODE_TO_BASE
  NS_IMETHOD GetType(uint32_t* type) override {
    *type = mContainerType;
    return NS_OK;
  }
  NS_IMETHOD GetUri(nsACString& aURI) override {
    aURI = mURI;
    return NS_OK;
  }
  NS_DECL_NSINAVHISTORYCONTAINERRESULTNODE

 public:
  virtual void OnRemoving() override;

  bool AreChildrenVisible();

  // Overridded by descendents to populate.
  virtual nsresult OpenContainer();
  nsresult CloseContainer(bool aSuppressNotifications = false);

  virtual nsresult OpenContainerAsync();

  // This points to the result that owns this container. All containers have
  // their result pointer set so we can quickly get to the result without having
  // to walk the tree. Yet, this also saves us from storing a million pointers
  // for every leaf node to the result.
  RefPtr<nsNavHistoryResult> mResult;

  // For example, RESULT_TYPE_QUERY. Query and Folder results override GetType
  // so this is not used, but is still kept in sync.
  uint32_t mContainerType;

  // When there are children, this stores the open state in the tree
  // this is set to the default in the constructor.
  bool mExpanded;

  // Filled in by the result type generator in nsNavHistory.
  nsCOMArray<nsNavHistoryResultNode> mChildren;

  // mOriginalOptions is the options object used to _define_ this specific
  // container node. It may differ from mOptions, that is the options used
  // to _fill_ this container node, because mOptions may be modified by
  // the direct parent of this container node, see SetAsParentOfNode. For
  // example, if the parent has excludeItems, options will have it too, even if
  // originally this object was not defined with that option.
  RefPtr<nsNavHistoryQueryOptions> mOriginalOptions;
  RefPtr<nsNavHistoryQueryOptions> mOptions;

  void FillStats();
  // Sets this container as parent of aNode, propagating the appropriate
  // options.
  void SetAsParentOfNode(nsNavHistoryResultNode* aNode);
  nsresult ReverseUpdateStats(int32_t aAccessCountChange);

  // Sorting methods.
  typedef nsCOMArray<nsNavHistoryResultNode>::TComparatorFunc SortComparator;
  virtual uint16_t GetSortType();

  static SortComparator GetSortingComparator(uint16_t aSortType);
  virtual void RecursiveSort(SortComparator aComparator);
  uint32_t FindInsertionPoint(nsNavHistoryResultNode* aNode,
                              SortComparator aComparator, bool* aItemExists);
  bool DoesChildNeedResorting(uint32_t aIndex, SortComparator aComparator);

  static int32_t SortComparison_StringLess(const nsAString& a,
                                           const nsAString& b);

  static int32_t SortComparison_Bookmark(nsNavHistoryResultNode* a,
                                         nsNavHistoryResultNode* b,
                                         void* closure);
  static int32_t SortComparison_TitleLess(nsNavHistoryResultNode* a,
                                          nsNavHistoryResultNode* b,
                                          void* closure);
  static int32_t SortComparison_TitleGreater(nsNavHistoryResultNode* a,
                                             nsNavHistoryResultNode* b,
                                             void* closure);
  static int32_t SortComparison_DateLess(nsNavHistoryResultNode* a,
                                         nsNavHistoryResultNode* b,
                                         void* closure);
  static int32_t SortComparison_DateGreater(nsNavHistoryResultNode* a,
                                            nsNavHistoryResultNode* b,
                                            void* closure);
  static int32_t SortComparison_URILess(nsNavHistoryResultNode* a,
                                        nsNavHistoryResultNode* b,
                                        void* closure);
  static int32_t SortComparison_URIGreater(nsNavHistoryResultNode* a,
                                           nsNavHistoryResultNode* b,
                                           void* closure);
  static int32_t SortComparison_VisitCountLess(nsNavHistoryResultNode* a,
                                               nsNavHistoryResultNode* b,
                                               void* closure);
  static int32_t SortComparison_VisitCountGreater(nsNavHistoryResultNode* a,
                                                  nsNavHistoryResultNode* b,
                                                  void* closure);
  static int32_t SortComparison_DateAddedLess(nsNavHistoryResultNode* a,
                                              nsNavHistoryResultNode* b,
                                              void* closure);
  static int32_t SortComparison_DateAddedGreater(nsNavHistoryResultNode* a,
                                                 nsNavHistoryResultNode* b,
                                                 void* closure);
  static int32_t SortComparison_LastModifiedLess(nsNavHistoryResultNode* a,
                                                 nsNavHistoryResultNode* b,
                                                 void* closure);
  static int32_t SortComparison_LastModifiedGreater(nsNavHistoryResultNode* a,
                                                    nsNavHistoryResultNode* b,
                                                    void* closure);
  static int32_t SortComparison_TagsLess(nsNavHistoryResultNode* a,
                                         nsNavHistoryResultNode* b,
                                         void* closure);
  static int32_t SortComparison_TagsGreater(nsNavHistoryResultNode* a,
                                            nsNavHistoryResultNode* b,
                                            void* closure);
  static int32_t SortComparison_FrecencyLess(nsNavHistoryResultNode* a,
                                             nsNavHistoryResultNode* b,
                                             void* closure);
  static int32_t SortComparison_FrecencyGreater(nsNavHistoryResultNode* a,
                                                nsNavHistoryResultNode* b,
                                                void* closure);

  // finding children: THESE DO NOT ADDREF
  nsNavHistoryResultNode* FindChildURI(const nsACString& aSpec,
                                       uint32_t* aNodeIndex);
  // returns the index of the given node, -1 if not found
  int32_t FindChild(nsNavHistoryResultNode* aNode) {
    return mChildren.IndexOf(aNode);
  }

  nsNavHistoryResultNode* FindChildByGuid(const nsACString& guid,
                                          int32_t* nodeIndex);

  nsresult InsertChildAt(nsNavHistoryResultNode* aNode, int32_t aIndex);
  nsresult InsertSortedChild(nsNavHistoryResultNode* aNode,
                             bool aIgnoreDuplicates = false);
  bool EnsureItemPosition(uint32_t aIndex);

  nsresult RemoveChildAt(int32_t aIndex);

  void RecursiveFindURIs(bool aOnlyOne,
                         nsNavHistoryContainerResultNode* aContainer,
                         const nsCString& aSpec,
                         nsCOMArray<nsNavHistoryResultNode>* aMatches);
  bool UpdateURIs(bool aRecursive, bool aOnlyOne, bool aUpdateSort,
                  const nsCString& aSpec,
                  nsresult (*aCallback)(nsNavHistoryResultNode*, const void*,
                                        const nsNavHistoryResult*),
                  const void* aClosure);
  nsresult ChangeTitles(nsIURI* aURI, const nsACString& aNewTitle,
                        bool aRecursive, bool aOnlyOne);

 protected:
  virtual ~nsNavHistoryContainerResultNode();

  enum AsyncCanceledState { NOT_CANCELED, CANCELED, CANCELED_RESTART_NEEDED };

  void CancelAsyncOpen(bool aRestart);
  nsresult NotifyOnStateChange(uint16_t aOldState);

  nsCOMPtr<mozIStoragePendingStatement> mAsyncPendingStmt;
  AsyncCanceledState mAsyncCanceledState;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsNavHistoryContainerResultNode,
                              NS_NAVHISTORYCONTAINERRESULTNODE_IID)

// nsNavHistoryQueryResultNode
//
//    Overridden container type for complex queries over history and/or
//    bookmarks. This keeps itself in sync by listening to history and
//    bookmark notifications.

class nsNavHistoryQueryResultNode final
    : public nsNavHistoryContainerResultNode,
      public nsINavHistoryQueryResultNode,
      public nsINavBookmarkObserver {
 public:
  nsNavHistoryQueryResultNode(const nsACString& aTitle, PRTime aTime,
                              const nsACString& aQueryURI,
                              const RefPtr<nsNavHistoryQuery>& aQuery,
                              const RefPtr<nsNavHistoryQueryOptions>& aOptions);

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_COMMON_RESULTNODE_TO_BASE
  NS_IMETHOD GetType(uint32_t* type) override {
    *type = nsNavHistoryResultNode::RESULT_TYPE_QUERY;
    return NS_OK;
  }
  NS_IMETHOD GetUri(nsACString& aURI) override;  // does special lazy creation
  NS_FORWARD_CONTAINERNODE_EXCEPT_HASCHILDREN
  NS_IMETHOD GetHasChildren(bool* aHasChildren) override;
  NS_DECL_NSINAVHISTORYQUERYRESULTNODE

  virtual nsresult OnMobilePrefChanged(bool newValue) override;

  bool CanExpand();
  bool IsContainersQuery();

  virtual nsresult OpenContainer() override;

  NS_DECL_BOOKMARK_HISTORY_OBSERVER_INTERNAL

  nsresult OnItemAdded(int64_t aItemId, int64_t aParentId, int32_t aIndex,
                       uint16_t aItemType, nsIURI* aURI, PRTime aDateAdded,
                       const nsACString& aGUID, const nsACString& aParentGUID,
                       uint16_t aSource);

  nsresult OnItemRemoved(int64_t aItemId, int64_t aParentFolder, int32_t aIndex,
                         uint16_t aItemType, nsIURI* aURI,
                         const nsACString& aGUID, const nsACString& aParentGUID,
                         uint16_t aSource);

  // The internal version has an output aAdded parameter, it is incremented by
  // query nodes when the visited uri belongs to them. If no such query exists,
  // the history result creates a new query node dynamically.
  nsresult OnVisit(nsIURI* aURI, int64_t aVisitId, PRTime aTime,
                   uint32_t aTransitionType, bool aHidden, uint32_t* aAdded);
  virtual void OnRemoving() override;

 public:
  RefPtr<nsNavHistoryQuery> mQuery;
  uint32_t mLiveUpdate;  // one of QUERYUPDATE_* in nsNavHistory.h
  bool mHasSearchTerms;

  // safe options getter, ensures query is parsed
  nsNavHistoryQueryOptions* Options();

  // this indicates whether the query contents are valid, they don't go away
  // after the container is closed until a notification comes in
  bool mContentsValid;

  nsresult FillChildren();
  void ClearChildren(bool unregister);
  nsresult Refresh() override;

  virtual uint16_t GetSortType() override;
  virtual void RecursiveSort(SortComparator aComparator) override;

  nsresult NotifyIfTagsChanged(nsIURI* aURI);

  uint32_t mBatchChanges;

  // Tracks transition type filters.
  nsTArray<uint32_t> mTransitions;

 protected:
  virtual ~nsNavHistoryQueryResultNode();
};

// nsNavHistoryFolderResultNode
//
//    Overridden container type for bookmark folders. It will keep the contents
//    of the folder in sync with the bookmark service.

class nsNavHistoryFolderResultNode final
    : public nsNavHistoryContainerResultNode,
      public nsINavHistoryQueryResultNode,
      public nsINavBookmarkObserver,
      public mozilla::places::WeakAsyncStatementCallback {
 public:
  nsNavHistoryFolderResultNode(const nsACString& aTitle,
                               nsNavHistoryQueryOptions* options,
                               int64_t aFolderId);

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_COMMON_RESULTNODE_TO_BASE
  NS_IMETHOD GetType(uint32_t* type) override {
    if (mTargetFolderItemId != mItemId) {
      *type = nsNavHistoryResultNode::RESULT_TYPE_FOLDER_SHORTCUT;
    } else {
      *type = nsNavHistoryResultNode::RESULT_TYPE_FOLDER;
    }
    return NS_OK;
  }
  NS_IMETHOD GetUri(nsACString& aURI) override;
  NS_FORWARD_CONTAINERNODE_EXCEPT_HASCHILDREN
  NS_IMETHOD GetHasChildren(bool* aHasChildren) override;
  NS_DECL_NSINAVHISTORYQUERYRESULTNODE

  virtual nsresult OpenContainer() override;

  virtual nsresult OpenContainerAsync() override;
  NS_DECL_ASYNCSTATEMENTCALLBACK

  // This object implements a bookmark observer interface. This is called from
  // the result's actual observer and it knows all observers are
  // FolderResultNodes
  NS_DECL_NSINAVBOOKMARKOBSERVER

  nsresult OnItemAdded(int64_t aItemId, int64_t aParentId, int32_t aIndex,
                       uint16_t aItemType, nsIURI* aURI, PRTime aDateAdded,
                       const nsACString& aGUID, const nsACString& aParentGUID,
                       uint16_t aSource);
  nsresult OnItemRemoved(int64_t aItemId, int64_t aParentFolder, int32_t aIndex,
                         uint16_t aItemType, nsIURI* aURI,
                         const nsACString& aGUID, const nsACString& aParentGUID,
                         uint16_t aSource);
  virtual void OnRemoving() override;

  // this indicates whether the folder contents are valid, they don't go away
  // after the container is closed until a notification comes in
  bool mContentsValid;

  // If the node is generated from a place:folder=X query, this is the target
  // folder id and GUID.  For regular folder nodes, they are set to the same
  // values as mItemId and mBookmarkGuid. For more complex queries, they are set
  // to -1/an empty string.
  int64_t mTargetFolderItemId;
  nsCString mTargetFolderGuid;

  nsresult FillChildren();
  void ClearChildren(bool aUnregister);
  nsresult Refresh() override;

  bool StartIncrementalUpdate();
  void ReindexRange(int32_t aStartIndex, int32_t aEndIndex, int32_t aDelta);

  nsNavHistoryResultNode* FindChildById(int64_t aItemId, uint32_t* aNodeIndex);

 protected:
  virtual ~nsNavHistoryFolderResultNode();

 private:
  nsresult OnChildrenFilled();
  void EnsureRegisteredAsFolderObserver();
  nsresult FillChildrenAsync();

  bool mIsRegisteredFolderObserver;
  int32_t mAsyncBookmarkIndex;
};

// nsNavHistorySeparatorResultNode
//
// Separator result nodes do not hold any data.
class nsNavHistorySeparatorResultNode : public nsNavHistoryResultNode {
 public:
  nsNavHistorySeparatorResultNode();

  NS_IMETHOD GetType(uint32_t* type) override {
    *type = nsNavHistoryResultNode::RESULT_TYPE_SEPARATOR;
    return NS_OK;
  }
};

#endif  // nsNavHistoryResult_h_
