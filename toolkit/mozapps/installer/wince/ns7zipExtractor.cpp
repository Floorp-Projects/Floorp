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

#include "wtypes.h"
#include "nsArchiveExtractor.h"
#include "ns7zipExtractor.h"
#include "7zLib.h"

static nsExtractorProgress *g_pProgress = NULL;

static void ExtractProgressCallback(int nPercentComplete)
{
  if (g_pProgress)
  {
    g_pProgress->Progress(nPercentComplete);
  }
}

static void ShowError()
{
  MessageBoxW(GetForegroundWindow(), GetExtractorError(), L"Extractor", MB_OK|MB_ICONERROR);
}

ns7zipExtractor::ns7zipExtractor(const WCHAR *sArchiveName, DWORD dwSfxStubSize, nsExtractorProgress *pProgress) :
  nsArchiveExtractor(sArchiveName, dwSfxStubSize, pProgress)
{
  g_pProgress = pProgress;
}

ns7zipExtractor::~ns7zipExtractor()
{
  g_pProgress = NULL;
}

int ns7zipExtractor::Extract(const WCHAR *sDestinationDir)
{
  int res = SzExtractSfx(m_sArchiveName, m_dwSfxStubSize, NULL, sDestinationDir, ExtractProgressCallback);
  if (res != 0)
    ShowError();
  return res;
}

int ns7zipExtractor::ExtractFile(const WCHAR *sFileName, const WCHAR *sDestinationDir)
{
  int res = SzExtractSfx(m_sArchiveName, m_dwSfxStubSize, sFileName, sDestinationDir, NULL);
  if (res != 0)
    ShowError();
  return res;
}
