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
// Registry interpreter

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "VerReg.h"
#include "NSReg.h"

extern char *errstr(REGERR err);
extern int DumpTree(void);

int gVerbose = 1;
int gPretend = 0;

int error(char *func, int err)
{
	if (err == REGERR_OK)
	{
		if (gVerbose)
			printf("%s Ok\n", func);
	}
	else
	{
		printf("%s returns %s\n", func, errstr(err));
	}

	return err;

}	// error

static char  *GetNextWord(char *cmd, char *buf)
{
	// copies until ',' or eos, then skips spaces
	if (!cmd || !buf)
		return 0;
	while (*cmd && *cmd != ',')
		*buf++ = *cmd++;
	*buf = '\0';
	if (*cmd == ',')
	{
		cmd++;
		while(*cmd && *cmd == ' ')
			cmd++;
	}
	return cmd;

}	// GetNextWord

static int vr_ParseVersion(char *verstr, VERSION *result)
{

	result->major = result->minor = result->release = result->build = 0;
	result->major = atoi(verstr);
	while (*verstr && *verstr != '.')
		verstr++;
	if (*verstr)
	{
		verstr++;
		result->minor = atoi(verstr);
		while (*verstr && *verstr != '.')
			verstr++;
		if (*verstr)
		{
			verstr++;
			result->release = atoi(verstr);
			while (*verstr && *verstr != '.')
				verstr++;
			if (*verstr)
			{
				verstr++;
				result->build = atoi(verstr);
				while (*verstr && *verstr != '.')
					verstr++;
			}
		}
	}

	return REGERR_OK;

}	// ParseVersion

void parse(char *cmd, char *name, VERSION *ver, char *path)
{
	// expects 'cmd' points to: "<name>, <ver>, <path>"
	char buf[256];
	char *p;

	cmd = GetNextWord(cmd, buf);
	strcpy(name, buf);

	p = cmd;					// 'cmd' points to version
	vr_ParseVersion(cmd, ver);
	ver->check = gPretend ? 0xad : 0;
	while (*p && *p != ',')		// skip to next ','
		p++;
	if (*p == ',')				// skip comma
		p++;
	while (*p && *p == ' ')		// skip white space
		p++;

	strcpy(path, p);

}	// parse

void vVerbose(char *cmd)
{

	if (stricmp(cmd, "ON") == 0)
	{
		gVerbose = 1;
		printf("Verbose mode is now ON.\n");
	}
	else
	{
		gVerbose = 0;
	}

}	// vVerbose

void vCreate(char *cmd)
{

	// Syntax: Create [new,] 5.0b1
	char buf[64];

	int flag = 0;
	cmd = GetNextWord(cmd, buf);
	if (stricmp(buf, "NEW,") == 0)
	{
		flag = CR_NEWREGISTRY;
	}
	error("VR_CreateRegistry", VR_CreateRegistry(flag, cmd));

}	// vCreate

void vDisplay(char *cmd)
{

	DumpTree();

}	// vDisplay


void vFind(char *cmd)
{

	VERSION ver;
	char path[MAXREGPATHLEN];

	if (error("VR_GetVersion", VR_GetVersion(cmd, &ver)) == REGERR_OK)
	{
		if (error("VR_GetPath", VR_GetPath(cmd, sizeof(path), path)) == REGERR_OK)
		{
			printf("%s found: ver=%d.%d.%d.%d, check=0x%04x, path=%s\n", 
				cmd, ver.major, ver.minor, ver.release, ver.build, ver.check,
				path);
			return;
		}
	}

	printf("%s not found.\n", cmd);
	return;

}	// vFind


void vHelp(char *cmd)
{

	puts("Enter a command:");
	puts("\tD)isplay         - display the current contents of the Registry");
	puts("\tI)nstall <name>, <version>, <path> - install a new component");
	puts("\tU)pdate <name>, <version>, <path>  - update a component");
	puts("\tF)ind <name>     - returns version and path");
	puts("\tV)erify <name>   - verify component exists and checks out");
	puts("\tC)reate <name>   - create a new instance of Navigator (e.g., \"4.0\")");
	puts("\tR)emove <name>   - deletes a component from the Registry");
	puts("\tT)est            - perform a simple test on the Registry");
	puts("\tver(B)ose ON|off - turn verbose mode on or off");
	puts("\tP)retend on|OFF  - pretend that test files exist");
	puts("\tS)ave            - save the Registry to disk");
	puts("\tpac(K) registry    - squeeze out unused space from the Registry");
	puts("\tQ)uit            - end the program");

}	// vHelp


void vInstall(char *cmd)
{

	char name[MAXREGPATHLEN+1];
	char path[MAXREGPATHLEN+1];
	VERSION ver;

	parse(cmd, name, &ver, path);
	error("VR_Install", VR_Install(name, path, &ver));

}	// vInstall

void vPack(char *cmd)
{
	error("VR_PackRegistry", VR_PackRegistry(0));

}	// vPack

void vPretend(char *cmd)
{

	if (!cmd)
	{
		gPretend = !!gPretend;
	}
	else
	{
		if (stricmp(cmd, "ON") == 0)
			gPretend = 1;
		else
			gPretend = 0;
	}

	if (gVerbose)
		printf("Pretend mode is %s\n", gPretend ? "ON" : "OFF");

}	// vPretend

void vRemove(char *cmd)
{

	error("VR_Remove", VR_Remove(cmd));

}	// vRemove


void vSave(char *cmd)
{

	error("VR_Checkpoint", VR_Checkpoint());

}	// vSave


