/*
liteFirewall is based on nsisFirewall.
Modified by Liang Sun on 19, July, 2011
http://liangsun.info/portfolio/nsis-plugin-litefirewall/
http://nsis.sourceforge.net/LiteFirewall_Plugin
http://www.msnlite.org
*/

/*
nsisFirewall -- Small NSIS plugin for simple tasks with Windows Firewall
Web site: http://wiz0u.free.fr/prog/nsisFirewall

Copyright (c) 2007-2009 Olivier Marcoux

This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.

    2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.

    3. This notice may not be removed or altered from any source distribution.
*/
#include <windows.h>
#include <tchar.h>
#include <shlwapi.h>
//#include <stdio.h>

#ifdef NSIS
#include "exdll.h"
#endif

//#import "libid:58FBCF7C-E7A9-467C-80B3-FC65E8FCCA08"
#import "netfw.tlb"
#include <netfw.h>
using namespace NetFwTypeLib;


#pragma comment( lib, "ole32.lib" )
#pragma comment( lib, "oleaut32.lib" )
// Forward declarations

#ifdef NSIS
HINSTANCE g_hInstance;
#endif

HRESULT     WFCOMInitialize(INetFwPolicy2** ppNetFwPolicy2);

HRESULT AddRule(LPCTSTR ExceptionName, LPCTSTR ProcessPath)
{
	HRESULT result = CoInitialize(NULL);
	if (FAILED(result))
		return result;
	result = REGDB_E_CLASSNOTREG;

	HRESULT hrComInit = S_OK;
	HRESULT hr = S_OK;

	INetFwPolicy2 *pNetFwPolicy2 = NULL;
	INetFwRules *pFwRules = NULL;
	INetFwRule *pFwRule = NULL;
/* Start Mozilla modification */
	INetFwRule *pFwRuleExisting = NULL;

//	long CurrentProfilesBitMask = 0;
/* End Mozilla modification */

	BSTR bstrRuleName = SysAllocString(ExceptionName);
	BSTR bstrApplicationName = SysAllocString(ProcessPath);
	BSTR bstrRuleInterfaceType = SysAllocString(L"All");

	// Initialize COM.
	hrComInit = CoInitializeEx(
		0,
		COINIT_APARTMENTTHREADED
		);

	// Ignore RPC_E_CHANGED_MODE; this just means that COM has already been
	// initialized with a different mode. Since we don't care what the mode is,
	// we'll just use the existing mode.
	if (hrComInit != RPC_E_CHANGED_MODE)
	{
		if (FAILED(hrComInit))
		{
			printf("CoInitializeEx failed: 0x%08lx\n", hrComInit);
			goto Cleanup;
		}
	}

	// Retrieve INetFwPolicy2
	hr = WFCOMInitialize(&pNetFwPolicy2);
	if (FAILED(hr))
	{
		try
		{
			INetFwMgrPtr fwMgr(L"HNetCfg.FwMgr");
			if (fwMgr)
			{
				INetFwAuthorizedApplicationPtr app(L"HNetCfg.FwAuthorizedApplication");
				if (app)
				{
					app->ProcessImageFileName = ProcessPath;
					app->Name = ExceptionName;
					app->Scope = NetFwTypeLib::NET_FW_SCOPE_ALL;
					app->IpVersion = NetFwTypeLib::NET_FW_IP_VERSION_ANY;
					app->Enabled = VARIANT_TRUE;
					fwMgr->LocalPolicy->CurrentProfile->AuthorizedApplications->Add(app);
				}
			}
		}
		catch (_com_error& e)
		{
			printf("%s", e.Error());
		}
		goto Cleanup;
	}

	// Retrieve INetFwRules
	hr = pNetFwPolicy2->get_Rules(&pFwRules);
	if (FAILED(hr))
	{
		printf("get_Rules failed: 0x%08lx\n", hr);
		goto Cleanup;
	}

/* Start Mozilla modification */
	// Don't add a new rule if there is an existing rule with the same name.
	hr = pFwRules->Item(bstrRuleName, &pFwRuleExisting);
	// Release the INetFwRule object
	if (pFwRuleExisting != NULL)
	{
		pFwRuleExisting->Release();
	}
	if (SUCCEEDED(hr))
	{
		printf("Firewall profile already exists\n");
		goto Cleanup;
	}

	// Retrieve Current Profiles bitmask
//	hr = pNetFwPolicy2->get_CurrentProfileTypes(&CurrentProfilesBitMask);
//	if (FAILED(hr))
//	{
//		printf("get_CurrentProfileTypes failed: 0x%08lx\n", hr);
//		goto Cleanup;
//	}

	// When possible we avoid adding firewall rules to the \ profile.
	// If Public is currently active and it is not the only active profile, we remove it from the bitmask
//	if ((CurrentProfilesBitMask & NET_FW_PROFILE2_PUBLIC) &&
//		(CurrentProfilesBitMask != NET_FW_PROFILE2_PUBLIC))
//	{
//		CurrentProfilesBitMask ^= NET_FW_PROFILE2_PUBLIC;
//	}

	// Create a new Firewall Rule object.
	hr = CoCreateInstance(
		__uuidof(NetFwRule),
		NULL,
		CLSCTX_INPROC_SERVER,
		__uuidof(INetFwRule),
		(void**)&pFwRule);
	if (FAILED(hr))
	{
		printf("CoCreateInstance for Firewall Rule failed: 0x%08lx\n", hr);
		goto Cleanup;
	}

    // Populate the Firewall Rule object
	pFwRule->put_Name(bstrRuleName);
	pFwRule->put_Protocol(NetFwTypeLib::NET_FW_IP_PROTOCOL_TCP);
	pFwRule->put_InterfaceTypes(bstrRuleInterfaceType);
	pFwRule->put_Profiles(NET_FW_PROFILE2_PRIVATE);
	pFwRule->put_Action(NET_FW_ACTION_ALLOW);
	pFwRule->put_Enabled(VARIANT_TRUE);

	pFwRule->put_ApplicationName(bstrApplicationName);
	// Add the Firewall Rule
	hr = pFwRules->Add(pFwRule);
	if (FAILED(hr))
	{
		printf("Firewall Rule Add failed: 0x%08lx\n", hr);
		goto Cleanup;
	}

	pFwRule->Release();
/* End Mozilla modification */

	// Create a new Firewall Rule object.
	hr = CoCreateInstance(
		__uuidof(NetFwRule),
		NULL,
		CLSCTX_INPROC_SERVER,
		__uuidof(INetFwRule),
		(void**)&pFwRule);
	if (FAILED(hr))
	{
		printf("CoCreateInstance for Firewall Rule failed: 0x%08lx\n", hr);
		goto Cleanup;
	}

	// Populate the Firewall Rule object
	pFwRule->put_Name(bstrRuleName);
/* Start Mozilla modification */
//	pFwRule->put_Protocol(NET_FW_IP_PROTOCOL_ANY);
	pFwRule->put_Protocol(NetFwTypeLib::NET_FW_IP_PROTOCOL_UDP);
/* End Mozilla modification */
	pFwRule->put_InterfaceTypes(bstrRuleInterfaceType);
/* Start Mozilla modification */
//	pFwRule->put_Profiles(CurrentProfilesBitMask);
	pFwRule->put_Profiles(NET_FW_PROFILE2_PRIVATE);
/* End Mozilla modification */
	pFwRule->put_Action(NET_FW_ACTION_ALLOW);
	pFwRule->put_Enabled(VARIANT_TRUE);

	pFwRule->put_ApplicationName(bstrApplicationName);
	// Add the Firewall Rule
	hr = pFwRules->Add(pFwRule);
	if (FAILED(hr))
	{
		printf("Firewall Rule Add failed: 0x%08lx\n", hr);
		goto Cleanup;
	}

Cleanup:

	// Free BSTR's
	SysFreeString(bstrRuleName);
	SysFreeString(bstrApplicationName);
	SysFreeString(bstrRuleInterfaceType);

	// Release the INetFwRule object
	if (pFwRule != NULL)
	{
		pFwRule->Release();
	}

	// Release the INetFwRules object
	if (pFwRules != NULL)
	{
		pFwRules->Release();
	}

	// Release the INetFwPolicy2 object
	if (pNetFwPolicy2 != NULL)
	{
		pNetFwPolicy2->Release();
	}

	CoUninitialize();
    return 0;
}

