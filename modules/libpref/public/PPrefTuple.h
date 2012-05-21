/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_pref_hash_entry_array_IPC_serialiser
#define dom_pref_hash_entry_array_IPC_serialiser

#include "IPC/IPCMessageUtils.h"
#include "nsTArray.h"
#include "pldhash.h"
#include "nsIPrefService.h"
#include "PrefTuple.h"


namespace IPC {

template <>
struct ParamTraits<PrefTuple>
{
  typedef PrefTuple paramType;

  // Function to serialize a PrefTuple
  static void Write(Message *aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.key);
    WriteParam(aMsg, (PRUint32)aParam.type);
    switch (aParam.type) {
      case PrefTuple::PREF_STRING:
        WriteParam(aMsg, aParam.stringVal);
        break;
      case PrefTuple::PREF_INT:
        WriteParam(aMsg, aParam.intVal);
        break;
      case PrefTuple::PREF_BOOL:
        WriteParam(aMsg, aParam.boolVal);
        break;
    }
  }

  // Function to de-serialize a PrefTuple
  static bool Read(const Message* aMsg, void **aIter, paramType* aResult)
  {
    PRUint32 type;

    if (!ReadParam(aMsg, aIter, &(aResult->key)))
      return false;

    if (!ReadParam(aMsg, aIter, &type))
      return false;

    switch (type) {
      case PrefTuple::PREF_STRING:
        aResult->type = PrefTuple::PREF_STRING;
        if (!ReadParam(aMsg, aIter, &(aResult->stringVal)))
          return false;
        break;
      case PrefTuple::PREF_INT:
        aResult->type = PrefTuple::PREF_INT;
        if (!ReadParam(aMsg, aIter, &(aResult->intVal)))
          return false;
        break;
      case PrefTuple::PREF_BOOL:
        aResult->type = PrefTuple::PREF_BOOL;
        if (!ReadParam(aMsg, aIter, &(aResult->boolVal)))
          return false;
        break;
    }
    return true;
  }
} ;

}
#endif

