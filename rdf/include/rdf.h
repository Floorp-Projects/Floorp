/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#ifndef rdf_h___
#define rdf_h___

/*
 * The following macros are to aid in vocabulary definition.
 * They creates const char*'s for "kURI[prefix]_[name]" and
 * "kTag[prefix]_[name]", with appropriate complete namespace
 * qualification on the URI, e.g.,
 *
 * #define RDF_NAMESPACE_URI "http://www.w3.org/TR/WD-rdf-syntax#"
 * DEFINE_RDF_ELEMENT(RDF_NAMESPACE_URI, RDF, ID);
 *
 * will define:
 *
 * kURIRDF_ID to be "http://www.w3.org/TR/WD-rdf-syntax#ID", and
 * kTagRDF_ID to be "ID"
 */

#define DEFINE_RDF_VOCAB(namespace, prefix, name) \
static const char* kURI##prefix##_##name = ##namespace #name ;\
static const char* kTag##prefix##_##name = kURI##prefix##_##name## + sizeof(##namespace) - 1

#endif /* rdf_h___ */
