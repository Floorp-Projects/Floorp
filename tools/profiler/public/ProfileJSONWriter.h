/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PROFILEJSONWRITER_H
#define PROFILEJSONWRITER_H

#include "mozilla/BaseProfileJSONWriter.h"

using ChunkedJSONWriteFunc = mozilla::baseprofiler::ChunkedJSONWriteFunc;
using JSONSchemaWriter = mozilla::baseprofiler::JSONSchemaWriter;
using OStreamJSONWriteFunc = mozilla::baseprofiler::OStreamJSONWriteFunc;
using SpliceableChunkedJSONWriter =
    mozilla::baseprofiler::SpliceableChunkedJSONWriter;
using SpliceableJSONWriter = mozilla::baseprofiler::SpliceableJSONWriter;
using UniqueJSONStrings = mozilla::baseprofiler::UniqueJSONStrings;

#endif  // PROFILEJSONWRITER_H
