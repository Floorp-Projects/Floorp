/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIFileView.h"
#include "nsITreeView.h"
#include "mozilla/ModuleUtils.h"
#include "nsITreeSelection.h"
#include "nsITreeColumns.h"
#include "nsITreeBoxObject.h"
#include "nsIFile.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsPrintfCString.h"
#include "nsIDateTimeFormat.h"
#include "nsQuickSort.h"
#include "nsIAtom.h"
#include "nsIAutoCompleteResult.h"
#include "nsIAutoCompleteSearch.h"
#include "nsISimpleEnumerator.h"
#include "nsAutoPtr.h"
#include "nsIMutableArray.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"

#include "nsWildCard.h"

class nsIDOMDataTransfer;
 
#define NS_FILECOMPLETE_CID { 0xcb60980e, 0x18a5, 0x4a77, \
                            { 0x91, 0x10, 0x81, 0x46, 0x61, 0x4c, 0xa7, 0xf0 } }
#define NS_FILECOMPLETE_CONTRACTID "@mozilla.org/autocomplete/search;1?name=file"

class nsFileResult final : public nsIAutoCompleteResult
{
public:
  // aSearchString is the text typed into the autocomplete widget
  // aSearchParam is the picker's currently displayed directory
  nsFileResult(const nsAString& aSearchString, const nsAString& aSearchParam);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUTOCOMPLETERESULT

  nsTArray<nsString> mValues;
  nsString mSearchString;
  uint16_t mSearchResult;
private:
  ~nsFileResult() {}
};

NS_IMPL_ISUPPORTS(nsFileResult, nsIAutoCompleteResult)

nsFileResult::nsFileResult(const nsAString& aSearchString,
                           const nsAString& aSearchParam):
  mSearchString(aSearchString)
{
  if (aSearchString.IsEmpty())
    mSearchResult = RESULT_IGNORED;
  else {
    int32_t slashPos = mSearchString.RFindChar('/');
    mSearchResult = RESULT_FAILURE;
    nsCOMPtr<nsIFile> directory;
    nsDependentSubstring parent(Substring(mSearchString, 0, slashPos + 1));
    if (!parent.IsEmpty() && parent.First() == '/')
      NS_NewLocalFile(parent, true, getter_AddRefs(directory));
    if (!directory) {
      if (NS_FAILED(NS_NewLocalFile(aSearchParam, true, getter_AddRefs(directory))))
        return;
      if (slashPos > 0)
        directory->AppendRelativePath(Substring(mSearchString, 0, slashPos));
    }
    nsCOMPtr<nsISimpleEnumerator> dirEntries;
    if (NS_FAILED(directory->GetDirectoryEntries(getter_AddRefs(dirEntries))))
      return;
    mSearchResult = RESULT_NOMATCH;
    bool hasMore = false;
    nsDependentSubstring prefix(Substring(mSearchString, slashPos + 1));
    while (NS_SUCCEEDED(dirEntries->HasMoreElements(&hasMore)) && hasMore) {
      nsCOMPtr<nsISupports> nextItem;
      dirEntries->GetNext(getter_AddRefs(nextItem));
      nsCOMPtr<nsIFile> nextFile(do_QueryInterface(nextItem));
      nsAutoString fileName;
      nextFile->GetLeafName(fileName);
      if (StringBeginsWith(fileName, prefix)) {
        fileName.Insert(parent, 0);
        if (mSearchResult == RESULT_NOMATCH && fileName.Equals(mSearchString))
          mSearchResult = RESULT_IGNORED;
        else
          mSearchResult = RESULT_SUCCESS;
        bool isDirectory = false;
        nextFile->IsDirectory(&isDirectory);
        if (isDirectory)
          fileName.Append('/');
        mValues.AppendElement(fileName);
      }
    }
    mValues.Sort();
  }
}

NS_IMETHODIMP nsFileResult::GetSearchString(nsAString & aSearchString)
{
  aSearchString.Assign(mSearchString);
  return NS_OK;
}

NS_IMETHODIMP nsFileResult::GetSearchResult(uint16_t *aSearchResult)
{
  NS_ENSURE_ARG_POINTER(aSearchResult);
  *aSearchResult = mSearchResult;
  return NS_OK;
}

