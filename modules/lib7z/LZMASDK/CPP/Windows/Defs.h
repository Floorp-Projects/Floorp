// Windows/Defs.h

#ifndef __WINDOWS_DEFS_H
#define __WINDOWS_DEFS_H

inline bool BOOLToBool(BOOL value)
  { return (value != FALSE); }

#ifdef _WIN32
inline bool LRESULTToBool(LRESULT value)
  { return (value != FALSE); }
#endif

inline BOOL BoolToBOOL(bool value)
  { return (value ? TRUE: FALSE); }

inline VARIANT_BOOL BoolToVARIANT_BOOL(bool value)
  { return (value ? VARIANT_TRUE: VARIANT_FALSE); }

inline bool VARIANT_BOOLToBool(VARIANT_BOOL value)
  { return (value != VARIANT_FALSE); }

#endif
