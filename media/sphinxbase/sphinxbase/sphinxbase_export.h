#ifndef __SPHINXBASE_EXPORT_H__
#define __SPHINXBASE_EXPORT_H__

/* Win32/WinCE DLL gunk */
#if (defined(_WIN32) || defined(_WIN32_WCE)) && !defined(_WIN32_WP) && !defined(__MINGW32__) && !defined(__CYGWIN__) && !defined(__WINSCW__) && !defined(__SYMBIAN32__)
#if defined(SPHINXBASE_EXPORTS) /* Visual Studio */
#define SPHINXBASE_EXPORT __declspec(dllexport)
#else
#define SPHINXBASE_EXPORT __declspec(dllimport)
#endif
#else /* !_WIN32 */
#define SPHINXBASE_EXPORT
#endif

#endif /* __SPHINXBASE_EXPORT_H__ */
