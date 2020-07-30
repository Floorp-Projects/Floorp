/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCommandLine.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "WinRemoteMessage.h"

using namespace mozilla;

WinRemoteMessageSender::WinRemoteMessageSender(const char* aCommandLine)
    : mData({static_cast<DWORD>(WinRemoteMessageVersion::CommandLineOnly)}),
      mUtf8Buffer(aCommandLine) {
  mUtf8Buffer.Append('\0');

  char* mutableBuffer;
  mData.cbData = mUtf8Buffer.GetMutableData(&mutableBuffer);
  mData.lpData = mutableBuffer;
}

WinRemoteMessageSender::WinRemoteMessageSender(const char* aCommandLine,
                                               const char* aWorkingDir)
    : mData({static_cast<DWORD>(
          WinRemoteMessageVersion::CommandLineAndWorkingDir)}),
      mUtf8Buffer(aCommandLine) {
  mUtf8Buffer.Append('\0');
  mUtf8Buffer.Append(aWorkingDir);
  mUtf8Buffer.Append('\0');

  char* mutableBuffer;
  mData.cbData = mUtf8Buffer.GetMutableData(&mutableBuffer);
  mData.lpData = mutableBuffer;
}

WinRemoteMessageSender::WinRemoteMessageSender(const wchar_t* aCommandLine,
                                               const wchar_t* aWorkingDir)
    : mData({static_cast<DWORD>(
          WinRemoteMessageVersion::CommandLineAndWorkingDirInUtf16)}),
      mUtf16Buffer(aCommandLine) {
  mUtf16Buffer.Append(u'\0');
  mUtf16Buffer.Append(aWorkingDir);
  mUtf16Buffer.Append(u'\0');

  char16_t* mutableBuffer;
  mData.cbData = mUtf16Buffer.GetMutableData(&mutableBuffer) * sizeof(char16_t);
  mData.lpData = mutableBuffer;
}

COPYDATASTRUCT* WinRemoteMessageSender::CopyData() { return &mData; }

nsresult WinRemoteMessageReceiver::ParseV0(char* aBuffer) {
  CommandLineParserWin<char> parser;
  parser.HandleCommandLine(aBuffer);

  mCommandLine = new nsCommandLine();
  return mCommandLine->Init(parser.Argc(), parser.Argv(), nullptr,
                            nsICommandLine::STATE_REMOTE_AUTO);
}

nsresult WinRemoteMessageReceiver::ParseV1(char* aBuffer) {
  CommandLineParserWin<char> parser;
  parser.HandleCommandLine(aBuffer);

  // Moving |wdpath| to the working dir followed by the first null char.
  char* wdpath = aBuffer;
  while (*wdpath) {
    ++wdpath;
  }
  ++wdpath;

  nsCOMPtr<nsIFile> workingDir;
  NS_NewLocalFile(NS_ConvertUTF8toUTF16(wdpath), false,
                  getter_AddRefs(workingDir));

  mCommandLine = new nsCommandLine();
  return mCommandLine->Init(parser.Argc(), parser.Argv(), workingDir,
                            nsICommandLine::STATE_REMOTE_AUTO);
}

nsresult WinRemoteMessageReceiver::ParseV2(char16_t* aBuffer) {
  CommandLineParserWin<char16_t> parser;
  parser.HandleCommandLine(aBuffer);

  // Moving |wdpath| to the working dir followed by the first null char.
  char16_t* wdpath = aBuffer;
  while (*wdpath) {
    ++wdpath;
  }
  ++wdpath;

  nsCOMPtr<nsIFile> workingDir;
  NS_NewLocalFile(nsDependentString(wdpath), false, getter_AddRefs(workingDir));

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

nsresult WinRemoteMessageReceiver::Parse(COPYDATASTRUCT* aMessageData) {
  switch (static_cast<WinRemoteMessageVersion>(aMessageData->dwData)) {
    case WinRemoteMessageVersion::CommandLineOnly:
      return ParseV0(reinterpret_cast<char*>(aMessageData->lpData));
    case WinRemoteMessageVersion::CommandLineAndWorkingDir:
      return ParseV1(reinterpret_cast<char*>(aMessageData->lpData));
    case WinRemoteMessageVersion::CommandLineAndWorkingDirInUtf16:
      return ParseV2(reinterpret_cast<char16_t*>(aMessageData->lpData));
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported message version");
      return NS_ERROR_FAILURE;
  }
}

nsICommandLineRunner* WinRemoteMessageReceiver::CommandLineRunner() {
  return mCommandLine;
}