HRESULT RemoveRule(LPCTSTR ExceptionName, LPCTSTR ProcessPath)
{
    HRESULT result = CoInitialize(NULL);
	if (FAILED(result))
        return result;
	try
	{
		INetFwMgrPtr fwMgr(L"HNetCfg.FwMgr");
        if (fwMgr)
        {
		    fwMgr->LocalPolicy->CurrentProfile->AuthorizedApplications->Remove(ProcessPath);
            result = S_OK;
        }
	}
	catch (_com_error& e)
	{
		e;
	}
	HRESULT hrComInit = S_OK;
	HRESULT hr = S_OK;

	INetFwPolicy2 *pNetFwPolicy2 = NULL;
	INetFwRules *pFwRules = NULL;

/* Start Mozilla modification */
//	long CurrentProfilesBitMask = 0;
/* End Mozilla modification */

	BSTR bstrRuleName = SysAllocString(ExceptionName);

	// Retrieve INetFwPolicy2
	hr = WFCOMInitialize(&pNetFwPolicy2);
	if (FAILED(hr))
	{
		goto Cleanup;
	}

	// Retrieve INetFwRules
	hr = pNetFwPolicy2->get_Rules(&pFwRules);
	if (FAILED(hr))
	{
		printf("get_Rules failed: 0x%08lx\n", hr);
		goto Cleanup;
	}

/* Start Mozilla modification */
	// Retrieve Current Profiles bitmask
//	hr = pNetFwPolicy2->get_CurrentProfileTypes(&CurrentProfilesBitMask);
//	if (FAILED(hr))
//	{
//		printf("get_CurrentProfileTypes failed: 0x%08lx\n", hr);
//		goto Cleanup;
//	}

	// When possible we avoid adding firewall rules to the Public profile.
	// If Public is currently active and it is not the only active profile, we remove it from the bitmask
//	if ((CurrentProfilesBitMask & NET_FW_PROFILE2_PUBLIC) &&
//		(CurrentProfilesBitMask != NET_FW_PROFILE2_PUBLIC))
//	{
//		CurrentProfilesBitMask ^= NET_FW_PROFILE2_PUBLIC;
//	}
/* End Mozilla modification */

	// Remove the Firewall Rule
	hr = pFwRules->Remove(bstrRuleName);
	if (FAILED(hr))
	{
		printf("Firewall Rule Remove failed: 0x%08lx\n", hr);
		goto Cleanup;
	}

Cleanup:

	// Free BSTR's
	SysFreeString(bstrRuleName);

	// Release the INetFwRules object
	if (pFwRules != NULL)
	{
		pFwRules->Release();
	}

	// Release the INetFwPolicy2 object
	if (pNetFwPolicy2 != NULL)
	{
		pNetFwPolicy2->Release();
	}

	CoUninitialize();
	return 0;
}


