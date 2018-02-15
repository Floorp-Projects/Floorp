// ListViewDialog.cpp

#include "StdAfx.h"

#include "ListViewDialog.h"
#include "RegistryUtils.h"

#ifdef LANG
#include "LangUtils.h"
#endif

using namespace NWindows;

bool CListViewDialog::OnInit()
{
  #ifdef LANG
  LangSetDlgItems(*this, NULL, 0);
  #endif
  _listView.Attach(GetItem(IDL_LISTVIEW));

  CFmSettings st;
  st.Load();
  if (st.SingleClick)
    _listView.SetExtendedListViewStyle(LVS_EX_ONECLICKACTIVATE | LVS_EX_TRACKSELECT);

  SetText(Title);

  LVCOLUMN columnInfo;
  columnInfo.mask = LVCF_FMT | LVCF_WIDTH | LVCF_SUBITEM;
  columnInfo.fmt = LVCFMT_LEFT;
  columnInfo.iSubItem = 0;
  columnInfo.cx = 200;

  _listView.InsertColumn(0, &columnInfo);

  FOR_VECTOR (i, Strings)
    _listView.InsertItem(i, Strings[i]);

  if (Strings.Size() > 0)
    _listView.SetItemState_FocusedSelected(0);

  _listView.SetColumnWidthAuto(0);
  StringsWereChanged = false;

  NormalizeSize();
  return CModalDialog::OnInit();
}

bool CListViewDialog::OnSize(WPARAM /* wParam */, int xSize, int ySize)
{
  int mx, my;
  GetMargins(8, mx, my);
  int bx1, bx2, by;
  GetItemSizes(IDCANCEL, bx1, by);
  GetItemSizes(IDOK, bx2, by);
  int y = ySize - my - by;
  int x = xSize - mx - bx1;

  /*
  RECT rect;
  GetClientRect(&rect);
  rect.top = y - my;
  InvalidateRect(&rect);
  */
  InvalidateRect(NULL);

  MoveItem(IDCANCEL, x, y, bx1, by);
  MoveItem(IDOK, x - mx - bx2, y, bx2, by);
  /*
  if (wParam == SIZE_MAXSHOW || wParam == SIZE_MAXIMIZED || wParam == SIZE_MAXHIDE)
    mx = 0;
  */
  _listView.Move(mx, my, xSize - mx * 2, y - my * 2);
  return false;
}

extern bool g_LVN_ITEMACTIVATE_Support;

bool CListViewDialog::OnNotify(UINT /* controlID */, LPNMHDR header)
{
  if (header->hwndFrom != _listView)
    return false;
  switch (header->code)
  {
    case LVN_ITEMACTIVATE:
      if (g_LVN_ITEMACTIVATE_Support)
      {
        OnOK();
        return true;
      }
      break;
    case NM_DBLCLK:
    case NM_RETURN: // probabably it's unused
      if (!g_LVN_ITEMACTIVATE_Support)
      {
        OnOK();
        return true;
      }
      break;

    case LVN_KEYDOWN:
    {
      LPNMLVKEYDOWN keyDownInfo = LPNMLVKEYDOWN(header);
      switch (keyDownInfo->wVKey)
      {
        case VK_DELETE:
        {
          if (!DeleteIsAllowed)
            return false;
          for (;;)
          {
            int index = _listView.GetNextSelectedItem(-1);
            if (index < 0)
              break;
            StringsWereChanged = true;
            _listView.DeleteItem(index);
            Strings.Delete(index);
          }
          int focusedIndex = _listView.GetFocusedItem();
          if (focusedIndex >= 0)
            _listView.SetItemState_FocusedSelected(focusedIndex);
          _listView.SetColumnWidthAuto(0);
          return true;
        }
        case 'A':
        {
          if (IsKeyDown(VK_CONTROL))
          {
            _listView.SelectAll();
            return true;
          }
        }
      }
    }
  }
  return false;
}

void CListViewDialog::OnOK()
{
  FocusedItemIndex = _listView.GetFocusedItem();
  CModalDialog::OnOK();
}
