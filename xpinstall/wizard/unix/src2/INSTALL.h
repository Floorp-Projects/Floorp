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

#ifndef _INSTALL_H_
#define _INSTALL_H_

#include "xpistub.h"
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

/*_____________________________________________ func ptrs __*/
typedef nsresult (*pfnXPI_Init) (const char *aProgramDir, pfnXPIProgress progressCB);
typedef nsresult (*pfnXPI_Install) (const char *file, const char *args, long flags);
typedef void     (*pfnXPI_Exit) ();
                                 
/*________________________________________________ arrays __*/
#define CORE_LIB_COUNT 10
static char core_libs[CORE_LIB_COUNT*2][32] = 
{
/*      Archive Subdir      File                            */
/*      --------------      ----                            */
		"bin/", 			"libjsdom.so", 
		"bin/", 			"libmozjs.so",
		"bin/", 			"libnspr3.so",
		"bin/",				"libplc3.so",
		"bin/", 			"libplds3.so",
		"bin/",				"libxpcom.so",
		"bin/",				"libxpistub.so",
		"bin/components/",	"libxpinstall.so",
		"bin/components/",	"libjar50.so",
		"bin/",				"component.reg"
};

/*_______________________________________________ structs __*/
typedef struct _xpistub_t
{
	const char		*name;
	void			*handle;
	pfnXPI_Init		fn_init;
	pfnXPI_Install	fn_install;
	pfnXPI_Exit		fn_exit;
} xpistub_t;
	
typedef struct _xpi_t 
{
	char 			name[32];
	struct _xpi_t 	*next;
} xpi_t;

/*_________________________________________________ funcs __*/
int 		main(int argc, char **argv);
int			init();
int			extract_core();
int			load_stub();
void		progress_callback(const char *msg, PRInt32 max, PRInt32 val);
int			build_list(xpi_t **listhead);
int 		install_all(xpi_t *listhead);
int			install_one(xpi_t *pkg);
int			shutdown(xpi_t *list);

/*________________________________________________ macros __*/
#ifdef DEBUG_sgehani___
#define DUMP(_msg)                         \
	printf("%s %d: ", __FILE__, __LINE__); \
	printf(_msg);                          \
	printf("\n");
#else
#define DUMP(_msg) 
#endif

#define XPISTUB		"libxpistub.so"
#define FN_INIT		"XPI_Init"
#define FN_INSTALL	"XPI_Install"
#define FN_EXIT		"XPI_Exit"

#ifndef RTLD_NOW
#define RTLD_NOW 0
#endif 

#ifndef NULL
#define NULL 0
#endif

#define CORE_ARCHIVE "modules/install.xpi"
#define MANIFEST     "modules/MANIFEST"

#define ERR_CHECK(_func)                 \
{                                        \
	int ret_err = XI_OK;                 \
	ret_err = _func;                     \
	if (ret_err != XI_OK)                \
	{                                    \
		PRINT_ERR(ret_err);              \
		return ret_err;                  \
	}                                    \
}

#ifdef DEBUG_sgehani
#define PRINT_ERR(_err) \
		printf("%s %d: err = %d \n", __FILE__, __LINE__, _err); 
#endif

/*________________________________________________ errors __*/
#define XI_OK			0
#define XI_NULL_PTR		-1
#define XI_OUT_OF_MEM	-2
#define XI_FAIL			-3
#define XI_LIB_OPEN 	-4
#define XI_LIB_SYM		-5
#define XI_XPI_FAIL		-6
#define XI_NO_MANIFEST	-7

#endif /* _INSTALL_H_ */
