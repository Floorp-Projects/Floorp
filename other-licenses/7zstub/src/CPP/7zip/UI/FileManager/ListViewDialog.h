// ListViewDialog.h

#ifndef __LISTVIEW_DIALOG_H
#define __LISTVIEW_DIALOG_H

#include "../../../Windows/Control/Dialog.h"
#include "../../../Windows/Control/ListView.h"

#include "ListViewDialogRes.h"

class CListViewDialog: public NWindows::NControl::CModalDialog
{
  NWindows::NControl::CListView _listView;
  virtual void OnOK();
  virtual bool OnInit();
  virtual bool OnSize(WPARAM wParam, int xSize, int ySize);
  virtual bool OnNotify(UINT controlID, LPNMHDR header);
public:
  UString Title;
  bool DeleteIsAllowed;
  bool StringsWereChanged;
  UStringVector Strings;
  int FocusedItemIndex;

  INT_PTR Create(HWND wndParent = 0) { return CModalDialog::Create(IDD_LISTVIEW, wndParent); }

  CListViewDialog(): DeleteIsAllowed(false) {}
};

#endif
