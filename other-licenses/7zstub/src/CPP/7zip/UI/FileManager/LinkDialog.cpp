// LinkDialog.cpp

#include "StdAfx.h"

#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileIO.h"
#include "../../../Windows/FileName.h"

#ifdef LANG
#include "LangUtils.h"
#endif

#include "BrowseDialog.h"
#include "CopyDialogRes.h"
#include "LinkDialog.h"
#include "resourceGui.h"

#include "App.h"

#include "resource.h"

extern bool g_SymLink_Supported;

using namespace NWindows;
using namespace NFile;

#ifdef LANG
static const UInt32 kLangIDs[] =
{
  IDB_LINK_LINK,
  IDT_LINK_PATH_FROM,
  IDT_LINK_PATH_TO,
  IDG_LINK_TYPE,
  IDR_LINK_TYPE_HARD,
  IDR_LINK_TYPE_SYM_FILE,
  IDR_LINK_TYPE_SYM_DIR,
  IDR_LINK_TYPE_JUNCTION
};
#endif

static bool GetSymLink(CFSTR path, CReparseAttr &attr)
{
  NIO::CInFile file;
  if (!file.Open(path,
      FILE_SHARE_READ,
      OPEN_EXISTING,
      FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS))
    return false;

  const unsigned kBufSize = MAXIMUM_REPARSE_DATA_BUFFER_SIZE;
  CByteArr buf(kBufSize);
  DWORD returnedSize;
  if (!file.DeviceIoControlOut(my_FSCTL_GET_REPARSE_POINT, buf, kBufSize, &returnedSize))
    return false;
  
  DWORD errorCode = 0;
  if (!attr.Parse(buf, returnedSize, errorCode))
    return false;

  CByteBuffer data2;
  if (!FillLinkData(data2, attr.GetPath(), attr.IsSymLink()))
    return false;
    
  if (data2.Size() != returnedSize ||
      memcmp(data2, buf, returnedSize) != 0)
    return false;

  return true;
}

static const int k_LinkType_Buttons[] =
{
  IDR_LINK_TYPE_HARD,
  IDR_LINK_TYPE_SYM_FILE,
  IDR_LINK_TYPE_SYM_DIR,
  IDR_LINK_TYPE_JUNCTION
};

void CLinkDialog::Set_LinkType_Radio(int idb)
{
  CheckRadioButton(k_LinkType_Buttons[0], k_LinkType_Buttons[ARRAY_SIZE(k_LinkType_Buttons) - 1], idb);
}

bool CLinkDialog::OnInit()
{
  #ifdef LANG
  LangSetWindowText(*this, IDD_LINK);
  LangSetDlgItems(*this, kLangIDs, ARRAY_SIZE(kLangIDs));
  #endif
  
  _pathFromCombo.Attach(GetItem(IDC_LINK_PATH_FROM));
  _pathToCombo.Attach(GetItem(IDC_LINK_PATH_TO));
  
  if (!FilePath.IsEmpty())
  {
    NFind::CFileInfo fi;
    int linkType = 0;
    if (!fi.Find(us2fs(FilePath)))
      linkType = IDR_LINK_TYPE_SYM_FILE;
    else
    {
      if (fi.HasReparsePoint())
      {
        CReparseAttr attr;
        bool res = GetSymLink(us2fs(FilePath), attr);
        
        UString s = attr.PrintName;
        if (!attr.IsOkNamePair())
        {
          s += " : ";
          s += attr.SubsName;
        }
        if (!res)
          s.Insert(0, L"ERROR: ");
        
        SetItemText(IDT_LINK_PATH_TO_CUR, s);
        
        UString destPath = attr.GetPath();
        _pathFromCombo.SetText(FilePath);
        _pathToCombo.SetText(destPath);
        
        if (res)
        {
          if (attr.IsMountPoint())
            linkType = IDR_LINK_TYPE_JUNCTION;
          if (attr.IsSymLink())
          {
            linkType =
              fi.IsDir() ?
                IDR_LINK_TYPE_SYM_DIR :
                IDR_LINK_TYPE_SYM_FILE;
            // if (attr.IsRelative()) linkType = IDR_LINK_TYPE_SYM_RELATIVE;
          }
          
          if (linkType != 0)
            Set_LinkType_Radio(linkType);
        }
      }
      else
      {
        _pathFromCombo.SetText(AnotherPath);
        _pathToCombo.SetText(FilePath);
        if (fi.IsDir())
          linkType = g_SymLink_Supported ?
              IDR_LINK_TYPE_SYM_DIR :
              IDR_LINK_TYPE_JUNCTION;
        else
          linkType = IDR_LINK_TYPE_HARD;
      }
    }
    if (linkType != 0)
      Set_LinkType_Radio(linkType);
  }

  NormalizeSize();
  return CModalDialog::OnInit();
}

