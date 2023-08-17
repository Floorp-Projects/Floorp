/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCommandLine.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "WinRemoteMessage.h"

using namespace mozilla;

WinRemoteMessageSender::WinRemoteMessageSender(const wchar_t* aCommandLine,
                                               const wchar_t* aWorkingDir)
    : mData({static_cast<DWORD>(
          WinRemoteMessageVersion::CommandLineAndWorkingDirInUtf16)}) {
  mUtf16Buffer.Append(aCommandLine);
  mUtf16Buffer.Append(u'\0');
  mUtf16Buffer.Append(aWorkingDir);
  mUtf16Buffer.Append(u'\0');

  char16_t* mutableBuffer;
  mData.cbData = mUtf16Buffer.GetMutableData(&mutableBuffer) * sizeof(char16_t);
  mData.lpData = mutableBuffer;
}

COPYDATASTRUCT* WinRemoteMessageSender::CopyData() { return &mData; }

nsresult WinRemoteMessageReceiver::ParseV2(const nsAString& aBuffer) {
  CommandLineParserWin<char16_t> parser;
  size_t cch = parser.HandleCommandLine(aBuffer);
  ++cch;  // skip a null char

  nsCOMPtr<nsIFile> workingDir;
  if (cch < aBuffer.Length()) {
    NS_NewLocalFile(Substring(aBuffer, cch), false, getter_AddRefs(workingDir));
  }

  int argc = parser.Argc();
  Vector<nsAutoCString> utf8args;
  if (!utf8args.reserve(argc)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  UniquePtr<const char*[]> argv(new const char*[argc]);
  for (int i = 0; i < argc; ++i) {
    utf8args.infallibleAppend(NS_ConvertUTF16toUTF8(parser.Argv()[i]));
    argv[i] = utf8args[i].get();
  }

  mCommandLine = new nsCommandLine();
  return mCommandLine->Init(argc, argv.get(), workingDir,
                            nsICommandLine::STATE_REMOTE_AUTO);
}

nsresult WinRemoteMessageReceiver::Parse(const COPYDATASTRUCT* aMessageData) {
  switch (static_cast<WinRemoteMessageVersion>(aMessageData->dwData)) {
    case WinRemoteMessageVersion::CommandLineAndWorkingDirInUtf16:
      return ParseV2(nsDependentSubstring(
          reinterpret_cast<char16_t*>(aMessageData->lpData),
          aMessageData->cbData / sizeof(char16_t)));
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported message version");
      return NS_ERROR_FAILURE;
  }
}

nsICommandLineRunner* WinRemoteMessageReceiver::CommandLineRunner() {
  return mCommandLine;
}