void vTest(char *cmd)
{

	VERSION ver;
	ver.major = 4;
	ver.minor = 0;
	ver.release = 0;
	ver.build = 237;
	ver.check = gPretend ? 0xad : 0;

	if (error("VR_Install", VR_Install("Navigator/NS.exe", 
		"c:\\Program Files\\Netscape\\Navigator\\Program\\NETSCAPE.EXE", &ver)))
		return;
	if (error("VR_Install", VR_Install("Navigator/Help", 
		"c:\\Program Files\\Netscape\\Navigator\\Program\\NETSCAPE.HLP", &ver)))
		return;
	if (error("VR_Install", VR_Install("Navigator/NSPR", 
		"c:\\Program Files\\Netscape\\Navigator\\Program\\NSPR32.DLL", &ver)))
		return;
	if (error("VR_Install", VR_Install("Navigator/Player", 
		"c:\\Program Files\\Netscape\\Navigator\\Program\\NSPLAYER.EXE", &ver)))
		return;
	if (error("VR_Install", VR_Install("Navigator/NSJava", 
		"c:\\Program Files\\Netscape\\Navigator\\Program\\NSJAVA32.DLL", &ver)))
		return;
	if (error("VR_Install", VR_Install("Web/Certificate.DB", 
		"c:\\Program Files\\Netscape\\Navigator\\Program\\CERT.DB", &ver)))
		return;
	if (error("VR_Install", VR_Install("Web/CertificateNI.DB", 
		"c:\\Program Files\\Netscape\\Navigator\\Program\\CERTNI.DB", &ver)))
		return;
	if (error("VR_Install", VR_Install("Web/Keys", 
		"c:\\Program Files\\Netscape\\Navigator\\Program\\KEY.DB", &ver)))
		return;
	if (error("VR_Install", VR_Install("MailNews/Postal", 
		"c:\\Program Files\\Netscape\\Navigator\\System\\POSTAL32.DLL", &ver)))
		return;
	if (error("VR_Install", VR_Install("MailNews/Folders/Inbox", 
		"c:\\Program Files\\Netscape\\Navigator\\Mail\\INBOX.SNM", &ver)))
		return;
	if (error("VR_Install", VR_Install("MailNews/Folders/Sent", 
		"c:\\Program Files\\Netscape\\Navigator\\Mail\\SENT.SNM", &ver)))
		return;
	if (error("VR_Install", VR_Install("MailNews/Folders/Trash", 
		"c:\\Program Files\\Netscape\\Navigator\\Mail\\TRASH.SNM", &ver)))
		return;
	if (error("VR_Install", VR_Install("Components/NUL", 
		"c:\\Program Files\\Netscape\\Navigator\\Program\\Plugins\\NPNUL32.DLL", &ver)))
		return;
	if (error("VR_Install", VR_Install("Components/PointCast", 
		"c:\\Program Files\\Netscape\\Navigator\\Program\\Plugins\\NPPCN32.DLL", &ver)))
		return;
	if (error("VR_Install", VR_Install("Components/AWT", 
		"c:\\Program Files\\Netscape\\Navigator\\Program\\Java\\bin\\AWT3220.DLL", &ver)))
		return;
	if (error("VR_Install", VR_Install("Components/MM", 
		"c:\\Program Files\\Netscape\\Navigator\\Program\\Java\\bin\\MM3220.DLL", &ver)))
		return;
	if (error("VR_Install", VR_Install("Java/Classes.Zip", 
		"c:\\Program Files\\Netscape\\Navigator\\Program\\Java\\classes\\MOZ2_0.ZIP", &ver)))
		return;
	if (error("VR_Install", VR_Install("Java/Classes Directory", 
		"c:\\Program Files\\Netscape\\Navigator\\Program\\Java\\classes\\MOZ2_0", &ver)))
		return;


}	// vTest


void vUpdate(char *cmd)
{

	char name[MAXREGPATHLEN+1];
	char path[MAXREGPATHLEN+1];
	VERSION ver;

	parse(cmd, name, &ver, path);
	error("VR_Update", VR_Update(name, path, &ver));

}	// vUpdate


void vVerify(char *cmd)
{

	error("VR_CheckEntry", VR_CheckEntry(0, cmd));

}	// vVerify

			
void interp(void)
{

	char line[256];
	char *p;

	while(1)
	{
		putchar('>');
		putchar(' ');
		flushall();
		gets(line);

		// p points to next word after verb on command line
		p = line;
		while (*p && *p!=' ')
			p++;
		if (!*p)
			p = 0;
		else
		{
			while(*p && *p==' ')
				p++;
		}

		switch(toupper(line[0]))
		{
		case 'B':
			vVerbose(p);
			break;
		case 'C':
			vCreate(p);
			break;
		case 'D':
			vDisplay(p);
			break;
		case 'F':
			vFind(p);
			break;
		case 'H':
		default:
			vHelp(line);
			break;
		case 'I':
			vInstall(p);
			break;
		case 'K':
			vPack(p);
			break;
		case 'P':
			vPretend(p);
			break;
		case 'R':
			vRemove(p);
			break;
		case 'S':
			vSave(p);
			break;
		case 'T':
			vTest(p);
			break;
		case 'U':
			vUpdate(p);
			break;
		case 'V':
			vVerify(p);
			break;
		case 'Q':
		case 'X':
			vSave(0);
			return;
		}	// switch
	}	// while

	assert(0);
	return;	// shouldn't get here

}	// interp

// EOF: interp.c
