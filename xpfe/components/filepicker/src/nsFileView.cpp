/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communicationsm Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@netscape.com>
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

#include "nsIFileView.h"
#include "nsIOutlinerView.h"
#include "nsIGenericFactory.h"
#include "nsIOutlinerSelection.h"
#include "nsIOutlinerBoxObject.h"
#include "nsIFile.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "prmem.h"
#include "nsPrintfCString.h"
#include "nsVoidArray.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsQuickSort.h"

#include "nsWildCard.h"

#define NS_FILEVIEW_CID { 0xa5570462, 0x1dd1, 0x11b2, \
                         { 0x9d, 0x19, 0xdf, 0x30, 0xa2, 0x7f, 0xbd, 0xc4 } }

static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);

class nsFileView : public nsIFileView,
                   public nsIOutlinerView
{
public:
  nsFileView();
  nsresult Init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIFILEVIEW
  NS_DECL_NSIOUTLINERVIEW
  
protected:
  virtual ~nsFileView();
  
  PRInt32 FilterFiles();
  void ReverseArray(nsISupportsArray* aArray);
  void SortArray(nsISupportsArray* aArray);
  void SortInternal();

  nsCOMPtr<nsISupportsArray> mFileList;
  nsCOMPtr<nsISupportsArray> mDirList;
  nsCOMPtr<nsISupportsArray> mFilteredFiles;

  nsCOMPtr<nsIFile> mDirectoryPath;
  nsCOMPtr<nsIOutlinerBoxObject> mOutliner;
  nsCOMPtr<nsIOutlinerSelection> mSelection;
  nsCOMPtr<nsIAtom> mDirectoryAtom;
  nsCOMPtr<nsIAtom> mFileAtom;
  nsCOMPtr<nsIDateTimeFormat> mDateFormatter;

  PRInt16 mSortType;
  PRInt32 mTotalRows;

  nsVoidArray mCurrentFilters;

  PRPackedBool mShowHiddenFiles;
  PRPackedBool mDirectoryFilter;
  PRPackedBool mReverseSort;
};

// Factory constructor
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsFileView, Init)

static nsModuleComponentInfo components[] =
{
  { "nsFileView", NS_FILEVIEW_CID,
    NS_FILEVIEW_CONTRACTID, nsFileViewConstructor }
};

NS_IMPL_NSGETMODULE(nsFileViewModule, components);


nsFileView::nsFileView() :
  mSortType(-1),
  mTotalRows(0),
  mShowHiddenFiles(PR_FALSE),
  mDirectoryFilter(PR_FALSE),
  mReverseSort(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
}

nsFileView::~nsFileView()
{
  PRInt32 count = mCurrentFilters.Count();
  for (PRInt32 i = 0; i < count; ++i)
    PR_Free(mCurrentFilters[i]);
}

nsresult
nsFileView::Init()
{
  mDirectoryAtom = dont_AddRef(NS_NewAtom("directory"));
  mFileAtom = dont_AddRef(NS_NewAtom("file"));
  NS_NewISupportsArray(getter_AddRefs(mFileList));
  NS_NewISupportsArray(getter_AddRefs(mDirList));
  NS_NewISupportsArray(getter_AddRefs(mFilteredFiles));
  mDateFormatter = do_CreateInstance(kDateTimeFormatCID);

  return NS_OK;
}

// nsISupports implementation

NS_IMPL_ISUPPORTS2(nsFileView, nsIOutlinerView, nsIFileView);

// nsIFileView implementation

