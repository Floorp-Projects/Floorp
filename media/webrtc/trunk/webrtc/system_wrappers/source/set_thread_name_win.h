/*
 *  Use of this source code is governed by the MICROSOFT LIMITED PUBLIC LICENSE
 *  copyright license which can be found in the LICENSE file in the
 *  third_party_mods/mslpl directory of the source tree or at
 *  http://msdn.microsoft.com/en-us/cc300389.aspx#P.
 */
/*
 *  The original code can be found here:
 *  http://msdn.microsoft.com/en-us/library/xcb2z8hs(VS.71).aspx
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_THREAD_WINDOWS_SET_NAME_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_THREAD_WINDOWS_SET_NAME_H_

namespace webrtc {

struct THREADNAME_INFO
{
   DWORD dwType;     // must be 0x1000
   LPCSTR szName;    // pointer to name (in user addr space)
   DWORD dwThreadID; // thread ID (-1 = caller thread)
   DWORD dwFlags;    // reserved for future use, must be zero
};

void SetThreadName(DWORD dwThreadID, LPCSTR szThreadName)
{
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = szThreadName;
    info.dwThreadID = dwThreadID;
    info.dwFlags = 0;

    __try
    {
        RaiseException(0x406D1388, 0, sizeof(info) / sizeof(DWORD),
                       (ULONG_PTR*)&info);
    }
    __except (EXCEPTION_CONTINUE_EXECUTION)
    {
    }
}
} // namespace webrtc
#endif // WEBRTC_SYSTEM_WRAPPERS_SOURCE_THREAD_WINDOWS_SET_NAME_H_
