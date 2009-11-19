// ParseProperties.h

#ifndef __PARSEPROPERTIES_H
#define __PARSEPROPERTIES_H

#include "Common/MyString.h"
#include "Common/Types.h"

HRESULT ParsePropValue(const UString &name, const PROPVARIANT &prop, UInt32 &resValue);
HRESULT ParsePropDictionaryValue(const UString &srcStringSpec, UInt32 &dicSize);
HRESULT ParsePropDictionaryValue(const UString &name, const PROPVARIANT &prop, UInt32 &resValue);

bool StringToBool(const UString &s, bool &res);
HRESULT SetBoolProperty(bool &dest, const PROPVARIANT &value);
int ParseStringToUInt32(const UString &srcString, UInt32 &number);
HRESULT ParseMtProp(const UString &name, const PROPVARIANT &prop, UInt32 defaultNumThreads, UInt32 &numThreads);

#endif
