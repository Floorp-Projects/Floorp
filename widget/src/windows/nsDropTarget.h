//IDropTarget.h

#ifndef  __IDROPTARGET_H__
#define  __IDROPTARGET_H__

#include "OLEIDL.H"

//forward declaration 
//CLASS CContainDoc;

class nsIWidget;

class nsDropTarget : public IDropTarget
{
public:

	STDMETHODIMP QueryInterface (REFIID riid, LPVOID FAR* ppv);
	STDMETHODIMP_(ULONG) AddRef (void);
	STDMETHODIMP_(ULONG) Release (void);

	STDMETHODIMP DragEnter (LPDATAOBJECT pDataObj, DWORD grfKeyState,POINTL pt, LPDWORD pdwEffect);
	STDMETHODIMP DragOver  (DWORD grfKeyState, POINTL pt, LPDWORD pdwEffect);
	STDMETHODIMP DragLeave ();
	STDMETHODIMP Drop (LPDATAOBJECT pDataObj, DWORD grfKeyState, POINTL pt,LPDWORD pdwEffect);

   nsDropTarget(nsIWidget * aWindow);
   ~nsDropTarget();

protected:
  USHORT nsDropTarget::GetFormats(DWORD direction, USHORT maxFormats, FORMATETC* formats);

private:
   nsIWidget * m_pWin;
   long         m_refs;

   DWORD    FindDragDropEffect(DWORD grfKeyState, POINTL pointl);
};

#endif
