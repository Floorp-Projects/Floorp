/* -*- Mode: C++; c-basic-offset: 2; tab-width: 8; indent-tabs-mode: nil; -*- */
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
 * The Original Code is Fennec Installer for WinCE.
 *
 * The Initial Developer of the Original Code is The Mozilla Foundation.
 *
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Pakhotin <alexp@mozilla.com> (original author)
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

/**
 *
 * Class to work with localizable string resources
 *
 */

#include <windows.h>
#include "nsSetupStrings.h"
#include "readstrings.h"
#include "errors.h"

#define STR_KEY(x) #x "\0"
const char *kSetupStrKeys =
#include "nsSetupStrings.inc"
;
#undef STR_KEY

nsSetupStrings::nsSetupStrings()
{
  m_sBuf = NULL;
  memset(m_arrStrings, 0, sizeof(m_arrStrings));
}

nsSetupStrings::~nsSetupStrings()
{
  delete[] m_sBuf;
}

BOOL nsSetupStrings::LoadStrings(WCHAR *sFileName)
{
  if (!sFileName)
    return FALSE;

  BOOL bResult = FALSE;
  char strings[kNumberOfStrings][MAX_TEXT_LEN];
  if (ReadStrings(sFileName, kSetupStrKeys, kNumberOfStrings, strings) == OK)
  {
    int nSize = 0;
    for (int i = 0; i < kNumberOfStrings; i++)
    {
      nSize += strlen(strings[i]) + 1;
    }

    m_sBuf = new WCHAR[nSize];
    if (!m_sBuf)
      return FALSE;

    WCHAR *p = m_sBuf;
    for (int i = 0; i < kNumberOfStrings; i++)
    {
      int len = strlen(strings[i]) + 1;
      MultiByteToWideChar(CP_UTF8, 0, strings[i], len, p, len);
      m_arrStrings[i] = p;
      p += len;
    }

    bResult = TRUE;
  }

  return bResult;
}

const WCHAR* nsSetupStrings::GetString(int nID)
{
  if (nID >= kNumberOfStrings)
    return L"";
  else
    return m_arrStrings[nID];
}
