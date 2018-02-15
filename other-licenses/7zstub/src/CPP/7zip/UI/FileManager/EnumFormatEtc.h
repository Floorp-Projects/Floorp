// EnumFormatEtc.h

#ifndef __ENUMFORMATETC_H
#define __ENUMFORMATETC_H

#include <windows.h>

HRESULT CreateEnumFormatEtc(UINT numFormats, const FORMATETC *formats, IEnumFORMATETC **enumFormat);

#endif
