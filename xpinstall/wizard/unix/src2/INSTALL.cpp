/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/ 
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License 
 * for the specific language governing rights and limitations under the 
 * License. 
 * 
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are Copyright (C) 1999
 * Netscape Communications Corporation. All Rights Reserved.  
 * 
 * Contributors:
 *     Samir Gehani <sgehani@netscape.com>
 */

#include "INSTALL.h"

static xpistub_t gstub;

int 
main(int argc, char **argv)
{
	int 		err = XI_OK;
	xpi_t 		*pkglist;

	ERR_CHECK( init() );
	ERR_CHECK( build_list(&pkglist) );
	ERR_CHECK( install_all(pkglist) );
	ERR_CHECK( shutdown(pkglist) );

	return err;
}

int
init()
{
	nsresult rv = NS_OK;
	char progdir[512];
	int err = XI_OK;
	DUMP("initialize");

	err = extract_core();
	if (err != XI_OK)
	{
		PRINT_ERR(err);
		shutdown(NULL);
		return err;
	}

	err = load_stub();
	if (err != XI_OK)
	{
		PRINT_ERR(err);
		shutdown(NULL);
		return err;
	}

	getcwd(progdir, 512);
	rv = gstub.fn_init(progdir, progress_callback);
	if (NS_FAILED(rv))
	{
		PRINT_ERR(rv);
		shutdown(NULL);
		return XI_XPI_FAIL;
	}
	DUMP("XPI_Init called");

	return err;
}

int
extract_core()
{
	char cmd[256];
	int i;
	int err = XI_OK;
	DUMP("extract_core");

	for (i = 0; i < CORE_LIB_COUNT*2; i++)
	{
		strcpy(cmd, "unzip -j ");    /* -j = junk paths; install here */
		strcat(cmd, CORE_ARCHIVE);   /* relative path to install.xpi */
		strcat(cmd, " -d . ");       /* -d = destination root */
		strcat(cmd, core_libs[i]);  /* archive subdir */
		strcat(cmd, core_libs[++i]);/* lib file name */
		strcat(cmd, "\0");
		DUMP(cmd);

		system(cmd);
	}

	return err;
}

int
load_stub()
{
	char libpath[512];
	char *dlerr;
	int err = XI_OK;
	DUMP("load_stub");

	/* get the working dir and tack on lib name */
	getcwd(libpath, 512);
	strncat(libpath, "/", 1);
	strncat(libpath, XPISTUB, strlen(XPISTUB));
	DUMP(libpath); 

	/* open the library */
	gstub.handle = NULL;
	gstub.handle = dlopen(libpath, RTLD_NOW);
	if (!gstub.handle)
	{
		dlerr = dlerror();
		DUMP(dlerr);
		return XI_LIB_OPEN;
	}
	DUMP("xpistub opened");
	
	/* read and store symbol addresses */
	gstub.fn_init    = (pfnXPI_Init) dlsym(gstub.handle, FN_INIT);
	gstub.fn_install = (pfnXPI_Install) dlsym(gstub.handle, FN_INSTALL);
	gstub.fn_exit    = (pfnXPI_Exit) dlsym(gstub.handle, FN_EXIT);
	if (!gstub.fn_init || !gstub.fn_install || !gstub.fn_exit)
	{
		dlerr = dlerror();
		DUMP(dlerr);
		return XI_LIB_SYM;
	}

	return err;
}

void
progress_callback(const char* msg, PRInt32 max, PRInt32 val)
{
	DUMP("progress_callback");
	if (max == 0)
	{
		if (val > 0)
			printf("Preparing item %d ...\n", val);
	}
	else
		printf("Installing item %d of %d ...\n", val, max);
}

