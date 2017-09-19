/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 * 
 */



#ifndef _NSELEMENTABLE
#define _NSELEMENTABLE

#include "nsHTMLTags.h"
#include "nsIDTD.h"

#ifdef DEBUG
extern void CheckElementTable();
#endif

struct nsHTMLElement
{
  static  bool    IsContainer(nsHTMLTag aTag);
  static  bool    IsBlock(nsHTMLTag aTag);
};

#endif