NS_IMETHODIMP
nsFileView::SetShowHiddenFiles(PRBool aShowHidden)
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
nsFileView::GetShowHiddenFiles(PRBool* aShowHidden)
{
  *aShowHidden = mShowHiddenFiles;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::SetShowOnlyDirectories(PRBool aOnlyDirs)
{
  if (aOnlyDirs == mDirectoryFilter)
    return NS_OK;

  mDirectoryFilter = aOnlyDirs;
  PRUint32 dirCount;
  mDirList->Count(&dirCount);
  if (mDirectoryFilter) {
    PRInt32 rowDiff = mTotalRows - dirCount;

    mFilteredFiles->Clear();
    mTotalRows = dirCount;
    if (mOutliner)
      mOutliner->RowCountChanged(mTotalRows, -rowDiff);
  } else {
    // Run the filter again to get the file list back
    PRInt32 rowsAdded = FilterFiles();
    if (rowsAdded)
      mOutliner->RowCountChanged(dirCount, rowsAdded);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetShowOnlyDirectories(PRBool* aOnlyDirs)
{
  *aOnlyDirs = mDirectoryFilter;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetSortType(PRInt16* aSortType)
{
  *aSortType = mSortType;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetReverseSort(PRBool* aReverseSort)
{
  *aReverseSort = mReverseSort;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::Sort(PRInt16 aSortType, PRBool aReverseSort)
{
  if (aSortType == mSortType) {
    if (aReverseSort != mReverseSort) {
      mReverseSort = aReverseSort;
      ReverseArray(mDirList);
      ReverseArray(mFilteredFiles);
    } else
      return NS_OK;
  } else {
    mSortType = aSortType;
    mReverseSort = aReverseSort;
    SortInternal();
  }

  if (mOutliner)
    mOutliner->Invalidate();

  return NS_OK;
}

NS_IMETHODIMP
nsFileView::SetDirectory(nsIFile* aDirectory)
{
  mDirectoryPath = aDirectory;
  mFileList->Clear();
  mDirList->Clear();

  nsCOMPtr<nsISimpleEnumerator> dirEntries;
  mDirectoryPath->GetDirectoryEntries(getter_AddRefs(dirEntries));
  PRBool hasMore = PR_FALSE;
  PRInt32 dirCount = 0, fileCount = 0;

  while (NS_SUCCEEDED(dirEntries->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> nextItem;
    dirEntries->GetNext(getter_AddRefs(nextItem));
    nsCOMPtr<nsIFile> theFile = do_QueryInterface(nextItem);

    PRBool isDirectory;
    theFile->IsDirectory(&isDirectory);

    if (isDirectory) {
      PRBool isHidden;
      theFile->IsHidden(&isHidden);
      if (mShowHiddenFiles || !isHidden) {
        mDirList->AppendElement(theFile);
        ++dirCount;
      }
    }
    else {
      mFileList->AppendElement(theFile);
      ++fileCount;
    }
  }

  PRInt32 oldRows = mTotalRows;

  FilterFiles();
  SortInternal();

  if (mOutliner) {
    mOutliner->RowCountChanged(0, -oldRows);
    mOutliner->RowCountChanged(0, mTotalRows);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFileView::SetFilter(const PRUnichar* aFilterString)
{
  PRInt32 filterCount = mCurrentFilters.Count();
  for (PRInt32 i = 0; i < filterCount; ++i)
    PR_Free(mCurrentFilters[i]);
  mCurrentFilters.Clear();
  
  const PRUnichar* chr, *aPos = aFilterString;
  for (chr = aFilterString; *chr; ++chr) {
    if (*chr == ';') {
      PRUnichar* aNewString = nsCRT::strndup(aPos, (chr - aPos));
      mCurrentFilters.AppendElement(aNewString);

      // ; will be followed by a space, and then the next filter
      chr += 2;
      aPos = chr;
    }
  }

  if ((aPos < chr) && *aPos) {
    PRUnichar* aNewString = nsCRT::strndup(aPos, (chr - aPos));
    mCurrentFilters.AppendElement(aNewString);
  }

  mFilteredFiles->Clear();

  PRUint32 dirCount;
  mDirList->Count(&dirCount);
  PRInt32 oldFileRows = mTotalRows - dirCount;
  PRInt32 newFileRows = FilterFiles();

  SortArray(mFilteredFiles);
  if (mReverseSort)
    ReverseArray(mFilteredFiles);

  if (mOutliner) {
    mOutliner->RowCountChanged(dirCount, newFileRows - oldFileRows);

    PRInt32 commonRange = PR_MIN(newFileRows, oldFileRows);
    if (commonRange)
      mOutliner->InvalidateRange(dirCount, dirCount + commonRange);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetSelectedFile(nsIFile** aFile)
{
  *aFile = nsnull;

  PRInt32 currentIndex;
  mSelection->GetCurrentIndex(&currentIndex);

  if (0 <= currentIndex) {
    PRUint32 dirCount;
    mDirList->Count(&dirCount);
    if (currentIndex < (PRInt32) dirCount)
      mDirList->QueryElementAt(currentIndex, NS_GET_IID(nsIFile),
                               (void**)aFile);
    else {
      if (currentIndex < mTotalRows)
        mFilteredFiles->QueryElementAt(currentIndex - dirCount,
                                       NS_GET_IID(nsIFile), (void**)aFile);
    }
  }

  return NS_OK;
}


// nsIOutlinerView implementation

NS_IMETHODIMP
nsFileView::GetRowCount(PRInt32* aRowCount)
{
  *aRowCount = mTotalRows;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetSelection(nsIOutlinerSelection** aSelection)
{
  *aSelection = mSelection;
  NS_IF_ADDREF(*aSelection);
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::SetSelection(nsIOutlinerSelection* aSelection)
{
  mSelection = aSelection;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetRowProperties(PRInt32 aIndex,
                             nsISupportsArray* aProperties)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetCellProperties(PRInt32 aRow, const PRUnichar* aColID,
                              nsISupportsArray* aProperties)
{
  PRUint32 dirCount;
  mDirList->Count(&dirCount);

  if (aRow < (PRInt32) dirCount)
    aProperties->AppendElement(mDirectoryAtom);
  else if (aRow < mTotalRows)
    aProperties->AppendElement(mFileAtom);

  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetColumnProperties(const PRUnichar* aColID, 
                                nsIDOMElement* aColElement,
                                nsISupportsArray* aProperties)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::IsContainer(PRInt32 aIndex, PRBool* aIsContainer)
{
  *aIsContainer = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::IsContainerOpen(PRInt32 aIndex, PRBool* aIsOpen)
{
  *aIsOpen = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::IsContainerEmpty(PRInt32 aIndex, PRBool* aIsEmpty)
{
  *aIsEmpty = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::IsSeparator(PRInt32 aIndex, PRBool* aIsSeparator)
{
  *aIsSeparator = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::IsSorted(PRBool* aIsSorted)
{
  *aIsSorted = (mSortType >= 0);
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::CanDropOn(PRInt32 aIndex, PRBool* aCanDrop)
{
  *aCanDrop = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::CanDropBeforeAfter(PRInt32 aIndex, PRBool aBefore, 
                               PRBool* aCanDrop)
{
  *aCanDrop = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::Drop(PRInt32 aRow, PRInt32 aOrientation)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetParentIndex(PRInt32 aRowIndex, PRInt32* aParentIndex)
{
  *aParentIndex = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::HasNextSibling(PRInt32 aRowIndex, PRInt32 aAfterIndex, 
                           PRBool* aHasSibling)
{
  *aHasSibling = (aAfterIndex < (mTotalRows - 1));
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetLevel(PRInt32 aIndex, PRInt32* aLevel)
{
  *aLevel = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::GetCellText(PRInt32 aRow, const PRUnichar* aColID,
                        PRUnichar** aCellText)
{
  PRUint32 dirCount, fileCount;
  mDirList->Count(&dirCount);
  mFilteredFiles->Count(&fileCount);

  PRBool isDirectory;
  nsCOMPtr<nsIFile> curFile;

  if (aRow < (PRInt32) dirCount) {
    isDirectory = PR_TRUE;
    curFile = do_QueryElementAt(mDirList, aRow);
  } else if (aRow < mTotalRows) {
    isDirectory = PR_FALSE;
    curFile = do_QueryElementAt(mFilteredFiles, aRow - dirCount);
  } else {
    // invalid row
    *aCellText = ToNewUnicode(NS_LITERAL_STRING(""));
    return NS_OK;
  }

  if (NS_LITERAL_STRING("FilenameColumn").Equals(aColID)) {
    curFile->GetUnicodeLeafName(aCellText);
  } else if (NS_LITERAL_STRING("LastModifiedColumn").Equals(aColID)) {
    PRInt64 lastModTime;
    curFile->GetLastModificationTime(&lastModTime);
    nsAutoString dateString;
    mDateFormatter->FormatPRTime(nsnull, kDateFormatShort, kTimeFormatSeconds,
                                 lastModTime * 1000, dateString);
    *aCellText = ToNewUnicode(dateString);
  } else {
    // file size
    if (isDirectory)
      *aCellText = ToNewUnicode(NS_LITERAL_STRING(""));
    else {
      PRInt64 fileSize;
      curFile->GetFileSize(&fileSize);
      *aCellText = ToNewUnicode(nsPrintfCString("%lld", fileSize));
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFileView::SetOutliner(nsIOutlinerBoxObject* aOutliner)
{
  mOutliner = aOutliner;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::ToggleOpenState(PRInt32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::CycleHeader(const PRUnichar* aColID, nsIDOMElement* aElement)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::SelectionChanged()
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::CycleCell(PRInt32 aRow, const PRUnichar* aColID)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::IsEditable(PRInt32 aRow, const PRUnichar* aColID,
                       PRBool* aIsEditable)
{
  *aIsEditable = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::SetCellText(PRInt32 aRow, const PRUnichar* aColID,
                        const PRUnichar* aValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::PerformAction(const PRUnichar* aAction)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::PerformActionOnRow(const PRUnichar* aAction, PRInt32 aRow)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFileView::PerformActionOnCell(const PRUnichar* aAction, PRInt32 aRow,
                                const PRUnichar* aColID)
{
  return NS_OK;
}

// Private methods

PRInt32
nsFileView::FilterFiles()
{
  PRUint32 count = 0, filteredFiles = 0;
  mFileList->Count(&count);
  mFilteredFiles->Clear();
  PRInt32 filterCount = mCurrentFilters.Count();

  nsCOMPtr<nsIFile> file;
  for (PRUint32 i = 0; i < count; ++i) {
    file = do_QueryElementAt(mFileList, i);
    PRBool isHidden = PR_FALSE;
    if (!mShowHiddenFiles)
      file->IsHidden(&isHidden);
    
    nsXPIDLString unicodeLeafName;
    file->GetUnicodeLeafName(getter_Copies(unicodeLeafName));
    
    if (!isHidden) {
      for (PRInt32 j = 0; j < filterCount; ++j) {
        if (NS_WildCardMatch(unicodeLeafName.get(),
                             (const PRUnichar*) mCurrentFilters.ElementAt(j),
                             PR_TRUE) == MATCH) {
          mFilteredFiles->AppendElement(file);
          ++filteredFiles;
          break;
        }
      }
    }
  }

  mDirList->Count(&count);
  mTotalRows = count + filteredFiles;
  return filteredFiles;
}

void
nsFileView::ReverseArray(nsISupportsArray* aArray)
{
  PRUint32 count;
  aArray->Count(&count);
  for (PRUint32 i = 0; i < count/2; ++i) {
    nsCOMPtr<nsISupports> element = dont_AddRef(aArray->ElementAt(i));
    nsCOMPtr<nsISupports> element2 = dont_AddRef(aArray->ElementAt(count-i-1));
    aArray->ReplaceElementAt(element2, i);
    aArray->ReplaceElementAt(element, count-i-1);
  }
}

PR_STATIC_CALLBACK(int)
SortNameCallback(const void* aElement1, const void* aElement2, void* aContext)
{
  nsIFile* file1 = *NS_STATIC_CAST(nsIFile* const *, aElement1);
  nsIFile* file2 = *NS_STATIC_CAST(nsIFile* const *, aElement2);
  
  nsXPIDLString leafName1, leafName2;
  file1->GetUnicodeLeafName(getter_Copies(leafName1));
  file2->GetUnicodeLeafName(getter_Copies(leafName2));

  return nsCRT::strcmp(leafName1.get(), leafName2.get());
}

PR_STATIC_CALLBACK(int)
SortSizeCallback(const void* aElement1, const void* aElement2, void* aContext)
{
  nsIFile* file1 = *NS_STATIC_CAST(nsIFile* const *, aElement1);
  nsIFile* file2 = *NS_STATIC_CAST(nsIFile* const *, aElement2);

  PRInt64 size1, size2;
  file1->GetFileSize(&size1);
  file2->GetFileSize(&size2);

  if (LL_EQ(size1, size2))
    return 0;

  return (LL_CMP(size1, <, size2) ? -1 : 1);
}

PR_STATIC_CALLBACK(int)
SortDateCallback(const void* aElement1, const void* aElement2, void* aContext)
{
  nsIFile* file1 = *NS_STATIC_CAST(nsIFile* const *, aElement1);
  nsIFile* file2 = *NS_STATIC_CAST(nsIFile* const *, aElement2);

  PRInt64 time1, time2;
  file1->GetLastModificationTime(&time1);
  file2->GetLastModificationTime(&time2);

  if (LL_EQ(time1, time2))
    return 0;

  return (LL_CMP(time1, <, time2) ? -1 : 1);
}

void
nsFileView::SortArray(nsISupportsArray* aArray)
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

  PRUint32 count;
  aArray->Count(&count);

  // each item will have an additional refcount while
  // the array is alive.
  nsIFile** array = new nsIFile*[count];
  PRUint32 i;
  for (i = 0; i < count; ++i)
    aArray->QueryElementAt(i, NS_GET_IID(nsIFile), (void**)&(array[i]));

  NS_QuickSort(array, count, sizeof(nsIFile*), compareFunc, nsnull);

  for (i = 0; i < count; ++i) {
    aArray->ReplaceElementAt(array[i], i);
    NS_RELEASE(array[i]);
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
