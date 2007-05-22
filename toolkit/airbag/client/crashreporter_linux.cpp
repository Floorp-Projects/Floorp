/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Toolkit Crash Reporter
 *
 * The Initial Developer of the Original Code is
 * Ted Mielczarek <ted.mielczarek@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "crashreporter.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#include <algorithm>
#include <cctype>

using std::string;

bool UIInit()
{
  //XXX: implement me
  return true;
}

void UIShutdown()
{
  //XXX: implement me
}

void UIShowDefaultUI()
{
  //XXX: implement me
}

void UIShowCrashUI(const string& dumpfile,
                   const StringTable& queryParameters,
                   const string& sendURL)
{
  //XXX: implement me
}

void UIError(const string& message)
{
  //XXX: implement me
  printf("Error: %s\n", message.c_str());
}

bool UIGetIniPath(string& path)
{
  path = gArgv[0];
  path.append(".ini");

  return true;
}

/*
 * Settings are stored in ~/.vendor/product, or
 * ~/.product if vendor is empty.
 */
bool UIGetSettingsPath(const string& vendor,
                       const string& product,
                       string& settingsPath)
{
  char* home = getenv("HOME");
  
  if (!home)
    return false;

  settingsPath = home;
  settingsPath += "/.";
  if (!vendor.empty()) {
    string lc_vendor;
    std::transform(vendor.begin(), vendor.end(), back_inserter(lc_vendor),
		   (int(*)(int)) std::tolower);
    settingsPath += lc_vendor + "/";
  }
  string lc_product;
  std::transform(product.begin(), product.end(), back_inserter(lc_product),
		 (int(*)(int)) std::tolower);
  settingsPath += lc_product + "/Crash Reports";
  printf("settingsPath: %s\n", settingsPath.c_str());
  return UIEnsurePathExists(settingsPath);
}

bool UIEnsurePathExists(const string& path)
{
  int ret = mkdir(path.c_str(), S_IRWXU);
  int e = errno;
  if (ret == -1 && e != EEXIST)
    return false;

  return true;
}

bool UIMoveFile(const string& file, const string& newfile)
{
  return (rename(file.c_str(), newfile.c_str()) != -1);
}

bool UIDeleteFile(const string& file)
{
  return (unlink(file.c_str()) != -1);
}