bool CLinkDialog::OnSize(WPARAM /* wParam */, int xSize, int ySize)
{
  int mx, my;
  GetMargins(8, mx, my);
  int bx1, bx2, by;
  GetItemSizes(IDCANCEL, bx1, by);
  GetItemSizes(IDB_LINK_LINK, bx2, by);
  int yPos = ySize - my - by;
  int xPos = xSize - mx - bx1;

  InvalidateRect(NULL);

  {
    RECT r, r2;
    GetClientRectOfItem(IDB_LINK_PATH_FROM, r);
    GetClientRectOfItem(IDB_LINK_PATH_TO, r2);
    int bx = RECT_SIZE_X(r);
    int newButtonXpos = xSize - mx - bx;

    MoveItem(IDB_LINK_PATH_FROM, newButtonXpos, r.top, bx, RECT_SIZE_Y(r));
    MoveItem(IDB_LINK_PATH_TO, newButtonXpos, r2.top, bx, RECT_SIZE_Y(r2));

    int newComboXsize = newButtonXpos - mx - mx;
    ChangeSubWindowSizeX(_pathFromCombo, newComboXsize);
    ChangeSubWindowSizeX(_pathToCombo, newComboXsize);
  }

  MoveItem(IDCANCEL, xPos, yPos, bx1, by);
  MoveItem(IDB_LINK_LINK, xPos - mx - bx2, yPos, bx2, by);

  return false;
}

bool CLinkDialog::OnButtonClicked(int buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDB_LINK_PATH_FROM:
      OnButton_SetPath(false);
      return true;
    case IDB_LINK_PATH_TO:
      OnButton_SetPath(true);
      return true;
    case IDB_LINK_LINK:
      OnButton_Link();
      return true;
  }
  return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
}

void CLinkDialog::OnButton_SetPath(bool to)
{
  UString currentPath;
  NWindows::NControl::CComboBox &combo = to ?
    _pathToCombo :
    _pathFromCombo;
  combo.GetText(currentPath);
  // UString title = "Specify a location for output folder";
  UString title = LangString(IDS_SET_FOLDER);

  UString resultPath;
  if (!MyBrowseForFolder(*this, title, currentPath, resultPath))
    return;
  NName::NormalizeDirPathPrefix(resultPath);
  combo.SetCurSel(-1);
  combo.SetText(resultPath);
}

void CLinkDialog::ShowError(const wchar_t *s)
{
  ::MessageBoxW(*this, s, L"7-Zip", MB_ICONERROR);
}

void CLinkDialog::ShowLastErrorMessage()
{
  ShowError(NError::MyFormatMessage(GetLastError()));
}

void CLinkDialog::OnButton_Link()
{
  UString from, to;
  _pathFromCombo.GetText(from);
  _pathToCombo.GetText(to);

  if (from.IsEmpty())
    return;
  if (!NName::IsAbsolutePath(from))
    from.Insert(0, CurDirPrefix);

  int idb = -1;
  for (unsigned i = 0;; i++)
  {
    if (i >= ARRAY_SIZE(k_LinkType_Buttons))
      return;
    idb = k_LinkType_Buttons[i];
    if (IsButtonCheckedBool(idb))
      break;
  }

  NFind::CFileInfo info1, info2;
  bool finded1 = info1.Find(us2fs(from));
  bool finded2 = info2.Find(us2fs(to));

  bool isDirLink = (
      idb == IDR_LINK_TYPE_SYM_DIR ||
      idb == IDR_LINK_TYPE_JUNCTION);

  if (finded1 && info1.IsDir() != isDirLink ||
      finded2 && info2.IsDir() != isDirLink)
  {
    ShowError(L"Incorrect link type");
    return;
  }
  
  if (idb == IDR_LINK_TYPE_HARD)
  {
    if (!NDir::MyCreateHardLink(us2fs(from), us2fs(to)))
    {
      ShowLastErrorMessage();
      return;
    }
  }
  else
  {
    bool isSymLink = (idb != IDR_LINK_TYPE_JUNCTION);
    
    CByteBuffer data;
    if (!FillLinkData(data, to, isSymLink))
    {
      ShowError(L"Incorrect link");
      return;
    }
    
    CReparseAttr attr;
    DWORD errorCode = 0;
    if (!attr.Parse(data, data.Size(), errorCode))
    {
      ShowError(L"Internal conversion error");
      return;
    }
    
    
    if (!NIO::SetReparseData(us2fs(from), isDirLink, data, (DWORD)data.Size()))
    {
      ShowLastErrorMessage();
      return;
    }
  }

  End(IDOK);
}

void CApp::Link()
{
  unsigned srcPanelIndex = GetFocusedPanelIndex();
  CPanel &srcPanel = Panels[srcPanelIndex];
  if (!srcPanel.IsFSFolder())
  {
    srcPanel.MessageBox_Error_UnsupportOperation();
    return;
  }
  CRecordVector<UInt32> indices;
  srcPanel.GetOperatedItemIndices(indices);
  if (indices.IsEmpty())
    return;
  if (indices.Size() != 1)
  {
    srcPanel.MessageBox_Error_LangID(IDS_SELECT_ONE_FILE);
    return;
  }
  int index = indices[0];
  const UString itemName = srcPanel.GetItemName(index);

  const UString fsPrefix = srcPanel.GetFsPath();
  const UString srcPath = fsPrefix + srcPanel.GetItemPrefix(index);
  UString path = srcPath;
  {
    unsigned destPanelIndex = (NumPanels <= 1) ? srcPanelIndex : (1 - srcPanelIndex);
    CPanel &destPanel = Panels[destPanelIndex];
    if (NumPanels > 1)
      if (destPanel.IsFSFolder())
        path = destPanel.GetFsPath();
  }

  CLinkDialog dlg;
  dlg.CurDirPrefix = fsPrefix;
  dlg.FilePath = srcPath + itemName;
  dlg.AnotherPath = path;

  if (dlg.Create(srcPanel.GetParent()) != IDOK)
    return;

  RefreshTitleAlways();
}
