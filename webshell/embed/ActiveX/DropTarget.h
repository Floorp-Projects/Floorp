/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef DROPTARGET_H
#define DROPTARGET_H

// Simple drop target implementation

class CDropTarget : public CComObjectRootEx<CComSingleThreadModel>,
                    public IDropTarget
{
public:
	CDropTarget();
	virtual ~CDropTarget();

BEGIN_COM_MAP(CDropTarget)
	COM_INTERFACE_ENTRY(IDropTarget)
END_COM_MAP()

// IDropTarget
	virtual HRESULT STDMETHODCALLTYPE DragEnter(/* [unique][in] */ IDataObject __RPC_FAR *pDataObj, /* [in] */ DWORD grfKeyState, /* [in] */ POINTL pt, /* [out][in] */ DWORD __RPC_FAR *pdwEffect);
	virtual HRESULT STDMETHODCALLTYPE DragOver(/* [in] */ DWORD grfKeyState, /* [in] */ POINTL pt, /* [out][in] */ DWORD __RPC_FAR *pdwEffect);
	virtual HRESULT STDMETHODCALLTYPE DragLeave(void);
	virtual HRESULT STDMETHODCALLTYPE Drop(/* [unique][in] */ IDataObject __RPC_FAR *pDataObj, /* [in] */ DWORD grfKeyState, /* [in] */ POINTL pt, /* [out][in] */ DWORD __RPC_FAR *pdwEffect);
};

typedef CComObject<CDropTarget> CDropTargetInstance;

#endif