int 
build_list(xpi_t **listhead)
{
	xpi_t *currxpi = NULL, *lastxpi = NULL;
	char mfpath[512];
	char *buf = NULL, *pcurr, curr[32];
	fpos_t mfeof;
	FILE *fdmf;
	size_t rd = 0;
	int len = 0, total, head;
	int err = XI_OK;
	DUMP("build_list");

	if (!listhead)
		return XI_NULL_PTR;

	*listhead = NULL;

/* XXX optionally set mf path in cmd line args */
	strcpy(mfpath, MANIFEST);

	/* open manifest file */
	fdmf = NULL;
	fdmf = fopen(mfpath, "r");
	if (!fdmf)	
	{
		err = XI_NO_MANIFEST;
		goto bail;
	}
	DUMP("opened manifest");

	/* read the file into a buffer */
	if (fseek(fdmf, 0, SEEK_END) != 0)
	{
		err = XI_NO_MANIFEST;
		goto bail;
	}
	if (fgetpos(fdmf, &mfeof) != 0)
	{
		err = XI_NO_MANIFEST;
		goto bail;
	}
	DUMP("eof manifest found");

	buf = (char*) malloc(mfeof * sizeof(char));
	if (!buf)
	{
		err = XI_OUT_OF_MEM;
		goto bail;
	}
	DUMP("malloc'd manifest buf");

	if (fseek(fdmf, 0, SEEK_SET) != 0)
	{
		err = XI_NO_MANIFEST;
		goto bail;
	}
	rd = fread( (void*)buf, 1, mfeof, fdmf);
	if (!rd)
	{
		err = XI_NO_MANIFEST;
		goto bail;
	}
	DUMP(buf);

	/* loop over each line reading in a .xpi module 
	 * name unless commented with ';' 
	 */
	head = 1;
	for (pcurr = buf, total = 0; total < mfeof; pcurr = strchr(pcurr, '\n')+1)
	{	
		len = strchr(pcurr, '\n') - pcurr;
		total += len + 1;
		strncpy(curr, pcurr, len);
		curr[ (len>32?32:len) ] = '\0';

		/* malloc a new xpi structure */
		currxpi = (xpi_t *) malloc(sizeof(xpi_t));
		if (!currxpi)
		{
			err = XI_OUT_OF_MEM;
			goto bail;
		}
		currxpi->next = NULL;
		if (head)
		{
			*listhead = currxpi;
			lastxpi = currxpi;
		}
		strncpy(currxpi->name, curr, (len>32?32:len));

		/* link to xpi list */	
		if (!head && *listhead)
		{
			lastxpi->next = currxpi;
			lastxpi = currxpi;
		}
		else
			head = 0;
	}

	/* close manifest file */
	fclose(fdmf);

	return err;

bail:
	if (buf)
		free(buf);
	shutdown(*listhead);
	return err;
}

int 
install_all(xpi_t *listhead)
{
	xpi_t *curr = listhead;
	int err = XI_OK;
	DUMP("install_all");

	if (!listhead)
		return XI_NULL_PTR;

	while (curr)
	{
		install_one(curr);
		curr = curr->next;
	}

	return err;
}

int
install_one(xpi_t *pkg)
{
	nsresult rv = NS_OK;
	char xpipath[512];
	int err = XI_OK;
	DUMP("install_one");

	getcwd(xpipath, 512);
	strncat(xpipath, "/modules/", 9);
	strncat(xpipath, pkg->name, strlen(pkg->name));

	DUMP("--------- installing xpi package: "); 
	DUMP(xpipath); 
	DUMP("---------\n");

/* XXX check if file exists: if not skip it  with status */

	rv = gstub.fn_install(xpipath, "", 0);
	if (NS_FAILED(rv))
	{
		err = XI_XPI_FAIL;
		printf("ERROR %d: Installation of %s failed.\n", rv, pkg->name);
	}
	else
		printf("Installed %s successfully.\n", pkg->name);

	return err;
}

int
shutdown(xpi_t *list)
{
	xpi_t *curr = list;
	int i;
	int err = XI_OK;
	DUMP("shutdown");

	/* release XPCOM and XPInstall */
	if (gstub.fn_exit)
	{
		gstub.fn_exit();
		DUMP("XPI_Exit called");
	}

	/* close xpistub library */
	if (gstub.handle)
	{
		dlclose(gstub.handle);
		DUMP("xpistub closed");
	}
		
	while (curr)
	{
		list = curr->next;
		free(curr);
		curr = list;
	}

	/* clean up extracted core libs */
	for (i=1; i<CORE_LIB_COUNT*2; i+=2)
	{
		unlink(core_libs[i]);
	}

	return err;
}
