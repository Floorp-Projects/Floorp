/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#ifdef notdef
#include "xp_core.h"
#include "xp_file.h"
#endif
#include "secrng.h"
#include "mcom_db.h"
#ifdef XP_MAC
#include <Events.h>
#include <OSUtils.h>
#include <QDOffscreen.h>
#include <PPCToolbox.h>
#include <Processes.h>
#include <LowMem.h>

/* Static prototypes */
static size_t CopyLowBits(void *dst, size_t dstlen, void *src, size_t srclen);
void FE_ReadScreen();

static size_t CopyLowBits(void *dst, size_t dstlen, void *src, size_t srclen)
{
    union endianness {
        int32 i;
        char c[4];
    } u;

    if (srclen <= dstlen) {
        memcpy(dst, src, srclen);
        return srclen;
    }
    u.i = 0x01020304;
    if (u.c[0] == 0x01) {
        /* big-endian case */
        memcpy(dst, (char*)src + (srclen - dstlen), dstlen);
    } else {
        /* little-endian case */
        memcpy(dst, src, dstlen);
    }
    return dstlen;
}

size_t RNG_GetNoise(void *buf, size_t maxbytes)
{
    uint32 c = TickCount();
    return CopyLowBits(buf, maxbytes,  &c, sizeof(c));
}

void RNG_FileForRNG(char *filename)
{   
    unsigned char buffer[BUFSIZ];
    size_t bytes;
#ifdef notdef /*sigh*/
    XP_File file;
	unsigned long totalFileBytes = 0;

	if (filename == NULL)	/* For now, read in global history if filename is null */
		file = XP_FileOpen(NULL, xpGlobalHistory,XP_FILE_READ_BIN);
	else
		file = XP_FileOpen(NULL, xpURL,XP_FILE_READ_BIN);
    if (file != NULL) {
        for (;;) {
            bytes = XP_FileRead(buffer, sizeof(buffer), file);
            if (bytes == 0) break;
            RNG_RandomUpdate( buffer, bytes);
            totalFileBytes += bytes;
            if (totalFileBytes > 100*1024) break;	/* No more than 100 K */
        }
		XP_FileClose(file);
    }
#endif
    /*
     * Pass yet another snapshot of our highest resolution clock into
     * the hash function.
     */
    bytes = RNG_GetNoise(buffer, sizeof(buffer));
    RNG_RandomUpdate(buffer, sizeof(buffer));
}

