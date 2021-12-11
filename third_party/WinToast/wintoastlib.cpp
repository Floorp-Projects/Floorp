/* * Copyright (C) 2016-2019 Mohammed Boujemaoui <mohabouje@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "wintoastlib.h"
#include <memory>
#include <assert.h>
#include <unordered_map>
#include <array>

#pragma comment(lib,"shlwapi")
#pragma comment(lib,"user32")

#ifdef NDEBUG
    #define DEBUG_MSG(str) do { } while ( false )
#else
    #define DEBUG_MSG(str) do { std::wcout << str << std::endl; } while( false )
#endif

#define DEFAULT_SHELL_LINKS_PATH	L"\\Microsoft\\Windows\\Start Menu\\Programs\\"
#define DEFAULT_LINK_FORMAT			L".lnk"
#define STATUS_SUCCESS (0x00000000)


// Quickstart: Handling toast activations from Win32 apps in Windows 10
// https://blogs.msdn.microsoft.com/tiles_and_toasts/2015/10/16/quickstart-handling-toast-activations-from-win32-apps-in-windows-10/
using namespace WinToastLib;
namespace DllImporter {

    // Function load a function from library
    template <typename Function>
	HRESULT loadFunctionFromLibrary(HINSTANCE library, LPCSTR name, Function &func) {
		if (!library) {
			return E_INVALIDARG;
		}
        func = reinterpret_cast<Function>(GetProcAddress(library, name));
        return (func != nullptr) ? S_OK : E_FAIL;
    }

    typedef HRESULT(FAR STDAPICALLTYPE *f_SetCurrentProcessExplicitAppUserModelID)(__in PCWSTR AppID);
    typedef HRESULT(FAR STDAPICALLTYPE *f_PropVariantToString)(_In_ REFPROPVARIANT propvar, _Out_writes_(cch) PWSTR psz, _In_ UINT cch);
    typedef HRESULT(FAR STDAPICALLTYPE *f_RoGetActivationFactory)(_In_ HSTRING activatableClassId, _In_ REFIID iid, _COM_Outptr_ void ** factory);
    typedef HRESULT(FAR STDAPICALLTYPE *f_WindowsCreateStringReference)(_In_reads_opt_(length + 1) PCWSTR sourceString, UINT32 length, _Out_ HSTRING_HEADER * hstringHeader, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING * string);
    typedef PCWSTR(FAR STDAPICALLTYPE *f_WindowsGetStringRawBuffer)(_In_ HSTRING string, _Out_ UINT32 *length);
    typedef HRESULT(FAR STDAPICALLTYPE *f_WindowsDeleteString)(_In_opt_ HSTRING string);

    static f_SetCurrentProcessExplicitAppUserModelID    SetCurrentProcessExplicitAppUserModelID;
    static f_PropVariantToString                        PropVariantToString;
    static f_RoGetActivationFactory                     RoGetActivationFactory;
    static f_WindowsCreateStringReference               WindowsCreateStringReference;
    static f_WindowsGetStringRawBuffer                  WindowsGetStringRawBuffer;
    static f_WindowsDeleteString                        WindowsDeleteString;


    template<class T>
    _Check_return_ __inline HRESULT _1_GetActivationFactory(_In_ HSTRING activatableClassId, _COM_Outptr_ T** factory) {
        return RoGetActivationFactory(activatableClassId, IID_INS_ARGS(factory));
    }

    template<typename T>
    inline HRESULT Wrap_GetActivationFactory(_In_ HSTRING activatableClassId, _Inout_ Details::ComPtrRef<T> factory) noexcept {
        return _1_GetActivationFactory(activatableClassId, factory.ReleaseAndGetAddressOf());
    }

    inline HRESULT initialize() {
        HINSTANCE LibShell32 = LoadLibraryW(L"SHELL32.DLL");
        HRESULT hr = loadFunctionFromLibrary(LibShell32, "SetCurrentProcessExplicitAppUserModelID", SetCurrentProcessExplicitAppUserModelID);
        if (SUCCEEDED(hr)) {
            HINSTANCE LibPropSys = LoadLibraryW(L"PROPSYS.DLL");
            hr = loadFunctionFromLibrary(LibPropSys, "PropVariantToString", PropVariantToString);
            if (SUCCEEDED(hr)) {
                HINSTANCE LibComBase = LoadLibraryW(L"COMBASE.DLL");
                const bool succeded = SUCCEEDED(loadFunctionFromLibrary(LibComBase, "RoGetActivationFactory", RoGetActivationFactory))
										&& SUCCEEDED(loadFunctionFromLibrary(LibComBase, "WindowsCreateStringReference", WindowsCreateStringReference))
										&& SUCCEEDED(loadFunctionFromLibrary(LibComBase, "WindowsGetStringRawBuffer", WindowsGetStringRawBuffer))
										&& SUCCEEDED(loadFunctionFromLibrary(LibComBase, "WindowsDeleteString", WindowsDeleteString));
				return succeded ? S_OK : E_FAIL;
            }
        }
        return hr;
    }
}

class WinToastStringWrapper {
public:
    WinToastStringWrapper(_In_reads_(length) PCWSTR stringRef, _In_ UINT32 length) noexcept {
        HRESULT hr = DllImporter::WindowsCreateStringReference(stringRef, length, &_header, &_hstring);
        if (!SUCCEEDED(hr)) {
            RaiseException(static_cast<DWORD>(STATUS_INVALID_PARAMETER), EXCEPTION_NONCONTINUABLE, 0, nullptr);
        }
    }

    WinToastStringWrapper(_In_ const std::wstring &stringRef) noexcept {
        HRESULT hr = DllImporter::WindowsCreateStringReference(stringRef.c_str(), static_cast<UINT32>(stringRef.length()), &_header, &_hstring);
        if (FAILED(hr)) {
            RaiseException(static_cast<DWORD>(STATUS_INVALID_PARAMETER), EXCEPTION_NONCONTINUABLE, 0, nullptr);
        }
    }

    ~WinToastStringWrapper() {
        DllImporter::WindowsDeleteString(_hstring);
    }

    inline HSTRING Get() const noexcept {
        return _hstring;
    }
private:
    HSTRING _hstring;
    HSTRING_HEADER _header;

};

class InternalDateTime : public IReference<DateTime> {
public:
    static INT64 Now() {
        FILETIME now;
        GetSystemTimeAsFileTime(&now);
        return ((((INT64)now.dwHighDateTime) << 32) | now.dwLowDateTime);
    }

    InternalDateTime(DateTime dateTime) : _dateTime(dateTime) {}

    InternalDateTime(INT64 millisecondsFromNow) {
        _dateTime.UniversalTime = Now() + millisecondsFromNow * 10000;
    }

    virtual ~InternalDateTime() = default;

    operator INT64() {
        return _dateTime.UniversalTime;
    }

    HRESULT STDMETHODCALLTYPE get_Value(DateTime *dateTime) {
        *dateTime = _dateTime;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID& riid, void** ppvObject) {
        if (!ppvObject) {
            return E_POINTER;
        }
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IReference<DateTime>)) {
            *ppvObject = static_cast<IUnknown*>(static_cast<IReference<DateTime>*>(this));
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE Release() {
        return 1;
    }

    ULONG STDMETHODCALLTYPE AddRef() {
        return 2;
    }

    HRESULT STDMETHODCALLTYPE GetIids(ULONG*, IID**) {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING*) {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel*) {
        return E_NOTIMPL;
    }

protected:
    DateTime _dateTime;
};

namespace Util {

    typedef LONG NTSTATUS, *PNTSTATUS;
    typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    inline RTL_OSVERSIONINFOW getRealOSVersion() {
        HMODULE hMod = ::GetModuleHandleW(L"ntdll.dll");
        if (hMod) {
            RtlGetVersionPtr fxPtr = (RtlGetVersionPtr)::GetProcAddress(hMod, "RtlGetVersion");
            if (fxPtr != nullptr) {
                RTL_OSVERSIONINFOW rovi = { 0 };
                rovi.dwOSVersionInfoSize = sizeof(rovi);
                if (STATUS_SUCCESS == fxPtr(&rovi)) {
                    return rovi;
                }
            }
        }
        RTL_OSVERSIONINFOW rovi = { 0 };
        return rovi;
    }

    inline HRESULT defaultExecutablePath(_In_ WCHAR* path, _In_ DWORD nSize = MAX_PATH) {
        DWORD written = GetModuleFileNameExW(GetCurrentProcess(), nullptr, path, nSize);
        DEBUG_MSG("Default executable path: " << path);
        return (written > 0) ? S_OK : E_FAIL;
    }


    inline HRESULT commonShellLinksDirectory(_In_ const WCHAR* baseEnv, _In_ WCHAR* path, _In_ DWORD nSize) {
        DWORD written = GetEnvironmentVariableW(baseEnv, path, nSize);
        HRESULT hr = written > 0 ? S_OK : E_INVALIDARG;
        if (SUCCEEDED(hr)) {
            errno_t result = wcscat_s(path, nSize, DEFAULT_SHELL_LINKS_PATH);
            hr = (result == 0) ? S_OK : E_INVALIDARG;
            DEBUG_MSG("Default shell link path: " << path);
        }
        return hr;
    }

    inline HRESULT commonShellLinkPath(_In_ const WCHAR* baseEnv, const std::wstring& appname, _In_ WCHAR* path, _In_ DWORD nSize) {
        HRESULT hr = commonShellLinksDirectory(baseEnv, path, nSize);
        if (SUCCEEDED(hr)) {
            const std::wstring appLink(appname + DEFAULT_LINK_FORMAT);
            errno_t result = wcscat_s(path, nSize, appLink.c_str());
            hr = (result == 0) ? S_OK : E_INVALIDARG;
            DEBUG_MSG("Default shell link file path: " << path);
        }
        return hr;
    }

    inline HRESULT defaultUserShellLinkPath(const std::wstring& appname, _In_ WCHAR* path, _In_ DWORD nSize = MAX_PATH) {
      return commonShellLinkPath(L"APPDATA", appname, path, nSize);
    }

    inline HRESULT defaultSystemShellLinkPath(const std::wstring& appname, _In_ WCHAR* path, _In_ DWORD nSize = MAX_PATH) {
      return commonShellLinkPath(L"PROGRAMDATA", appname, path, nSize);
    }

    inline PCWSTR AsString(ComPtr<IXmlDocument> &xmlDocument) {
        HSTRING xml;
        ComPtr<IXmlNodeSerializer> ser;
        HRESULT hr = xmlDocument.As<IXmlNodeSerializer>(&ser);
        hr = ser->GetXml(&xml);
        if (SUCCEEDED(hr))
            return DllImporter::WindowsGetStringRawBuffer(xml, nullptr);
        return nullptr;
    }

    inline PCWSTR AsString(HSTRING hstring) {
        return DllImporter::WindowsGetStringRawBuffer(hstring, nullptr);
    }

    inline HRESULT setNodeStringValue(const std::wstring& string, IXmlNode *node, IXmlDocument *xml) {
        ComPtr<IXmlText> textNode;
        HRESULT hr = xml->CreateTextNode( WinToastStringWrapper(string).Get(), &textNode);
        if (SUCCEEDED(hr)) {
            ComPtr<IXmlNode> stringNode;
            hr = textNode.As(&stringNode);
            if (SUCCEEDED(hr)) {
                ComPtr<IXmlNode> appendedChild;
                hr = node->AppendChild(stringNode.Get(), &appendedChild);
            }
        }
        return hr;
    }

    inline HRESULT setEventHandlers(_In_ IToastNotification* notification, _In_ std::shared_ptr<IWinToastHandler> eventHandler, _In_ INT64 expirationTime) {
        EventRegistrationToken activatedToken, dismissedToken, failedToken;
        HRESULT hr = notification->add_Activated(
                    Callback < Implements < RuntimeClassFlags<ClassicCom>,
                    ITypedEventHandler<ToastNotification*, IInspectable* >> >(
                    [eventHandler](IToastNotification*, IInspectable* inspectable)
                {
                    IToastActivatedEventArgs *activatedEventArgs;
                    HRESULT hr = inspectable->QueryInterface(&activatedEventArgs);
                    if (SUCCEEDED(hr)) {
                        HSTRING argumentsHandle;
                        hr = activatedEventArgs->get_Arguments(&argumentsHandle);
                        if (SUCCEEDED(hr)) {
                            PCWSTR arguments = Util::AsString(argumentsHandle);
                            if (arguments && *arguments) {
                                eventHandler->toastActivated(static_cast<int>(wcstol(arguments, nullptr, 10)));
                                return S_OK;
                            }
                        }
                    }
                    eventHandler->toastActivated();
                    return S_OK;
                }).Get(), &activatedToken);

        if (SUCCEEDED(hr)) {
            hr = notification->add_Dismissed(Callback < Implements < RuntimeClassFlags<ClassicCom>,
                     ITypedEventHandler<ToastNotification*, ToastDismissedEventArgs* >> >(
                     [eventHandler, expirationTime](IToastNotification*, IToastDismissedEventArgs* e)
                 {
                     ToastDismissalReason reason;
                     if (SUCCEEDED(e->get_Reason(&reason)))
                     {
                         if (reason == ToastDismissalReason_UserCanceled && expirationTime && InternalDateTime::Now() >= expirationTime)
                            reason = ToastDismissalReason_TimedOut;
                         eventHandler->toastDismissed(static_cast<IWinToastHandler::WinToastDismissalReason>(reason));
                     }
                     return S_OK;
                 }).Get(), &dismissedToken);
            if (SUCCEEDED(hr)) {
                hr = notification->add_Failed(Callback < Implements < RuntimeClassFlags<ClassicCom>,
                    ITypedEventHandler<ToastNotification*, ToastFailedEventArgs* >> >(
                    [eventHandler](IToastNotification*, IToastFailedEventArgs*)
                {
                    eventHandler->toastFailed();
                    return S_OK;
                }).Get(), &failedToken);
            }
        }
        return hr;
    }

    inline HRESULT addAttribute(_In_ IXmlDocument *xml, const std::wstring &name, IXmlNamedNodeMap *attributeMap) {
        ComPtr<ABI::Windows::Data::Xml::Dom::IXmlAttribute> srcAttribute;
        HRESULT hr = xml->CreateAttribute(WinToastStringWrapper(name).Get(), &srcAttribute);
        if (SUCCEEDED(hr)) {
            ComPtr<IXmlNode> node;
            hr = srcAttribute.As(&node);
            if (SUCCEEDED(hr)) {
                ComPtr<IXmlNode> pNode;
                hr = attributeMap->SetNamedItem(node.Get(), &pNode);
            }
        }
        return hr;
    }

    inline HRESULT createElement(_In_ IXmlDocument *xml, _In_ const std::wstring& root_node, _In_ const std::wstring& element_name, _In_ const std::vector<std::wstring>& attribute_names) {
        ComPtr<IXmlNodeList> rootList;
        HRESULT hr = xml->GetElementsByTagName(WinToastStringWrapper(root_node).Get(), &rootList);
        if (SUCCEEDED(hr)) {
            ComPtr<IXmlNode> root;
            hr = rootList->Item(0, &root);
            if (SUCCEEDED(hr)) {
                ComPtr<ABI::Windows::Data::Xml::Dom::IXmlElement> audioElement;
                hr = xml->CreateElement(WinToastStringWrapper(element_name).Get(), &audioElement);
                if (SUCCEEDED(hr)) {
                    ComPtr<IXmlNode> audioNodeTmp;
                    hr = audioElement.As(&audioNodeTmp);
                    if (SUCCEEDED(hr)) {
                        ComPtr<IXmlNode> audioNode;
                        hr = root->AppendChild(audioNodeTmp.Get(), &audioNode);
                        if (SUCCEEDED(hr)) {
                            ComPtr<IXmlNamedNodeMap> attributes;
                            hr = audioNode->get_Attributes(&attributes);
                            if (SUCCEEDED(hr)) {
                                for (const auto& it : attribute_names) {
                                    hr = addAttribute(xml, it, attributes.Get());
                                }
                            }
                        }
                    }
                }
            }
        }
        return hr;
    }
}

WinToast* WinToast::instance() {
    static WinToast instance;
    return &instance;
}

WinToast::WinToast() :
    _isInitialized(false),
    _hasCoInitialized(false)
{
	if (!isCompatible()) {
		DEBUG_MSG(L"Warning: Your system is not compatible with this library ");
	}
}

WinToast::~WinToast() {
    if (_hasCoInitialized) {
        CoUninitialize();
    }
}

void WinToast::setAppName(_In_ const std::wstring& appName) {
    _appName = appName;
}


void WinToast::setAppUserModelId(_In_ const std::wstring& aumi) {
    _aumi = aumi;
    DEBUG_MSG(L"Default App User Model Id: " << _aumi.c_str());
}

void WinToast::setShortcutPolicy(_In_ ShortcutPolicy shortcutPolicy) {
    _shortcutPolicy = shortcutPolicy;
}

bool WinToast::isCompatible() {
	DllImporter::initialize();
    return !((DllImporter::SetCurrentProcessExplicitAppUserModelID == nullptr)
        || (DllImporter::PropVariantToString == nullptr)
        || (DllImporter::RoGetActivationFactory == nullptr)
        || (DllImporter::WindowsCreateStringReference == nullptr)
        || (DllImporter::WindowsDeleteString == nullptr));
}

bool WinToastLib::WinToast::isSupportingModernFeatures() {
    constexpr auto MinimumSupportedVersion = 6;
    return Util::getRealOSVersion().dwMajorVersion > MinimumSupportedVersion;

}
std::wstring WinToast::configureAUMI(_In_ const std::wstring &companyName,
                                               _In_ const std::wstring &productName,
                                               _In_ const std::wstring &subProduct,
                                               _In_ const std::wstring &versionInformation)
{
    std::wstring aumi = companyName;
    aumi += L"." + productName;
    if (subProduct.length() > 0) {
        aumi += L"." + subProduct;
        if (versionInformation.length() > 0) {
            aumi += L"." + versionInformation;
        }
    }

    if (aumi.length() > SCHAR_MAX) {
        DEBUG_MSG("Error: max size allowed for AUMI: 128 characters.");
    }
    return aumi;
}

const std::wstring& WinToast::strerror(WinToastError error) {
    static const std::unordered_map<WinToastError, std::wstring> Labels = {
        {WinToastError::NoError, L"No error. The process was executed correctly"},
        {WinToastError::NotInitialized, L"The library has not been initialized"},
        {WinToastError::SystemNotSupported, L"The OS does not support WinToast"},
        {WinToastError::ShellLinkNotCreated, L"The library was not able to create a Shell Link for the app"},
        {WinToastError::InvalidAppUserModelID, L"The AUMI is not a valid one"},
        {WinToastError::InvalidParameters, L"The parameters used to configure the library are not valid normally because an invalid AUMI or App Name"},
        {WinToastError::NotDisplayed, L"The toast was created correctly but WinToast was not able to display the toast"},
        {WinToastError::UnknownError, L"Unknown error"}
    };

    const auto iter = Labels.find(error);
    assert(iter != Labels.end());
    return iter->second;
}

enum WinToast::ShortcutResult WinToast::createShortcut() {
    if (_aumi.empty() || _appName.empty()) {
        DEBUG_MSG(L"Error: App User Model Id or Appname is empty!");
        return SHORTCUT_MISSING_PARAMETERS;
    }

    if (!isCompatible()) {
        DEBUG_MSG(L"Your OS is not compatible with this library! =(");
        return SHORTCUT_INCOMPATIBLE_OS;
    }

    if (!_hasCoInitialized) {
        HRESULT initHr = CoInitializeEx(nullptr, COINIT::COINIT_MULTITHREADED);
        if (initHr != RPC_E_CHANGED_MODE) {
            if (FAILED(initHr) && initHr != S_FALSE) {
                DEBUG_MSG(L"Error on COM library initialization!");
                return SHORTCUT_COM_INIT_FAILURE;
            }
            else {
                _hasCoInitialized = true;
            }
        }
    }

    bool wasChanged;
    HRESULT hr = validateShellLinkHelper(wasChanged);
    if (SUCCEEDED(hr))
        return wasChanged ? SHORTCUT_WAS_CHANGED : SHORTCUT_UNCHANGED;

    hr = createShellLinkHelper();
    return SUCCEEDED(hr) ? SHORTCUT_WAS_CREATED : SHORTCUT_CREATE_FAILED;
}

bool WinToast::initialize(_Out_ WinToastError* error) {
    _isInitialized = false;
    setError(error, WinToastError::NoError);

    if (!isCompatible()) {
        setError(error, WinToastError::SystemNotSupported);
        DEBUG_MSG(L"Error: system not supported.");
        return false;
    }


    if (_aumi.empty() || _appName.empty()) {
        setError(error, WinToastError::InvalidParameters);
        DEBUG_MSG(L"Error while initializing, did you set up a valid AUMI and App name?");
        return false;
    }

    if (_shortcutPolicy != SHORTCUT_POLICY_IGNORE) {
        if (createShortcut() < 0) {
            setError(error, WinToastError::ShellLinkNotCreated);
            DEBUG_MSG(L"Error while attaching the AUMI to the current proccess =(");
            return false;
        }
    }

    if (FAILED(DllImporter::SetCurrentProcessExplicitAppUserModelID(_aumi.c_str()))) {
        setError(error, WinToastError::InvalidAppUserModelID);
        DEBUG_MSG(L"Error while attaching the AUMI to the current proccess =(");
        return false;
    }

    _isInitialized = true;
    return _isInitialized;
}

bool WinToast::isInitialized() const {
    return _isInitialized;
}

const std::wstring& WinToast::appName() const {
    return _appName;
}

const std::wstring& WinToast::appUserModelId() const {
    return _aumi;
}


HRESULT	WinToast::validateShellLinkHelper(_Out_ bool& wasChanged) {
	WCHAR	path[MAX_PATH] = { L'\0' };
    Util::defaultUserShellLinkPath(_appName, path);
    // Check if the file exist
    DWORD attr = GetFileAttributesW(path);
    if (attr >= 0xFFFFFFF) {
        // The shortcut may be in the system Start Menu.
        WCHAR   systemPath[MAX_PATH] = { L'\0' };
        Util::defaultSystemShellLinkPath(_appName, systemPath);
        attr = GetFileAttributesW(systemPath);
        if (attr >= 0xFFFFFFF) {
            DEBUG_MSG("Error, shell link not found. Try to create a new one in: " << path);
            return E_FAIL;
        }
        wcscpy(path, systemPath);
    }

    // Let's load the file as shell link to validate.
    // - Create a shell link
    // - Create a persistant file
    // - Load the path as data for the persistant file
    // - Read the property AUMI and validate with the current
    // - Review if AUMI is equal.
    ComPtr<IShellLink> shellLink;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink));
    if (SUCCEEDED(hr)) {
        ComPtr<IPersistFile> persistFile;
        hr = shellLink.As(&persistFile);
        if (SUCCEEDED(hr)) {
            hr = persistFile->Load(path, STGM_READ);
            if (SUCCEEDED(hr)) {
                ComPtr<IPropertyStore> propertyStore;
                hr = shellLink.As(&propertyStore);
                if (SUCCEEDED(hr)) {
                    PROPVARIANT appIdPropVar;
                    hr = propertyStore->GetValue(PKEY_AppUserModel_ID, &appIdPropVar);
                    if (SUCCEEDED(hr)) {
                        WCHAR AUMI[MAX_PATH];
                        hr = DllImporter::PropVariantToString(appIdPropVar, AUMI, MAX_PATH);
                        wasChanged = false;
                        if (FAILED(hr) || _aumi != AUMI) {
                            if (_shortcutPolicy == SHORTCUT_POLICY_REQUIRE_CREATE) {
                                // AUMI Changed for the same app, let's update the current value! =)
                                wasChanged = true;
                                PropVariantClear(&appIdPropVar);
                                hr = InitPropVariantFromString(_aumi.c_str(), &appIdPropVar);
                                if (SUCCEEDED(hr)) {
                                    hr = propertyStore->SetValue(PKEY_AppUserModel_ID, appIdPropVar);
                                    if (SUCCEEDED(hr)) {
                                        hr = propertyStore->Commit();
                                        if (SUCCEEDED(hr) && SUCCEEDED(persistFile->IsDirty())) {
                                            hr = persistFile->Save(path, TRUE);
                                        }
                                    }
                                }
                            } else {
                                // Not allowed to touch the shortcut to fix the AUMI
                                hr = E_FAIL;
                            }
                        }
                        PropVariantClear(&appIdPropVar);
                    }
                }
            }
        }
    }
    return hr;
}



HRESULT	WinToast::createShellLinkHelper() {
    if (_shortcutPolicy != SHORTCUT_POLICY_REQUIRE_CREATE) {
      return E_FAIL;
    }

	WCHAR   exePath[MAX_PATH]{L'\0'};
	WCHAR	slPath[MAX_PATH]{L'\0'};
    Util::defaultUserShellLinkPath(_appName, slPath);
    Util::defaultExecutablePath(exePath);
    ComPtr<IShellLinkW> shellLink;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink));
    if (SUCCEEDED(hr)) {
        hr = shellLink->SetPath(exePath);
        if (SUCCEEDED(hr)) {
            hr = shellLink->SetArguments(L"");
            if (SUCCEEDED(hr)) {
                hr = shellLink->SetWorkingDirectory(exePath);
                if (SUCCEEDED(hr)) {
                    ComPtr<IPropertyStore> propertyStore;
                    hr = shellLink.As(&propertyStore);
                    if (SUCCEEDED(hr)) {
                        PROPVARIANT appIdPropVar;
                        hr = InitPropVariantFromString(_aumi.c_str(), &appIdPropVar);
                        if (SUCCEEDED(hr)) {
                            hr = propertyStore->SetValue(PKEY_AppUserModel_ID, appIdPropVar);
                            if (SUCCEEDED(hr)) {
                                hr = propertyStore->Commit();
                                if (SUCCEEDED(hr)) {
                                    ComPtr<IPersistFile> persistFile;
                                    hr = shellLink.As(&persistFile);
                                    if (SUCCEEDED(hr)) {
                                        hr = persistFile->Save(slPath, TRUE);
                                    }
                                }
                            }
                            PropVariantClear(&appIdPropVar);
                        }
                    }
                }
            }
        }
    }
    return hr;
}

INT64 WinToast::showToast(_In_ const WinToastTemplate& toast, _In_  IWinToastHandler* handler, _Out_ WinToastError* error)  {
    setError(error, WinToastError::NoError);
    INT64 id = -1;
    if (!isInitialized()) {
        setError(error, WinToastError::NotInitialized);
        DEBUG_MSG("Error when launching the toast. WinToast is not initialized.");
        return id;
    }
    if (!handler) {
        setError(error, WinToastError::InvalidHandler);
        DEBUG_MSG("Error when launching the toast. Handler cannot be nullptr.");
        return id;
    }

    ComPtr<IToastNotificationManagerStatics> notificationManager;
    HRESULT hr = DllImporter::Wrap_GetActivationFactory(WinToastStringWrapper(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager).Get(), &notificationManager);
    if (SUCCEEDED(hr)) {
        ComPtr<IToastNotifier> notifier;
        hr = notificationManager->CreateToastNotifierWithId(WinToastStringWrapper(_aumi).Get(), &notifier);
        if (SUCCEEDED(hr)) {
            ComPtr<IToastNotificationFactory> notificationFactory;
            hr = DllImporter::Wrap_GetActivationFactory(WinToastStringWrapper(RuntimeClass_Windows_UI_Notifications_ToastNotification).Get(), &notificationFactory);
            if (SUCCEEDED(hr)) {
				ComPtr<IXmlDocument> xmlDocument;
				HRESULT hr = notificationManager->GetTemplateContent(ToastTemplateType(toast.type()), &xmlDocument);
                if (SUCCEEDED(hr)) {
                    for (std::size_t i = 0, fieldsCount = toast.textFieldsCount(); i < fieldsCount && SUCCEEDED(hr); i++) {
                        hr = setTextFieldHelper(xmlDocument.Get(), toast.textField(WinToastTemplate::TextField(i)), i);
                    }

                    // Modern feature are supported Windows > Windows 10
                    if (SUCCEEDED(hr) && isSupportingModernFeatures()) {

                        // Note that we do this *after* using toast.textFieldsCount() to
                        // iterate/fill the template's text fields, since we're adding yet another text field.
                        if (SUCCEEDED(hr)
                            && !toast.attributionText().empty()) {
                            hr = setAttributionTextFieldHelper(xmlDocument.Get(), toast.attributionText());
                        }

                        std::array<WCHAR, 12> buf;
                        for (std::size_t i = 0, actionsCount = toast.actionsCount(); i < actionsCount && SUCCEEDED(hr); i++) {
                            _snwprintf_s(buf.data(), buf.size(), _TRUNCATE, L"%zd", i);
                            hr = addActionHelper(xmlDocument.Get(), toast.actionLabel(i), buf.data());
                        }

                        if (SUCCEEDED(hr)) {
                            hr = (toast.audioPath().empty() && toast.audioOption() == WinToastTemplate::AudioOption::Default)
                                ? hr : setAudioFieldHelper(xmlDocument.Get(), toast.audioPath(), toast.audioOption());
                        }

                        if (SUCCEEDED(hr) && toast.duration() != WinToastTemplate::Duration::System) {
                            hr = addDurationHelper(xmlDocument.Get(),
                                (toast.duration() == WinToastTemplate::Duration::Short) ? L"short" : L"long");
                        }

                    } else {
                        DEBUG_MSG("Modern features (Actions/Sounds/Attributes) not supported in this os version");
                    }

                    if (SUCCEEDED(hr)) {
                        hr = toast.hasImage() ? setImageFieldHelper(xmlDocument.Get(), toast.imagePath()) : hr;
                        if (SUCCEEDED(hr)) {
                            ComPtr<IToastNotification> notification;
                            hr = notificationFactory->CreateToastNotification(xmlDocument.Get(), &notification);
                            if (SUCCEEDED(hr)) {
                                INT64 expiration = 0, relativeExpiration = toast.expiration();
                                if (relativeExpiration > 0) {
                                    InternalDateTime expirationDateTime(relativeExpiration);
                                    expiration = expirationDateTime;
                                    hr = notification->put_ExpirationTime(&expirationDateTime);
                                }

                                if (SUCCEEDED(hr)) {
                                    hr = Util::setEventHandlers(notification.Get(), std::shared_ptr<IWinToastHandler>(handler), expiration);
                                    if (FAILED(hr)) {
                                        setError(error, WinToastError::InvalidHandler);
                                    }
                                }

                                if (SUCCEEDED(hr)) {
                                    GUID guid;
                                    hr = CoCreateGuid(&guid);
                                    if (SUCCEEDED(hr)) {
                                        id = guid.Data1;
                                        _buffer[id] = notification;
                                        DEBUG_MSG("xml: " << Util::AsString(xmlDocument));
                                        hr = notifier->Show(notification.Get());
                                        if (FAILED(hr)) {
                                            setError(error, WinToastError::NotDisplayed);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return FAILED(hr) ? -1 : id;
}

ComPtr<IToastNotifier> WinToast::notifier(_In_ bool* succeded) const  {
	ComPtr<IToastNotificationManagerStatics> notificationManager;
	ComPtr<IToastNotifier> notifier;
	HRESULT hr = DllImporter::Wrap_GetActivationFactory(WinToastStringWrapper(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager).Get(), &notificationManager);
	if (SUCCEEDED(hr)) {
		hr = notificationManager->CreateToastNotifierWithId(WinToastStringWrapper(_aumi).Get(), &notifier);
	}
	*succeded = SUCCEEDED(hr);
	return notifier;
}

bool WinToast::hideToast(_In_ INT64 id) {
    if (!isInitialized()) {
        DEBUG_MSG("Error when hiding the toast. WinToast is not initialized.");
        return false;
    }

    if (_buffer.find(id) != _buffer.end()) {
        auto succeded = false;
        auto notify = notifier(&succeded);
		if (succeded) {
            auto result = notify->Hide(_buffer[id].Get());
            _buffer.erase(id);
            return SUCCEEDED(result);
		}
	}
    return false;
}

void WinToast::clear() {
    auto succeded = false;
    auto notify = notifier(&succeded);
	if (succeded) {
		auto end = _buffer.end();
		for (auto it = _buffer.begin(); it != end; ++it) {
			notify->Hide(it->second.Get());
		}
        _buffer.clear();
	}
}

//
// Available as of Windows 10 Anniversary Update
// Ref: https://docs.microsoft.com/en-us/windows/uwp/design/shell/tiles-and-notifications/adaptive-interactive-toasts
//
// NOTE: This will add a new text field, so be aware when iterating over
//       the toast's text fields or getting a count of them.
//
HRESULT WinToast::setAttributionTextFieldHelper(_In_ IXmlDocument *xml, _In_ const std::wstring& text) {
    Util::createElement(xml, L"binding", L"text", { L"placement" });
    ComPtr<IXmlNodeList> nodeList;
    HRESULT hr = xml->GetElementsByTagName(WinToastStringWrapper(L"text").Get(), &nodeList);
    if (SUCCEEDED(hr)) {
        UINT32 nodeListLength;
        hr = nodeList->get_Length(&nodeListLength);
        if (SUCCEEDED(hr)) {
            for (UINT32 i = 0; i < nodeListLength; i++) {
                ComPtr<IXmlNode> textNode;
                hr = nodeList->Item(i, &textNode);
                if (SUCCEEDED(hr)) {
                    ComPtr<IXmlNamedNodeMap> attributes;
                    hr = textNode->get_Attributes(&attributes);
                    if (SUCCEEDED(hr)) {
                        ComPtr<IXmlNode> editedNode;
                        if (SUCCEEDED(hr)) {
                            hr = attributes->GetNamedItem(WinToastStringWrapper(L"placement").Get(), &editedNode);
                            if (FAILED(hr) || !editedNode) {
                                continue;
                            }
                            hr = Util::setNodeStringValue(L"attribution", editedNode.Get(), xml);
                            if (SUCCEEDED(hr)) {
                                return setTextFieldHelper(xml, text, i);
                            }
                        }
                    }
                }
            }
        }
    }
    return hr;
}

HRESULT WinToast::addDurationHelper(_In_ IXmlDocument *xml, _In_ const std::wstring& duration) {
    ComPtr<IXmlNodeList> nodeList;
    HRESULT hr = xml->GetElementsByTagName(WinToastStringWrapper(L"toast").Get(), &nodeList);
    if (SUCCEEDED(hr)) {
        UINT32 length;
        hr = nodeList->get_Length(&length);
        if (SUCCEEDED(hr)) {
            ComPtr<IXmlNode> toastNode;
            hr = nodeList->Item(0, &toastNode);
            if (SUCCEEDED(hr)) {
                ComPtr<IXmlElement> toastElement;
                hr = toastNode.As(&toastElement);
                if (SUCCEEDED(hr)) {
                    hr = toastElement->SetAttribute(WinToastStringWrapper(L"duration").Get(),
                                                    WinToastStringWrapper(duration).Get());
                }
            }
        }
    }
    return hr;
}

HRESULT WinToast::setTextFieldHelper(_In_ IXmlDocument *xml, _In_ const std::wstring& text, _In_ UINT32 pos) {
    ComPtr<IXmlNodeList> nodeList;
    HRESULT hr = xml->GetElementsByTagName(WinToastStringWrapper(L"text").Get(), &nodeList);
    if (SUCCEEDED(hr)) {
        ComPtr<IXmlNode> node;
        hr = nodeList->Item(pos, &node);
        if (SUCCEEDED(hr)) {
            hr = Util::setNodeStringValue(text, node.Get(), xml);
        }
    }
    return hr;
}


HRESULT WinToast::setImageFieldHelper(_In_ IXmlDocument *xml, _In_ const std::wstring& path)  {
    assert(path.size() < MAX_PATH);

    wchar_t imagePath[MAX_PATH] = L"file:///";
    HRESULT hr = StringCchCatW(imagePath, MAX_PATH, path.c_str());
    if (SUCCEEDED(hr)) {
        ComPtr<IXmlNodeList> nodeList;
        HRESULT hr = xml->GetElementsByTagName(WinToastStringWrapper(L"image").Get(), &nodeList);
        if (SUCCEEDED(hr)) {
            ComPtr<IXmlNode> node;
            hr = nodeList->Item(0, &node);
            if (SUCCEEDED(hr))  {
                ComPtr<IXmlNamedNodeMap> attributes;
                hr = node->get_Attributes(&attributes);
                if (SUCCEEDED(hr)) {
                    ComPtr<IXmlNode> editedNode;
                    hr = attributes->GetNamedItem(WinToastStringWrapper(L"src").Get(), &editedNode);
                    if (SUCCEEDED(hr)) {
                        Util::setNodeStringValue(imagePath, editedNode.Get(), xml);
                    }
                }
            }
        }
    }
    return hr;
}

HRESULT WinToast::setAudioFieldHelper(_In_ IXmlDocument *xml, _In_ const std::wstring& path, _In_opt_ WinToastTemplate::AudioOption option) {
    std::vector<std::wstring> attrs;
    if (!path.empty()) attrs.push_back(L"src");
    if (option == WinToastTemplate::AudioOption::Loop) attrs.push_back(L"loop");
    if (option == WinToastTemplate::AudioOption::Silent) attrs.push_back(L"silent");
    Util::createElement(xml, L"toast", L"audio", attrs);

    ComPtr<IXmlNodeList> nodeList;
    HRESULT hr = xml->GetElementsByTagName(WinToastStringWrapper(L"audio").Get(), &nodeList);
    if (SUCCEEDED(hr)) {
        ComPtr<IXmlNode> node;
        hr = nodeList->Item(0, &node);
        if (SUCCEEDED(hr)) {
            ComPtr<IXmlNamedNodeMap> attributes;
            hr = node->get_Attributes(&attributes);
            if (SUCCEEDED(hr)) {
                ComPtr<IXmlNode> editedNode;
                if (!path.empty()) {
                    if (SUCCEEDED(hr)) {
                        hr = attributes->GetNamedItem(WinToastStringWrapper(L"src").Get(), &editedNode);
                        if (SUCCEEDED(hr)) {
                            hr = Util::setNodeStringValue(path, editedNode.Get(), xml);
                        }
                    }
                }

                if (SUCCEEDED(hr)) {
                    switch (option) {
                    case WinToastTemplate::AudioOption::Loop:
                        hr = attributes->GetNamedItem(WinToastStringWrapper(L"loop").Get(), &editedNode);
                        if (SUCCEEDED(hr)) {
                            hr = Util::setNodeStringValue(L"true", editedNode.Get(), xml);
                        }
                        break;
                    case WinToastTemplate::AudioOption::Silent:
                        hr = attributes->GetNamedItem(WinToastStringWrapper(L"silent").Get(), &editedNode);
                        if (SUCCEEDED(hr)) {
                            hr = Util::setNodeStringValue(L"true", editedNode.Get(), xml);
                        }
                    default:
                        break;
                    }
                }
            }
        }
    }
    return hr;
}

HRESULT WinToast::addActionHelper(_In_ IXmlDocument *xml, _In_ const std::wstring& content, _In_ const std::wstring& arguments) {
	ComPtr<IXmlNodeList> nodeList;
	HRESULT hr = xml->GetElementsByTagName(WinToastStringWrapper(L"actions").Get(), &nodeList);
    if (SUCCEEDED(hr)) {
        UINT32 length;
        hr = nodeList->get_Length(&length);
        if (SUCCEEDED(hr)) {
            ComPtr<IXmlNode> actionsNode;
            if (length > 0) {
                hr = nodeList->Item(0, &actionsNode);
            } else {
                hr = xml->GetElementsByTagName(WinToastStringWrapper(L"toast").Get(), &nodeList);
                if (SUCCEEDED(hr)) {
                    hr = nodeList->get_Length(&length);
                    if (SUCCEEDED(hr)) {
                        ComPtr<IXmlNode> toastNode;
                        hr = nodeList->Item(0, &toastNode);
                        if (SUCCEEDED(hr)) {
                            ComPtr<IXmlElement> toastElement;
                            hr = toastNode.As(&toastElement);
                            if (SUCCEEDED(hr))
                                        hr = toastElement->SetAttribute(WinToastStringWrapper(L"template").Get(), WinToastStringWrapper(L"ToastGeneric").Get());
                            if (SUCCEEDED(hr))
                                        hr = toastElement->SetAttribute(WinToastStringWrapper(L"duration").Get(), WinToastStringWrapper(L"long").Get());
                            if (SUCCEEDED(hr)) {
                                ComPtr<IXmlElement> actionsElement;
                                hr = xml->CreateElement(WinToastStringWrapper(L"actions").Get(), &actionsElement);
                                if (SUCCEEDED(hr)) {
                                    hr = actionsElement.As(&actionsNode);
                                    if (SUCCEEDED(hr)) {
                                        ComPtr<IXmlNode> appendedChild;
                                        hr = toastNode->AppendChild(actionsNode.Get(), &appendedChild);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (SUCCEEDED(hr)) {
                ComPtr<IXmlElement> actionElement;
                hr = xml->CreateElement(WinToastStringWrapper(L"action").Get(), &actionElement);
                if (SUCCEEDED(hr))
                    hr = actionElement->SetAttribute(WinToastStringWrapper(L"content").Get(), WinToastStringWrapper(content).Get());
                if (SUCCEEDED(hr))
                    hr = actionElement->SetAttribute(WinToastStringWrapper(L"arguments").Get(), WinToastStringWrapper(arguments).Get());
                if (SUCCEEDED(hr)) {
                    ComPtr<IXmlNode> actionNode;
                    hr = actionElement.As(&actionNode);
                    if (SUCCEEDED(hr)) {
                        ComPtr<IXmlNode> appendedChild;
                        hr = actionsNode->AppendChild(actionNode.Get(), &appendedChild);
                    }
                }
            }
        }
    }
    return hr;
}

void WinToast::setError(_Out_ WinToastError* error, _In_ WinToastError value) {
    if (error) {
        *error = value;
    } 
}

WinToastTemplate::WinToastTemplate(_In_ WinToastTemplateType type) : _type(type) {
    static constexpr std::size_t TextFieldsCount[] = { 1, 2, 2, 3, 1, 2, 2, 3};
    _textFields = std::vector<std::wstring>(TextFieldsCount[type], L"");
}

WinToastTemplate::~WinToastTemplate() {
    _textFields.clear();
}

void WinToastTemplate::setTextField(_In_ const std::wstring& txt, _In_ WinToastTemplate::TextField pos) {
    const auto position = static_cast<std::size_t>(pos);
    assert(position < _textFields.size());
    _textFields[position] = txt;
}

void WinToastTemplate::setImagePath(_In_ const std::wstring& imgPath) {
    _imagePath = imgPath;
}

void WinToastTemplate::setAudioPath(_In_ const std::wstring& audioPath) {
    _audioPath = audioPath;
}

void WinToastTemplate::setAudioPath(_In_ AudioSystemFile file) {
    static const std::unordered_map<AudioSystemFile, std::wstring> Files = {
        {AudioSystemFile::DefaultSound, L"ms-winsoundevent:Notification.Default"},
        {AudioSystemFile::IM, L"ms-winsoundevent:Notification.IM"},
        {AudioSystemFile::Mail, L"ms-winsoundevent:Notification.Mail"},
        {AudioSystemFile::Reminder, L"ms-winsoundevent:Notification.Reminder"},
        {AudioSystemFile::SMS, L"ms-winsoundevent:Notification.SMS"},
        {AudioSystemFile::Alarm, L"ms-winsoundevent:Notification.Looping.Alarm"},
        {AudioSystemFile::Alarm2, L"ms-winsoundevent:Notification.Looping.Alarm2"},
        {AudioSystemFile::Alarm3, L"ms-winsoundevent:Notification.Looping.Alarm3"},
        {AudioSystemFile::Alarm4, L"ms-winsoundevent:Notification.Looping.Alarm4"},
        {AudioSystemFile::Alarm5, L"ms-winsoundevent:Notification.Looping.Alarm5"},
        {AudioSystemFile::Alarm6, L"ms-winsoundevent:Notification.Looping.Alarm6"},
        {AudioSystemFile::Alarm7, L"ms-winsoundevent:Notification.Looping.Alarm7"},
        {AudioSystemFile::Alarm8, L"ms-winsoundevent:Notification.Looping.Alarm8"},
        {AudioSystemFile::Alarm9, L"ms-winsoundevent:Notification.Looping.Alarm9"},
        {AudioSystemFile::Alarm10, L"ms-winsoundevent:Notification.Looping.Alarm10"},
        {AudioSystemFile::Call, L"ms-winsoundevent:Notification.Looping.Call"},
        {AudioSystemFile::Call1, L"ms-winsoundevent:Notification.Looping.Call1"},
        {AudioSystemFile::Call2, L"ms-winsoundevent:Notification.Looping.Call2"},
        {AudioSystemFile::Call3, L"ms-winsoundevent:Notification.Looping.Call3"},
        {AudioSystemFile::Call4, L"ms-winsoundevent:Notification.Looping.Call4"},
        {AudioSystemFile::Call5, L"ms-winsoundevent:Notification.Looping.Call5"},
        {AudioSystemFile::Call6, L"ms-winsoundevent:Notification.Looping.Call6"},
        {AudioSystemFile::Call7, L"ms-winsoundevent:Notification.Looping.Call7"},
        {AudioSystemFile::Call8, L"ms-winsoundevent:Notification.Looping.Call8"},
        {AudioSystemFile::Call9, L"ms-winsoundevent:Notification.Looping.Call9"},
        {AudioSystemFile::Call10, L"ms-winsoundevent:Notification.Looping.Call10"},
    };
    const auto iter = Files.find(file);
    assert(iter != Files.end());
    _audioPath = iter->second;
}

void WinToastTemplate::setAudioOption(_In_ WinToastTemplate::AudioOption audioOption) {
    _audioOption = audioOption;
}

void WinToastTemplate::setFirstLine(const std::wstring &text) {
    setTextField(text, WinToastTemplate::FirstLine);
}

void WinToastTemplate::setSecondLine(const std::wstring &text) {
    setTextField(text, WinToastTemplate::SecondLine);
}

void WinToastTemplate::setThirdLine(const std::wstring &text) {
    setTextField(text, WinToastTemplate::ThirdLine);
}

void WinToastTemplate::setDuration(_In_ Duration duration) {
    _duration = duration;
}

void WinToastTemplate::setExpiration(_In_ INT64 millisecondsFromNow) {
    _expiration = millisecondsFromNow;
}

void WinToastTemplate::setAttributionText(_In_ const std::wstring& attributionText) {
    _attributionText = attributionText;
}

void WinToastTemplate::addAction(_In_ const std::wstring & label) {
	_actions.push_back(label);
}

std::size_t WinToastTemplate::textFieldsCount() const {
    return _textFields.size();
}

std::size_t WinToastTemplate::actionsCount() const {
    return _actions.size();
}

bool WinToastTemplate::hasImage() const {
    return _type <  WinToastTemplateType::Text01;
}

const std::vector<std::wstring>& WinToastTemplate::textFields() const {
    return _textFields;
}

const std::wstring& WinToastTemplate::textField(_In_ TextField pos) const {
    const auto position = static_cast<std::size_t>(pos);
    assert(position < _textFields.size());
    return _textFields[position];
}

const std::wstring& WinToastTemplate::actionLabel(_In_ std::size_t position) const {
    assert(position < _actions.size());
    return _actions[position];
}

const std::wstring& WinToastTemplate::imagePath() const {
    return _imagePath;
}

const std::wstring& WinToastTemplate::audioPath() const {
    return _audioPath;
}

const std::wstring& WinToastTemplate::attributionText() const {
    return _attributionText;
}

INT64 WinToastTemplate::expiration() const {
    return _expiration;
}

WinToastTemplate::WinToastTemplateType WinToastTemplate::type() const {
    return _type;
}

WinToastTemplate::AudioOption WinToastTemplate::audioOption() const {
    return _audioOption;
}

WinToastTemplate::Duration WinToastTemplate::duration() const {
    return _duration;
}
