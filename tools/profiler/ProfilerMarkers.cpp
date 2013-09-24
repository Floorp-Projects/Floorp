/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerMarkers.h"
#include "gfxASurface.h"

ProfilerMarkerImagePayload::ProfilerMarkerImagePayload(gfxASurface *aImg)
  : mImg(aImg)
{}

template<typename Builder>
typename Builder::Object
ProfilerMarkerImagePayload::preparePayloadImp(Builder& b)
{
  typename Builder::RootedObject data(b.context(), b.CreateObject());
  b.DefineProperty(data, "type", "innerHTML");
  // TODO: Finish me
  //b.DefineProperty(data, "innerHTML", "<img src=''/>");
  return data;
}

template JSCustomObjectBuilder::Object
ProfilerMarkerImagePayload::preparePayloadImp<JSCustomObjectBuilder>(JSCustomObjectBuilder& b);
template JSObjectBuilder::Object
ProfilerMarkerImagePayload::preparePayloadImp<JSObjectBuilder>(JSObjectBuilder& b);

