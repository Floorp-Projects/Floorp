
//////////////////////////////////////////////////////////////////////////////
// CProxyDWebBrowserEvents
template <class T>
class CProxyDWebBrowserEvents : public IConnectionPointImpl<T, &DIID_DWebBrowserEvents, CComDynamicUnkArray>
{
public:
//methods:
//DWebBrowserEvents : IDispatch
public:
	void Fire_BeforeNavigate(
		BSTR URL,
		long Flags,
		BSTR TargetFrameName,
		VARIANT * PostData,
		BSTR Headers,
		VARIANT_BOOL * Cancel)
	{
		VARIANTARG* pvars = new VARIANTARG[6];
		for (int i = 0; i < 6; i++)
			VariantInit(&pvars[i]);
		T* pT = (T*)this;
		pT->Lock();
		IUnknown** pp = m_vec.begin();
		while (pp < m_vec.end())
		{
			if (*pp != NULL)
			{
				pvars[5].vt = VT_BSTR;
				pvars[5].bstrVal= URL;
				pvars[4].vt = VT_I4;
				pvars[4].lVal= Flags;
				pvars[3].vt = VT_BSTR;
				pvars[3].bstrVal= TargetFrameName;
				pvars[2].vt = VT_VARIANT | VT_BYREF;
				pvars[2].byref= PostData;
				pvars[1].vt = VT_BSTR;
				pvars[1].bstrVal= Headers;
				pvars[0].vt = VT_BOOL | VT_BYREF;
				pvars[0].byref= Cancel;
				DISPPARAMS disp = { pvars, NULL, 6, 0 };
				IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
				pDispatch->Invoke(0x64, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
			}
			pp++;
		}
		pT->Unlock();
		delete[] pvars;
	}
	void Fire_NavigateComplete(
		BSTR URL)
	{
		VARIANTARG* pvars = new VARIANTARG[1];
		for (int i = 0; i < 1; i++)
			VariantInit(&pvars[i]);
		T* pT = (T*)this;
		pT->Lock();
		IUnknown** pp = m_vec.begin();
		while (pp < m_vec.end())
		{
			if (*pp != NULL)
			{
				pvars[0].vt = VT_BSTR;
				pvars[0].bstrVal= URL;
				DISPPARAMS disp = { pvars, NULL, 1, 0 };
				IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
				pDispatch->Invoke(0x65, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
			}
			pp++;
		}
		pT->Unlock();
		delete[] pvars;
	}
	void Fire_StatusTextChange(
		BSTR Text)
	{
		VARIANTARG* pvars = new VARIANTARG[1];
		for (int i = 0; i < 1; i++)
			VariantInit(&pvars[i]);
		T* pT = (T*)this;
		pT->Lock();
		IUnknown** pp = m_vec.begin();
		while (pp < m_vec.end())
		{
			if (*pp != NULL)
			{
				pvars[0].vt = VT_BSTR;
				pvars[0].bstrVal= Text;
				DISPPARAMS disp = { pvars, NULL, 1, 0 };
				IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
				pDispatch->Invoke(0x66, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
			}
			pp++;
		}
		pT->Unlock();
		delete[] pvars;
	}
	void Fire_ProgressChange(
		long Progress,
		long ProgressMax)
	{
		VARIANTARG* pvars = new VARIANTARG[2];
		for (int i = 0; i < 2; i++)
			VariantInit(&pvars[i]);
		T* pT = (T*)this;
		pT->Lock();
		IUnknown** pp = m_vec.begin();
		while (pp < m_vec.end())
		{
			if (*pp != NULL)
			{
				pvars[1].vt = VT_I4;
				pvars[1].lVal= Progress;
				pvars[0].vt = VT_I4;
				pvars[0].lVal= ProgressMax;
				DISPPARAMS disp = { pvars, NULL, 2, 0 };
				IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
				pDispatch->Invoke(0x6c, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
			}
			pp++;
		}
		pT->Unlock();
		delete[] pvars;
	}
	void Fire_DownloadComplete()
	{
		T* pT = (T*)this;
		pT->Lock();
		IUnknown** pp = m_vec.begin();
		while (pp < m_vec.end())
		{
			if (*pp != NULL)
			{
				DISPPARAMS disp = { NULL, NULL, 0, 0 };
				IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
				pDispatch->Invoke(0x68, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
			}
			pp++;
		}
		pT->Unlock();
	}
	void Fire_CommandStateChange(
		long Command,
		VARIANT_BOOL Enable)
	{
		VARIANTARG* pvars = new VARIANTARG[2];
		for (int i = 0; i < 2; i++)
			VariantInit(&pvars[i]);
		T* pT = (T*)this;
		pT->Lock();
		IUnknown** pp = m_vec.begin();
		while (pp < m_vec.end())
		{
			if (*pp != NULL)
			{
				pvars[1].vt = VT_I4;
				pvars[1].lVal= Command;
				pvars[0].vt = VT_BOOL;
				pvars[0].boolVal= Enable;
				DISPPARAMS disp = { pvars, NULL, 2, 0 };
				IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
				pDispatch->Invoke(0x69, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
			}
			pp++;
		}
		pT->Unlock();
		delete[] pvars;
	}
	void Fire_DownloadBegin()
	{
		T* pT = (T*)this;
		pT->Lock();
		IUnknown** pp = m_vec.begin();
		while (pp < m_vec.end())
		{
			if (*pp != NULL)
			{
				DISPPARAMS disp = { NULL, NULL, 0, 0 };
				IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
				pDispatch->Invoke(0x6a, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
			}
			pp++;
		}
		pT->Unlock();
	}
	void Fire_NewWindow(
		BSTR URL,
		long Flags,
		BSTR TargetFrameName,
		VARIANT * PostData,
		BSTR Headers,
		VARIANT_BOOL * Processed)
	{
		VARIANTARG* pvars = new VARIANTARG[6];
		for (int i = 0; i < 6; i++)
			VariantInit(&pvars[i]);
		T* pT = (T*)this;
		pT->Lock();
		IUnknown** pp = m_vec.begin();
		while (pp < m_vec.end())
		{
			if (*pp != NULL)
			{
				pvars[5].vt = VT_BSTR;
				pvars[5].bstrVal= URL;
				pvars[4].vt = VT_I4;
				pvars[4].lVal= Flags;
				pvars[3].vt = VT_BSTR;
				pvars[3].bstrVal= TargetFrameName;
				pvars[2].vt = VT_VARIANT | VT_BYREF;
				pvars[2].byref= PostData;
				pvars[1].vt = VT_BSTR;
				pvars[1].bstrVal= Headers;
				pvars[0].vt = VT_BOOL | VT_BYREF;
				pvars[0].byref= Processed;
				DISPPARAMS disp = { pvars, NULL, 6, 0 };
				IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
				pDispatch->Invoke(0x6b, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
			}
			pp++;
		}
		pT->Unlock();
		delete[] pvars;
	}
	void Fire_TitleChange(
		BSTR Text)
	{
		VARIANTARG* pvars = new VARIANTARG[1];
		for (int i = 0; i < 1; i++)
			VariantInit(&pvars[i]);
		T* pT = (T*)this;
		pT->Lock();
		IUnknown** pp = m_vec.begin();
		while (pp < m_vec.end())
		{
			if (*pp != NULL)
			{
				pvars[0].vt = VT_BSTR;
				pvars[0].bstrVal= Text;
				DISPPARAMS disp = { pvars, NULL, 1, 0 };
				IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
				pDispatch->Invoke(0x71, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
			}
			pp++;
		}
		pT->Unlock();
		delete[] pvars;
	}
	void Fire_FrameBeforeNavigate(
		BSTR URL,
		long Flags,
		BSTR TargetFrameName,
		VARIANT * PostData,
		BSTR Headers,
		VARIANT_BOOL * Cancel)
	{
		VARIANTARG* pvars = new VARIANTARG[6];
		for (int i = 0; i < 6; i++)
			VariantInit(&pvars[i]);
		T* pT = (T*)this;
		pT->Lock();
		IUnknown** pp = m_vec.begin();
		while (pp < m_vec.end())
		{
			if (*pp != NULL)
			{
				pvars[5].vt = VT_BSTR;
				pvars[5].bstrVal= URL;
				pvars[4].vt = VT_I4;
				pvars[4].lVal= Flags;
				pvars[3].vt = VT_BSTR;
				pvars[3].bstrVal= TargetFrameName;
				pvars[2].vt = VT_VARIANT | VT_BYREF;
				pvars[2].byref= PostData;
				pvars[1].vt = VT_BSTR;
				pvars[1].bstrVal= Headers;
				pvars[0].vt = VT_BOOL | VT_BYREF;
				pvars[0].byref= Cancel;
				DISPPARAMS disp = { pvars, NULL, 6, 0 };
				IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
				pDispatch->Invoke(0xc8, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
			}
			pp++;
		}
		pT->Unlock();
		delete[] pvars;
	}
	void Fire_FrameNavigateComplete(
		BSTR URL)
	{
		VARIANTARG* pvars = new VARIANTARG[1];
		for (int i = 0; i < 1; i++)
			VariantInit(&pvars[i]);
		T* pT = (T*)this;
		pT->Lock();
		IUnknown** pp = m_vec.begin();
		while (pp < m_vec.end())
		{
			if (*pp != NULL)
			{
				pvars[0].vt = VT_BSTR;
				pvars[0].bstrVal= URL;
				DISPPARAMS disp = { pvars, NULL, 1, 0 };
				IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
				pDispatch->Invoke(0xc9, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
			}
			pp++;
		}
		pT->Unlock();
		delete[] pvars;
	}
	void Fire_FrameNewWindow(
		BSTR URL,
		long Flags,
		BSTR TargetFrameName,
		VARIANT * PostData,
		BSTR Headers,
		VARIANT_BOOL * Processed)
	{
		VARIANTARG* pvars = new VARIANTARG[6];
		for (int i = 0; i < 6; i++)
			VariantInit(&pvars[i]);
		T* pT = (T*)this;
		pT->Lock();
		IUnknown** pp = m_vec.begin();
		while (pp < m_vec.end())
		{
			if (*pp != NULL)
			{
				pvars[5].vt = VT_BSTR;
				pvars[5].bstrVal= URL;
				pvars[4].vt = VT_I4;
				pvars[4].lVal= Flags;
				pvars[3].vt = VT_BSTR;
				pvars[3].bstrVal= TargetFrameName;
				pvars[2].vt = VT_VARIANT | VT_BYREF;
				pvars[2].byref= PostData;
				pvars[1].vt = VT_BSTR;
				pvars[1].bstrVal= Headers;
				pvars[0].vt = VT_BOOL | VT_BYREF;
				pvars[0].byref= Processed;
				DISPPARAMS disp = { pvars, NULL, 6, 0 };
				IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
				pDispatch->Invoke(0xcc, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
			}
			pp++;
		}
		pT->Unlock();
		delete[] pvars;
	}
	void Fire_Quit(
		VARIANT_BOOL * Cancel)
	{
		VARIANTARG* pvars = new VARIANTARG[1];
		for (int i = 0; i < 1; i++)
			VariantInit(&pvars[i]);
		T* pT = (T*)this;
		pT->Lock();
		IUnknown** pp = m_vec.begin();
		while (pp < m_vec.end())
		{
			if (*pp != NULL)
			{
				pvars[0].vt = VT_BOOL | VT_BYREF;
				pvars[0].byref= Cancel;
				DISPPARAMS disp = { pvars, NULL, 1, 0 };
				IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
				pDispatch->Invoke(0x67, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
			}
			pp++;
		}
		pT->Unlock();
		delete[] pvars;
	}
	void Fire_WindowMove()
	{
		T* pT = (T*)this;
		pT->Lock();
		IUnknown** pp = m_vec.begin();
		while (pp < m_vec.end())
		{
			if (*pp != NULL)
			{
				DISPPARAMS disp = { NULL, NULL, 0, 0 };
				IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
				pDispatch->Invoke(0x6d, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
			}
			pp++;
		}
		pT->Unlock();
	}
	void Fire_WindowResize()
	{
		T* pT = (T*)this;
		pT->Lock();
		IUnknown** pp = m_vec.begin();
		while (pp < m_vec.end())
		{
			if (*pp != NULL)
			{
				DISPPARAMS disp = { NULL, NULL, 0, 0 };
				IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
				pDispatch->Invoke(0x6e, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
			}
			pp++;
		}
		pT->Unlock();
	}
	void Fire_WindowActivate()
	{
		T* pT = (T*)this;
		pT->Lock();
		IUnknown** pp = m_vec.begin();
		while (pp < m_vec.end())
		{
			if (*pp != NULL)
			{
				DISPPARAMS disp = { NULL, NULL, 0, 0 };
				IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
				pDispatch->Invoke(0x6f, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
			}
			pp++;
		}
		pT->Unlock();
	}
	void Fire_PropertyChange(
		BSTR Property)
	{
		VARIANTARG* pvars = new VARIANTARG[1];
		for (int i = 0; i < 1; i++)
			VariantInit(&pvars[i]);
		T* pT = (T*)this;
		pT->Lock();
		IUnknown** pp = m_vec.begin();
		while (pp < m_vec.end())
		{
			if (*pp != NULL)
			{
				pvars[0].vt = VT_BSTR;
				pvars[0].bstrVal= Property;
				DISPPARAMS disp = { pvars, NULL, 1, 0 };
				IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
				pDispatch->Invoke(0x70, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
			}
			pp++;
		}
		pT->Unlock();
		delete[] pvars;
	}

};