void RNG_SystemInfoForRNG()
{
/* Time */
	{
		unsigned long sec;
		size_t bytes;
		GetDateTime(&sec);	/* Current time since 1970 */
		RNG_RandomUpdate( &sec, sizeof(sec));
	    bytes = RNG_GetNoise(&sec, sizeof(sec));
	    RNG_RandomUpdate(&sec, bytes);
    }
/* User specific variables */
	{
		MachineLocation loc;
		ReadLocation(&loc);
		RNG_RandomUpdate( &loc, sizeof(loc));
	}
/* User name */
	{
		unsigned long userRef;
		Str32 userName;
		GetDefaultUser(&userRef, userName);
		RNG_RandomUpdate( &userRef, sizeof(userRef));
		RNG_RandomUpdate( userName, sizeof(userName));
	}
/* Mouse location */
	{
		Point mouseLoc;
		GetMouse(&mouseLoc);
		RNG_RandomUpdate( &mouseLoc, sizeof(mouseLoc));
	}
/* Keyboard time threshold */
	{
		SInt16 keyTresh = LMGetKeyThresh();
		RNG_RandomUpdate( &keyTresh, sizeof(keyTresh));
	}
/* Last key pressed */
	{
		SInt8 keyLast;
		keyLast = LMGetKbdLast();
		RNG_RandomUpdate( &keyLast, sizeof(keyLast));
	}
/* Volume */
	{
		UInt8 volume = LMGetSdVolume();
		RNG_RandomUpdate( &volume, sizeof(volume));
	}
/* Current directory */
	{
		SInt32 dir = LMGetCurDirStore();
		RNG_RandomUpdate( &dir, sizeof(dir));
	}
/* Process information about all the processes in the machine */
	{
		ProcessSerialNumber 	process;
		ProcessInfoRec pi;
	
		process.highLongOfPSN = process.lowLongOfPSN  = kNoProcess;
		
		while (GetNextProcess(&process) == noErr)
		{
			FSSpec fileSpec;
			pi.processInfoLength = sizeof(ProcessInfoRec);
			pi.processName = NULL;
			pi.processAppSpec = &fileSpec;
			GetProcessInformation(&process, &pi);
			RNG_RandomUpdate( &pi, sizeof(pi));
			RNG_RandomUpdate( &fileSpec, sizeof(fileSpec));
		}
	}
	
/* Heap */
	{
		THz zone = LMGetTheZone();
		RNG_RandomUpdate( &zone, sizeof(zone));
	}
	
/* Screen */
	{
		GDHandle h = LMGetMainDevice();		/* GDHandle is **GDevice */
		RNG_RandomUpdate( *h, sizeof(GDevice));
	}
/* Scrap size */
	{
		SInt32 scrapSize = LMGetScrapSize();
		RNG_RandomUpdate( &scrapSize, sizeof(scrapSize));
	}
/* Scrap count */
	{
		SInt16 scrapCount = LMGetScrapCount();
		RNG_RandomUpdate( &scrapCount, sizeof(scrapCount));
	}
/*  File stuff, last modified, etc. */
	{
		HParamBlockRec			pb;
		GetVolParmsInfoBuffer	volInfo;
		pb.ioParam.ioVRefNum = 0;
		pb.ioParam.ioNamePtr = nil;
		pb.ioParam.ioBuffer = (Ptr) &volInfo;
		pb.ioParam.ioReqCount = sizeof(volInfo);
		PBHGetVolParmsSync(&pb);
		RNG_RandomUpdate( &volInfo, sizeof(volInfo));
	}
/* Event queue */
	{
		EvQElPtr		eventQ;
		for (eventQ = (EvQElPtr) LMGetEventQueue()->qHead; 
				eventQ; 
				eventQ = (EvQElPtr)eventQ->qLink)
			RNG_RandomUpdate( &eventQ->evtQWhat, sizeof(EventRecord));
	}
	FE_ReadScreen();
	RNG_FileForRNG(NULL);
}

void FE_ReadScreen()
{
	UInt16				coords[4];
	PixMapHandle 		pmap;
	GDHandle			gh;	
	UInt16				screenHeight;
	UInt16				screenWidth;			/* just what they say */
	UInt32				bytesToRead;			/* number of bytes we're giving */
	UInt32				offset;					/* offset into the graphics buffer */
	UInt16				rowBytes;
	UInt32				rowsToRead;
	float				bytesPerPixel;			/* dependent on buffer depth */
	Ptr					p;						/* temporary */
	UInt16				x, y, w, h;

	gh = LMGetMainDevice();
	if ( !gh )
		return;
	pmap = (**gh).gdPMap;
	if ( !pmap )
		return;
		
	RNG_GenerateGlobalRandomBytes( coords, sizeof( coords ) );
	
	/* make x and y inside the screen rect */	
	screenHeight = (**pmap).bounds.bottom - (**pmap).bounds.top;
	screenWidth = (**pmap).bounds.right - (**pmap).bounds.left;
	x = coords[0] % screenWidth;
	y = coords[1] % screenHeight;
	w = ( coords[2] & 0x7F ) | 0x40;		/* Make sure that w is in the range 64..128 */
	h = ( coords[3] & 0x7F ) | 0x40;		/* same for h */

	bytesPerPixel = (**pmap).pixelSize / 8;
	rowBytes = (**pmap).rowBytes & 0x7FFF;

	/* starting address */
	offset = ( rowBytes * y ) + (UInt32)( (float)x * bytesPerPixel );
	
	/* don't read past the end of the pixmap's rowbytes */
	bytesToRead = MIN(	(UInt32)( w * bytesPerPixel ),
						(UInt32)( rowBytes - ( x * bytesPerPixel ) ) );

	/* don't read past the end of the graphics device pixmap */
	rowsToRead = MIN(	h, 
						( screenHeight - y ) );
	
	p = GetPixBaseAddr( pmap ) + offset;
	
	while ( rowsToRead-- )
	{
		RNG_RandomUpdate( p, bytesToRead );
		p += rowBytes;
	}
}
#endif