NS_IMETHODIMP nsFileResult::GetDefaultIndex(int32_t *aDefaultIndex)
{
  NS_ENSURE_ARG_POINTER(aDefaultIndex);
  *aDefaultIndex = -1;
  return NS_OK;
}

NS_IMETHODIMP nsFileResult::GetErrorDescription(nsAString & aErrorDescription)
{
  aErrorDescription.Truncate();
  return NS_OK;
}

NS_IMETHODIMP nsFileResult::GetMatchCount(uint32_t *aMatchCount)
{
  NS_ENSURE_ARG_POINTER(aMatchCount);
  *aMatchCount = mValues.Length();
  return NS_OK;
}

NS_IMETHODIMP nsFileResult::GetTypeAheadResult(bool *aTypeAheadResult)
{
  NS_ENSURE_ARG_POINTER(aTypeAheadResult);
  *aTypeAheadResult = false;
  return NS_OK;
}

NS_IMETHODIMP nsFileResult::GetValueAt(int32_t index, nsAString & aValue)
{
  aValue = mValues[index];
  if (aValue.Last() == '/')
    aValue.Truncate(aValue.Length() - 1);
  return NS_OK;
}

NS_IMETHODIMP nsFileResult::GetLabelAt(int32_t index, nsAString & aValue)
{
  return GetValueAt(index, aValue);
}

NS_IMETHODIMP nsFileResult::GetCommentAt(int32_t index, nsAString & aComment)
{
  aComment.Truncate();
  return NS_OK;
}

NS_IMETHODIMP nsFileResult::GetStyleAt(int32_t index, nsAString & aStyle)
{
  if (mValues[index].Last() == '/')
    aStyle.AssignLiteral("directory");
  else
    aStyle.AssignLiteral("file");
  return NS_OK;
}

NS_IMETHODIMP nsFileResult::GetImageAt(int32_t index, nsAString & aImage)
{
  aImage.Truncate();
  return NS_OK;
}
NS_IMETHODIMP nsFileResult::GetFinalCompleteValueAt(int32_t index,
                                                    nsAString & aValue)
{
  return GetValueAt(index, aValue);
}

NS_IMETHODIMP nsFileResult::RemoveValueAt(int32_t rowIndex, bool removeFromDb)
{
  return NS_OK;
}

class nsFileComplete final : public nsIAutoCompleteSearch
{
  ~nsFileComplete() {}
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUTOCOMPLETESEARCH
};

NS_IMPL_ISUPPORTS(nsFileComplete, nsIAutoCompleteSearch)

NS_IMETHODIMP
nsFileComplete::StartSearch(const nsAString& aSearchString,
                            const nsAString& aSearchParam,
                            nsIAutoCompleteResult *aPreviousResult,
                            nsIAutoCompleteObserver *aListener)
{
  NS_ENSURE_ARG_POINTER(aListener);
  RefPtr<nsFileResult> result = new nsFileResult(aSearchString, aSearchParam);
  NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);
  return aListener->OnSearchResult(this, result);
}

NS_IMETHODIMP
nsFileComplete::StopSearch()
{
  return NS_OK;
}

#define NS_FILEVIEW_CID { 0xa5570462, 0x1dd1, 0x11b2, \
                         { 0x9d, 0x19, 0xdf, 0x30, 0xa2, 0x7f, 0xbd, 0xc4 } }

class nsFileView : public nsIFileView,
                   public nsITreeView
{
public:
  nsFileView();
  nsresult Init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIFILEVIEW
  NS_DECL_NSITREEVIEW
  
protected:
  virtual ~nsFileView();
  
  void FilterFiles();
  void ReverseArray(nsTArray<nsCOMPtr<nsIFile> >& aArray);
  void SortArray(nsTArray<nsCOMPtr<nsIFile> >& aArray);
  void SortInternal();

  nsTArray<nsCOMPtr<nsIFile> > mFileList;
  nsTArray<nsCOMPtr<nsIFile> > mDirList;
  nsTArray<nsCOMPtr<nsIFile> > mFilteredFiles;

  nsCOMPtr<nsIFile> mDirectoryPath;
  nsCOMPtr<nsITreeBoxObject> mTree;
  nsCOMPtr<nsITreeSelection> mSelection;
  nsCOMPtr<nsIDateTimeFormat> mDateFormatter;

  int16_t mSortType;
  int32_t mTotalRows;

  nsTArray<char16_t*> mCurrentFilters;

  bool mShowHiddenFiles;
  bool mDirectoryFilter;
  bool mReverseSort;
};

