#ifndef __POCKETSPHINX_EXPORT_H__
#define __POCKETSPHINX_EXPORT_H__

/* Win32/WinCE DLL gunk */
#if (defined(_WIN32) || defined(_WIN32_WCE)) && !defined(_WIN32_WP) && !defined(__MINGW32__) && !defined(__CYGWIN__) && !defined(__WINSCW__) && !defined(__SYMBIAN32__)
#ifdef POCKETSPHINX_EXPORTS /* Visual Studio */
#define POCKETSPHINX_EXPORT __declspec(dllexport)
#else
#define POCKETSPHINX_EXPORT __declspec(dllimport)
#endif
#else /* !_WIN32 */
#define POCKETSPHINX_EXPORT
#endif

#endif /* __POCKETSPHINX_EXPORT_H__ */
