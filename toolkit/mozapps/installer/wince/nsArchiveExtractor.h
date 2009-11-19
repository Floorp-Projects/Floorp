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

#pragma once

/**
 *  Class to show the progress
 */
class nsExtractorProgress
{
public:
  nsExtractorProgress() {}
  virtual void Progress(int nPercentComplete) = 0; /* to be overriden to get progress notifications */
};

/**
 *  Base archive-extractor class.
 *  Used to derive a specific archive format implementation.
 */
class nsArchiveExtractor
{
public:
  enum
  {
    OK = 0,
    ERR_FAIL = -1,
    ERR_PARAM = -2,
    ERR_READ = -3,
    ERR_WRITE = -4,
    ERR_MEM = -5,
    ERR_NO_ARCHIVE = -6,
    ERR_EXTRACTION = -7,
  };

  nsArchiveExtractor(const WCHAR *sArchiveName, DWORD dwSfxStubSize, nsExtractorProgress *pProgress);
  virtual ~nsArchiveExtractor();

  virtual int Extract(const WCHAR *sDestinationDir);
  virtual int ExtractFile(const WCHAR *sFileName, const WCHAR *sDestinationDir);

protected:
  WCHAR *m_sArchiveName;
  nsExtractorProgress *m_pProgress;
  DWORD m_dwSfxStubSize;
};