#ifdef NSIS
extern "C" void __declspec(dllexport) AddRule(HWND hwndParent, int string_size, 
                                      TCHAR *variables, stack_t **stacktop)
{
	EXDLL_INIT();
	
	TCHAR ExceptionName[256], ProcessPath[MAX_PATH];
    popstring(ProcessPath);
    popstring(ExceptionName);
    HRESULT result = AddRule(ExceptionName, ProcessPath);
	// push the result back to NSIS
    TCHAR intBuffer[16];
	wsprintf(intBuffer, _T("%d"), result);
	pushstring(intBuffer);
}

extern "C" void __declspec(dllexport) RemoveRule(HWND hwndParent, int string_size, 
                                      TCHAR *variables, stack_t **stacktop)
{
	EXDLL_INIT();
	
	TCHAR ExceptionName[256], ProcessPath[MAX_PATH];
    popstring(ProcessPath);
	popstring(ExceptionName);
    HRESULT result = RemoveRule(ExceptionName, ProcessPath);
	// push the result back to NSIS
    TCHAR intBuffer[16];
	wsprintf(intBuffer, _T("%d"), result);
	pushstring(intBuffer);
}

extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD, LPVOID)
{
	g_hInstance = hInstance;
	return TRUE;
}
#endif


// Instantiate INetFwPolicy2

HRESULT WFCOMInitialize(INetFwPolicy2** ppNetFwPolicy2)
{
	HRESULT hr = S_OK;

	hr = CoCreateInstance(
		__uuidof(NetFwPolicy2), 
		NULL, 
		CLSCTX_INPROC_SERVER, 
		__uuidof(INetFwPolicy2), 
		(void**)ppNetFwPolicy2);

	if (FAILED(hr))
	{
		printf("CoCreateInstance for INetFwPolicy2 failed: 0x%08lx\n", hr);
		goto Cleanup;        
	}

Cleanup:
	return hr;
}
