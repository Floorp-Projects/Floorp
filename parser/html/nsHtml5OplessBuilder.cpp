/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5OplessBuilder.h"

#include "nsScriptLoader.h"
#include "mozilla/css/Loader.h"
#include "nsIDocShell.h"
#include "nsIHTMLDocument.h"

nsHtml5OplessBuilder::nsHtml5OplessBuilder()
 : nsHtml5DocumentBuilder(true)
{
}

nsHtml5OplessBuilder::~nsHtml5OplessBuilder()
{
}

void
nsHtml5OplessBuilder::Start()
{
  mFlushState = eInFlush;
  BeginDocUpdate();
}

void
nsHtml5OplessBuilder::Finish()
{
  EndDocUpdate();
  DropParserAndPerfHint();
  mScriptLoader = nullptr;
  mDocument = nullptr;
  mNodeInfoManager = nullptr;
  mCSSLoader = nullptr;
  mDocumentURI = nullptr;
  mDocShell = nullptr;
  mOwnedElements.Clear();
  mFlushState = eNotFlushing;
}

void
nsHtml5OplessBuilder::SetParser(nsParserBase* aParser)
{
  mParser = aParser;
}
