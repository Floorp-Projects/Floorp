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

#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

#include "NSReg.h"
#include "VerReg.h"

extern void interp(void);

#define REGFILE "c:\\temp\\reg.dat"

char *gRegistry;

int main(int argc, char *argv[]);

char *errstr(REGERR err)
{

	switch( err )
	{
	case REGERR_OK:
		return "REGERR_OK";
	case REGERR_FAIL:
		return "REGERR_FAIL";
	case REGERR_NOMORE:
		return "REGERR_MORE";
	case REGERR_NOFIND:
		return "REGERR_NOFIND";
	case REGERR_BADREAD:
		return "REGERR_BADREAD";
	case REGERR_BADLOCN:
		return "REGERR_BADLOCN";
	case REGERR_PARAM:
		return "REGERR_PARAM";
	case REGERR_BADMAGIC:
		return "REGERR_BADMAGIC";
	default:
		return "<Unknown>";
	}

}	// errstr


int CreateEmptyRegistry(void)
{

#if 0
	int fh;
	remove(REGFILE);	// ignore errors like file not found

	fh = _open(REGFILE, _O_CREAT, _S_IREAD|_S_IWRITE);
	if (fh < 0)
		return -1;
	close(fh);
	return 0;
#endif

	return VR_CreateRegistry(CR_NEWREGISTRY, "4.0");

}	// CreateEmptyRegistry
		


int BuildTree(void)
{

	REGERR err;

	err = NR_RegAdd(0,"/Machine/Old");
	if (err != REGERR_OK)
	{
		printf("NR_RegAdd() returned %s.\n", errstr(err));
		return err;
	}
	err = NR_RegAdd(0,"/User");
	if (err != REGERR_OK)
	{
		printf("NR_RegAdd() returned %s.\n", errstr(err));
		return err;
	}
	err = NR_RegAdd(0,"/Machine/4.0/Name1=Val1");
	if (err != REGERR_OK)
	{
		printf("NR_RegAdd() returned %s.\n", errstr(err));
		return err;
	}
	err = NR_RegAdd(0,"/Machine/4.0/Name2=Val2");
	if (err != REGERR_OK)
	{
		printf("NR_RegAdd() returned %s.\n", errstr(err));
		return err;
	}
	err = NR_RegAdd(0,"/Machine/4.0/Name2=Val3");
	if (err != REGERR_OK)
	{
		printf("NR_RegAdd() returned %s.\n", errstr(err));
		return err;
	}
	err = NR_RegAdd(0,"/Machine/4.0/Name3=Val4");
	if (err != REGERR_OK)
	{
		printf("NR_RegAdd() returned %s.\n", errstr(err));
		return err;
	}

	return VR_Checkpoint();

}	// BuildTree


int FindKeys(void)
{

	RKEY key;
	REGERR err;
	char buf[80];

	if (NR_RegGetKey(0, "", &key) == REGERR_OK)
	{
		printf("NR_RegGetKey returns ok for an empty path.\n");
		return 1;
	}

	if (NR_RegGetKey(0, "/", &key) != REGERR_OK)
	{
		printf("NR_RegGetKey couldn't find root.\n");
		return 1;
	}

	if (NR_RegGetKey(0, "/Machine/Old", &key) != REGERR_OK)
	{
		printf("NR_RegGetKey couldn't find Old\n");
		return 1;
	}
	printf("NR_RegGetKey returns key for Old as: 0x%lx\n", (long) key);

	if (NR_RegGetKey(0, "/Machine/4.0", &key) != REGERR_OK)
	{
		printf("NR_RegGetKey couldn't find 4.0\n");
		return 1;
	}
	printf("NR_RegGetKey returns key for 4.0 as: 0x%lx\n", (long) key);

	// ----------------------------------------
	if ((err = NR_RegFindValue(0, "/Machine/4.0/Name3", 64, buf)) != REGERR_OK)
	{
		printf("NR_RegFindValue (no key) returns %s\n", errstr(err));
		return 1;
	}
	printf("NR_RegFindValue (no key) of Name3 = %s\n", buf);

	if (NR_RegFindValue(key, "Aliens", 64, buf) == REGERR_OK)
	{
		printf("NR_RegFindValue finds Aliens.\n");
		return 1;
	}

	if ((err = NR_RegFindValue(key, "Name3", 64, buf)) != REGERR_OK)
	{
		printf("NR_RegFindValue (w/key) returns %s\n", errstr(err));
		return 1;
	}
	printf("NR_RegFindValue (w/key) of Name3 = %s\n", buf);

	return 0;

}	// FindTree

int DumpTree(void)
{

	char *path;
	char *line = "------------------------------------------------------------";

	path = malloc(2048);
	if (!path)
		return REGERR_FAIL;

	strcpy(path, "/");
	puts(line);
	puts(path);

	while (NR_RegNext( 0, 512, path ) == REGERR_OK)
	{
		puts(path);
	}

	puts(line);

	return 0;

}	// DumpTree