// Factory constructor
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFileComplete)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsFileView, Init)
NS_DEFINE_NAMED_CID(NS_FILECOMPLETE_CID);
NS_DEFINE_NAMED_CID(NS_FILEVIEW_CID);

static const mozilla::Module::CIDEntry kFileViewCIDs[] = {
  { &kNS_FILECOMPLETE_CID, false, nullptr, nsFileCompleteConstructor },
  { &kNS_FILEVIEW_CID, false, nullptr, nsFileViewConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kFileViewContracts[] = {
  { NS_FILECOMPLETE_CONTRACTID, &kNS_FILECOMPLETE_CID },
  { NS_FILEVIEW_CONTRACTID, &kNS_FILEVIEW_CID },
  { nullptr }
};

static const mozilla::Module kFileViewModule = {
  mozilla::Module::kVersion,
  kFileViewCIDs,
  kFileViewContracts
};

NSMODULE_DEFN(nsFileViewModule) = &kFileViewModule;

nsFileView::nsFileView() :
  mSortType(-1),
  mTotalRows(0),
  mShowHiddenFiles(false),
  mDirectoryFilter(false),
  mReverseSort(false)
{
}

nsFileView::~nsFileView()
{
  uint32_t count = mCurrentFilters.Length();
  for (uint32_t i = 0; i < count; ++i)
    free(mCurrentFilters[i]);
}

nsresult
nsFileView::Init()
{
  mDateFormatter = nsIDateTimeFormat::Create();
  if (!mDateFormatter)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

// nsISupports implementation

NS_IMPL_ISUPPORTS(nsFileView, nsITreeView, nsIFileView)

// nsIFileView implementation

NS_IMETHODIMP
nsFileView::SetShowHiddenFiles(bool aShowHidden)
{
  if (aShowHidden != mShowHiddenFiles) {
    mShowHiddenFiles = aShowHidden;

    // This could be better optimized, but since the hidden
    // file functionality is not currently used, this will be fine.
    SetDirectory(mDirectoryPath);
  }
    
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetShowHiddenFiles(bool* aShowHidden)
{
  *aShowHidden = mShowHiddenFiles;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::SetShowOnlyDirectories(bool aOnlyDirs)
{
  if (aOnlyDirs == mDirectoryFilter)
    return NS_OK;

  mDirectoryFilter = aOnlyDirs;
  uint32_t dirCount = mDirList.Length();
  if (mDirectoryFilter) {
    int32_t rowDiff = mTotalRows - dirCount;

    mFilteredFiles.Clear();
    mTotalRows = dirCount;
    if (mTree)
      mTree->RowCountChanged(mTotalRows, -rowDiff);
  } else {
    // Run the filter again to get the file list back
    FilterFiles();

    SortArray(mFilteredFiles);
    if (mReverseSort)
      ReverseArray(mFilteredFiles);

    if (mTree)
      mTree->RowCountChanged(dirCount, mTotalRows - dirCount);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetShowOnlyDirectories(bool* aOnlyDirs)
{
  *aOnlyDirs = mDirectoryFilter;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetSortType(int16_t* aSortType)
{
  *aSortType = mSortType;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetReverseSort(bool* aReverseSort)
{
  *aReverseSort = mReverseSort;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::Sort(int16_t aSortType, bool aReverseSort)
{
  if (aSortType == mSortType) {
    if (aReverseSort == mReverseSort)
      return NS_OK;

    mReverseSort = aReverseSort;
    ReverseArray(mDirList);
    ReverseArray(mFilteredFiles);
  } else {
    mSortType = aSortType;
    mReverseSort = aReverseSort;
    SortInternal();
  }

  if (mTree)
    mTree->Invalidate();

  return NS_OK;
}

NS_IMETHODIMP
nsFileView::SetDirectory(nsIFile* aDirectory)
{
  NS_ENSURE_ARG_POINTER(aDirectory);

  nsCOMPtr<nsISimpleEnumerator> dirEntries;
  aDirectory->GetDirectoryEntries(getter_AddRefs(dirEntries));

  if (!dirEntries) {
    // Couldn't read in the directory, this can happen if the user does not
    // have permission to list it.
    return NS_ERROR_FAILURE;
  }

  mDirectoryPath = aDirectory;
  mFileList.Clear();
  mDirList.Clear();

  bool hasMore = false;

  while (NS_SUCCEEDED(dirEntries->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> nextItem;
    dirEntries->GetNext(getter_AddRefs(nextItem));
    nsCOMPtr<nsIFile> theFile = do_QueryInterface(nextItem);

    bool isDirectory = false;
    if (theFile) {
      theFile->IsDirectory(&isDirectory);

      if (isDirectory) {
        bool isHidden;
        theFile->IsHidden(&isHidden);
        if (mShowHiddenFiles || !isHidden) {
          mDirList.AppendElement(theFile);
        }
      }
      else {
        mFileList.AppendElement(theFile);
      }
    }
  }

  if (mTree) {
    mTree->BeginUpdateBatch();
    mTree->RowCountChanged(0, -mTotalRows);
  }

  FilterFiles();
  SortInternal();

  if (mTree) {
    mTree->EndUpdateBatch();
    mTree->ScrollToRow(0);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFileView::SetFilter(const nsAString& aFilterString)
{
  uint32_t filterCount = mCurrentFilters.Length();
  for (uint32_t i = 0; i < filterCount; ++i)
    free(mCurrentFilters[i]);
  mCurrentFilters.Clear();

  nsAString::const_iterator start, iter, end;
  aFilterString.BeginReading(iter);
  aFilterString.EndReading(end);

  while (true) {
    // skip over delimiters
    while (iter != end && (*iter == ';' || *iter == ' '))
      ++iter;

    if (iter == end)
      break;

    start = iter; // start of a filter

    // we know this is neither ';' nor ' ', skip to next char
    ++iter;

    // find next delimiter or end of string
    while (iter != end && (*iter != ';' && *iter != ' '))
      ++iter;

    char16_t* filter = ToNewUnicode(Substring(start, iter));
    if (!filter)
      return NS_ERROR_OUT_OF_MEMORY;

    if (!mCurrentFilters.AppendElement(filter)) {
      free(filter);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (iter == end)
      break;

    ++iter; // we know this is either ';' or ' ', skip to next char
  }

  if (mTree) {
    mTree->BeginUpdateBatch();
    uint32_t count = mDirList.Length();
    mTree->RowCountChanged(count, count - mTotalRows);
  }

  mFilteredFiles.Clear();

  FilterFiles();

  SortArray(mFilteredFiles);
  if (mReverseSort)
    ReverseArray(mFilteredFiles);

  if (mTree)
    mTree->EndUpdateBatch();

  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetSelectedFiles(nsIArray** aFiles)
{
  *aFiles = nullptr;
  if (!mSelection)
    return NS_OK;

  int32_t numRanges;
  mSelection->GetRangeCount(&numRanges);

  uint32_t dirCount = mDirList.Length();
  nsCOMPtr<nsIMutableArray> fileArray =
    do_CreateInstance(NS_ARRAY_CONTRACTID);
  NS_ENSURE_STATE(fileArray);

  for (int32_t range = 0; range < numRanges; ++range) {
    int32_t rangeBegin, rangeEnd;
    mSelection->GetRangeAt(range, &rangeBegin, &rangeEnd);

    for (int32_t itemIndex = rangeBegin; itemIndex <= rangeEnd; ++itemIndex) {
      nsIFile* curFile = nullptr;

      if (itemIndex < (int32_t) dirCount)
        curFile = mDirList[itemIndex];
      else {
        if (itemIndex < mTotalRows)
          curFile = mFilteredFiles[itemIndex - dirCount];
      }

      if (curFile)
        fileArray->AppendElement(curFile, false);
    }
  }

  fileArray.forget(aFiles);
  return NS_OK;
}


// nsITreeView implementation

NS_IMETHODIMP
nsFileView::GetRowCount(int32_t* aRowCount)
{
  *aRowCount = mTotalRows;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetSelection(nsITreeSelection** aSelection)
{
  *aSelection = mSelection;
  NS_IF_ADDREF(*aSelection);
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::SetSelection(nsITreeSelection* aSelection)
{
  mSelection = aSelection;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetRowProperties(int32_t aIndex, nsAString& aProps)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetCellProperties(int32_t aRow, nsITreeColumn* aCol,
                              nsAString& aProps)
{
  uint32_t dirCount = mDirList.Length();

  if (aRow < (int32_t) dirCount)
    aProps.AppendLiteral("directory");
  else if (aRow < mTotalRows)
    aProps.AppendLiteral("file");

  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetColumnProperties(nsITreeColumn* aCol, nsAString& aProps)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::IsContainer(int32_t aIndex, bool* aIsContainer)
{
  *aIsContainer = false;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::IsContainerOpen(int32_t aIndex, bool* aIsOpen)
{
  *aIsOpen = false;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::IsContainerEmpty(int32_t aIndex, bool* aIsEmpty)
{
  *aIsEmpty = false;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::IsSeparator(int32_t aIndex, bool* aIsSeparator)
{
  *aIsSeparator = false;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::IsSorted(bool* aIsSorted)
{
  *aIsSorted = (mSortType >= 0);
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::CanDrop(int32_t aIndex, int32_t aOrientation,
                    nsIDOMDataTransfer* dataTransfer, bool* aCanDrop)
{
  *aCanDrop = false;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::Drop(int32_t aRow, int32_t aOrientation, nsIDOMDataTransfer* dataTransfer)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetParentIndex(int32_t aRowIndex, int32_t* aParentIndex)
{
  *aParentIndex = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::HasNextSibling(int32_t aRowIndex, int32_t aAfterIndex, 
                           bool* aHasSibling)
{
  *aHasSibling = (aAfterIndex < (mTotalRows - 1));
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetLevel(int32_t aIndex, int32_t* aLevel)
{
  *aLevel = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetImageSrc(int32_t aRow, nsITreeColumn* aCol,
                        nsAString& aImageSrc)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetProgressMode(int32_t aRow, nsITreeColumn* aCol,
                            int32_t* aProgressMode)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetCellValue(int32_t aRow, nsITreeColumn* aCol,
                         nsAString& aCellValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetCellText(int32_t aRow, nsITreeColumn* aCol,
                        nsAString& aCellText)
{
  uint32_t dirCount = mDirList.Length();
  bool isDirectory;
  nsIFile* curFile = nullptr;

  if (aRow < (int32_t) dirCount) {
    isDirectory = true;
    curFile = mDirList[aRow];
  } else if (aRow < mTotalRows) {
    isDirectory = false;
    curFile = mFilteredFiles[aRow - dirCount];
  } else {
    // invalid row
    aCellText.SetCapacity(0);
    return NS_OK;
  }

  const char16_t* colID;
  aCol->GetIdConst(&colID);
  if (NS_LITERAL_STRING("FilenameColumn").Equals(colID)) {
    curFile->GetLeafName(aCellText);
  } else if (NS_LITERAL_STRING("LastModifiedColumn").Equals(colID)) {
    PRTime lastModTime;
    curFile->GetLastModifiedTime(&lastModTime);
    // XXX FormatPRTime could take an nsAString&
    nsAutoString temp;
    mDateFormatter->FormatPRTime(nullptr, kDateFormatShort, kTimeFormatSeconds,
                                 lastModTime * 1000, temp);
    aCellText = temp;
  } else {
    // file size
    if (isDirectory)
      aCellText.SetCapacity(0);
    else {
      int64_t fileSize;
      curFile->GetFileSize(&fileSize);
      CopyUTF8toUTF16(nsPrintfCString("%lld", fileSize), aCellText);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFileView::SetTree(nsITreeBoxObject* aTree)
{
  mTree = aTree;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::ToggleOpenState(int32_t aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::CycleHeader(nsITreeColumn* aCol)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::SelectionChanged()
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::CycleCell(int32_t aRow, nsITreeColumn* aCol)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::IsEditable(int32_t aRow, nsITreeColumn* aCol,
                       bool* aIsEditable)
{
  *aIsEditable = false;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::IsSelectable(int32_t aRow, nsITreeColumn* aCol,
                         bool* aIsSelectable)
{
  *aIsSelectable = false;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::SetCellValue(int32_t aRow, nsITreeColumn* aCol,
                         const nsAString& aValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::SetCellText(int32_t aRow, nsITreeColumn* aCol,
                        const nsAString& aValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::PerformAction(const char16_t* aAction)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::PerformActionOnRow(const char16_t* aAction, int32_t aRow)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::PerformActionOnCell(const char16_t* aAction, int32_t aRow,
                                nsITreeColumn* aCol)
{
  return NS_OK;
}

// Private methods

void
nsFileView::FilterFiles()
{
  uint32_t count = mDirList.Length();
  mTotalRows = count;
  count = mFileList.Length();
  mFilteredFiles.Clear();
  uint32_t filterCount = mCurrentFilters.Length();

  for (uint32_t i = 0; i < count; ++i) {
    nsIFile* file = mFileList[i];
    bool isHidden = false;
    if (!mShowHiddenFiles)
      file->IsHidden(&isHidden);
    
    nsAutoString ucsLeafName;
    if(NS_FAILED(file->GetLeafName(ucsLeafName))) {
      // need to check return value for GetLeafName()
      continue;
    }
    
    if (!isHidden) {
      for (uint32_t j = 0; j < filterCount; ++j) {
        bool matched = false;
        if (!nsCRT::strcmp(mCurrentFilters.ElementAt(j),
                           u"..apps"))
        {
          file->IsExecutable(&matched);
        } else
          matched = (NS_WildCardMatch(ucsLeafName.get(),
                                      mCurrentFilters.ElementAt(j),
                                      true) == MATCH);

        if (matched) {
          mFilteredFiles.AppendElement(file);
          ++mTotalRows;
          break;
        }
      }
    }
  }
}

void
nsFileView::ReverseArray(nsTArray<nsCOMPtr<nsIFile> >& aArray)
{
  uint32_t count = aArray.Length();
  for (uint32_t i = 0; i < count/2; ++i) {
    // If we get references to the COMPtrs in the array, and then .swap() them
    // we avoid AdRef() / Release() calls.
    nsCOMPtr<nsIFile>& element = aArray[i];
    nsCOMPtr<nsIFile>& element2 = aArray[count - i - 1];
    element.swap(element2);
  }
}

static int
SortNameCallback(const void* aElement1, const void* aElement2, void* aContext)
{
  nsIFile* file1 = *static_cast<nsIFile* const *>(aElement1);
  nsIFile* file2 = *static_cast<nsIFile* const *>(aElement2);
  
  nsAutoString leafName1, leafName2;
  file1->GetLeafName(leafName1);
  file2->GetLeafName(leafName2);

  return Compare(leafName1, leafName2);
}

static int
SortSizeCallback(const void* aElement1, const void* aElement2, void* aContext)
{
  nsIFile* file1 = *static_cast<nsIFile* const *>(aElement1);
  nsIFile* file2 = *static_cast<nsIFile* const *>(aElement2);

  int64_t size1, size2;
  file1->GetFileSize(&size1);
  file2->GetFileSize(&size2);

  if (size1 == size2)
    return 0;

  return size1 < size2 ? -1 : 1;
}

static int
SortDateCallback(const void* aElement1, const void* aElement2, void* aContext)
{
  nsIFile* file1 = *static_cast<nsIFile* const *>(aElement1);
  nsIFile* file2 = *static_cast<nsIFile* const *>(aElement2);

  PRTime time1, time2;
  file1->GetLastModifiedTime(&time1);
  file2->GetLastModifiedTime(&time2);

  if (time1 == time2)
    return 0;

  return time1 < time2 ? -1 : 1;
}

void
nsFileView::SortArray(nsTArray<nsCOMPtr<nsIFile> >& aArray)
{
  // We assume the array to be in filesystem order, which
  // for our purposes, is completely unordered.

  int (*compareFunc)(const void*, const void*, void*);

  switch (mSortType) {
  case sortName:
    compareFunc = SortNameCallback;
    break;
  case sortSize:
    compareFunc = SortSizeCallback;
    break;
  case sortDate:
    compareFunc = SortDateCallback;
    break;
  default:
    return;
  }

  uint32_t count = aArray.Length();

  nsIFile** array = new nsIFile*[count];
  for (uint32_t i = 0; i < count; ++i) {
    array[i] = aArray[i];
  }

  NS_QuickSort(array, count, sizeof(nsIFile*), compareFunc, nullptr);

  for (uint32_t i = 0; i < count; ++i) {
    // Use swap() to avoid refcounting.
    aArray[i].swap(array[i]);
  }

  delete[] array;
}

void
nsFileView::SortInternal()
{
  SortArray(mDirList);
  SortArray(mFilteredFiles);

  if (mReverseSort) {
    ReverseArray(mDirList);
    ReverseArray(mFilteredFiles);
  }
}
