/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5OplessBuilder_h
#define nsHtml5OplessBuilder_h

#include "nsHtml5DocumentBuilder.h"

class nsParserBase;

class nsHtml5OplessBuilder : public nsHtml5DocumentBuilder
{
public:
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  nsHtml5OplessBuilder();
  ~nsHtml5OplessBuilder();
  void Start();
  void Finish();
  void SetParser(nsParserBase* aParser);
};

#endif // nsHtml5OplessBuilder_h