int ChangeKeys(void)
{

	REGERR err;

	err = NR_RegUpdate(0,"/Machine/4.0/name3", "Infospect Software, Inc.");
	if (err)
	{
		printf("Couldn't update name3's value to ISI.\n");
		return err;
	}

	err = NR_RegUpdate(0,"/Machine/4.0/name3=Infospect Software, Inc.", "\"Jonathan=Kid1\"");
	if (err)
	{
		printf("Couldn't update name3's value to Jon.\n");
		return err;
	}

	err = NR_RegRename(0,"/Machine/4.0/name3=\"Jonathan=Kid1\"", "First born");
	if (err)
	{
		printf("Couldn't update name3's name to First born.\n");
		return err;
	}

	err = NR_RegUpdate(0,"/Machine/4.0/name2=Val2", "Kelley Ann");
	if (err)
	{
		printf("Couldn't update name2's value to Kelley.\n");
		return err;
	}


	return VR_Checkpoint();

}	// ChangeKeys


int DeleteKeys(void)
{

	REGERR err;

	err = NR_RegDelete(0, "/User");
	if (err)
	{
		printf("NR_RegDelete returned %s.\n", errstr(err));
		return err;
	}

	return VR_Checkpoint();

}	// DeleteKeys


int StressTest(void)
{

	REGERR err;
	RKEY key;

	printf("Starting stress...\n");

	err = NR_RegGetKey(0, "/Machine/4.0", &key);
	if (err)
	{
		printf("Error getting key for 4.0 = %s\n", errstr(err));
		return err;
	}

	err = NR_RegAdd(key, "A/B/C/D/E/F/G/H/I/J/K/L/M/N/O/P/Q/R/S/T/U/V/W/X/Y/Z/"
		"A/B/C/D/E/F/G/H/I/J/K/L/M/N/O/P/Q/R/S/T/U/V/W/X/Y/Z/"
		"A/B/C/D/E/F/G/H/I/J/K/L/M/N/O/P/Q/R/S/T/U/V/W/X/Y/Z/" );

	if (err)
	{
		printf("Adding humungous string returned %s\n", errstr(err));
		return err;
	}

	// TODO: Add a value to one of the middle keys, get it back.

	printf("Stress done.\n");
	return 0;

}	// StressTest


int Install(void)
{

	int err;
	VERSION ver;

	ver.major = 4;
	ver.minor = 2;
	ver.release = 10;
	ver.build = 937;
	ver.check = 0;

	err = VR_Install("Web/Navigator/netscape.exe", 
		"c:\\Netscape\\NETSCAPE.EXE", &ver);
	if (err)
		return err;

	ver.release = 19;
	ver.build = 722;
	ver.check = 0;
	err = VR_Install("Web/Navigator/nspr.dll", 
		"c:\\Netscape\\System\\Vtcprac.386", &ver);

	if (err)
		return err;

	return VR_Checkpoint();

}

int GetInfo(void)
{

	int err; 
	char buf[256];
	VERSION ver;

	err = VR_GetPath("Web/Navigator/nspr.dll", 256, buf);
	if (err)
		return err;

	printf("GetPath(nspr.dll) returns %s\n", buf);

	err = VR_GetVersion("Web/Navigator/netscape.exe", &ver);
	if (err)
		return err;
	printf("GetVersion(netscape.exe) returns %d.%d.%d.%d and check=%d\n",
		ver.major, ver.minor, ver.release, ver.build, ver.check);

	return 0;

}


int main(int argc, char *argv[])
{

	printf("Registry Test 10/01/96.\n");

	if (argc > 1)
	{
		gRegistry = argv[1];
	}
	else
	{
		gRegistry = REGFILE;
	}
	VR_RegistryName(gRegistry);

#if 1
	if (NR_RegOpen(gRegistry) != REGERR_OK)
		VR_CreateRegistry(CR_NEWREGISTRY, "4.0");
	interp();
#else
	if (CreateEmptyRegistry())
		goto abort;

	if (Install())
		goto done;

	if (DumpTree())
		goto done;

	if (GetInfo())
		goto done;


#if defined(TEST_NR)
	if ((err = NR_RegOpen(REGFILE)) != REGERR_OK)
	{
		printf("NR_RegOpen(%s) returned %s...Test aborted.\n", REGFILE, errstr(err));
		goto abort;
	}

	if (BuildTree())
		goto done;

	if (FindKeys())
		goto done;

	if (DumpTree())
		goto done;

	if (ChangeKeys())
		goto done;

	if (DeleteKeys())
		goto done;

	if (DumpTree())
		goto done;

	if (StressTest())
		goto done;

	if (DumpTree())
		goto done;

done:
	NR_RegClose();
#else
done:
#endif

abort:
	puts("Press Enter to continue...");
	getchar();
#endif

	return 0;

}


