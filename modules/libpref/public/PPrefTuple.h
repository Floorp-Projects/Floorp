/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla preference service code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mike Kristoffersen <moz@mikek.dk> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

