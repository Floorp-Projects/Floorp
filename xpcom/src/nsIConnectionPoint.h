/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef __nsIConnectionPoint_h
#define __nsIConnectionPoint_h

typedef struct _nsConnection {
  nsISupports *sink;
  void *cookie;
} nsConnection;

class nsIConnectionPoint;
class nsIConnectionPointContainer;
class nsIEnumConnections;
class nsIEnumConnectionPoints;

/**
 * nsIConnectionPoint
 */

// {7BB53331-A405-11d1-A963-00805F8A7AC4}
#define NS_ICONNECTION_POINT_IID \
{ 0x7bb53331, 0xa405, 0x11d1, \
  { 0xa9, 0x63, 0x0, 0x80, 0x5f, 0x8a, 0x7a, 0xc4 } }

class nsIConnectionPoint: public nsISupports {
public:
  NS_IMETHOD GetConnectionInterface(nsIID *aIID) = 0;
  NS_IMETHOD GetConnectionPointContainer(nsIConnectionPointContainer 
						**aContainer) = 0;
  NS_IMETHOD Advise(nsISupports *aSink, void *aCookie) = 0;
  NS_IMETHOD Unadvise(void *aCookie) = 0;
  NS_IMETHOD EnumConnections(nsIEnumConnections **aEnum) = 0;
};

/**
 *  nsIConnectionPointContainer
 */

// {7BB53332-A405-11d1-A963-00805F8A7AC4}
#define NS_ICONNECTION_POINT_CONTAINER_IID \
{ 0x7bb53332, 0xa405, 0x11d1, \
  { 0xa9, 0x63, 0x0, 0x80, 0x5f, 0x8a, 0x7a, 0xc4 } }

class nsIConnectionPointContainer: public nsISupports {
public:
  NS_IMETHOD EnumConnectionPoints(nsIEnumConnectionPoints **aEnum) = 0;
  NS_IMETHOD FindConnectionPoint(nsIID *aIID,
				 nsIConnectionPoint *aPoint) = 0;
};

/**
 * nsIEnumConnections
 */

// {7BB53333-A405-11d1-A963-00805F8A7AC4}
#define NS_IENUM_CONNECTIONS_IID \
{ 0x7bb53333, 0xa405, 0x11d1, \
  { 0xa9, 0x63, 0x0, 0x80, 0x5f, 0x8a, 0x7a, 0xc4 } }

class nsIEnumConnections: public nsISupports {
  NS_IMETHOD Next(nsConnection *aConnection) = 0;
  NS_IMETHOD Reset() = 0;
};

/**
 * nsIEnumConnectionPoints
 */

// {7BB53333-A405-11d1-A963-00805F8A7AC4}
#define NS_IENUM_CONNECTION_POINTS_IID \
{ 0x7bb53333, 0xa405, 0x11d1, \
  { 0xa9, 0x63, 0x0, 0x80, 0x5f, 0x8a, 0x7a, 0xc4 } }

class nsIEnumConnectionPoints: public nsISupports {
  NS_IMETHOD Next(nsIConnectionPoint *aConnectionPoint) = 0;
  NS_IMETHOD Reset() = 0;
};

#endif

