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

#ifndef	_RDF_GLUE_H_
#define	_RDF_GLUE_H_


#include "xp.h"
#include "xp_mem.h"
#include "net.h"
#include "ntypes.h"
#include "fe_proto.h"

#ifdef NSPR20
#include "prinit.h"
#else
#include "prglobal.h"
#endif

#ifdef XP_UNIX
#include <sys/fcntl.h>
#elif defined(XP_MAC)
#include <fcntl.h>
#endif

#include "rdf.h"
#include "rdf-int.h"
#include "ht.h"



#define	APPLICATION_RDF		"application/x-rdf"		/* XXX what should these be? */
#define	EXTENSION_RDF		".rdf"
#define	EXTENSION_MCF		".mcf"



/* external globals */
extern char* gLocalStoreURL;
extern char* gBookmarkURL;



/* glue.c prototypes */

XP_BEGIN_PROTOS

#ifdef NSPR20
void ht_fprintf(PRFileDesc *file, const char *fmt, ...);
#else
void ht_fprintf(PRFileHandle file, const char *fmt, ...);
#endif



unsigned int		rdf_write_ready(NET_StreamClass *stream);
void			rdf_complete(NET_StreamClass *stream);
void			rdf_abort(NET_StreamClass *stream, int status);
NET_StreamClass *	rdf_Converter(FO_Present_Types format_out,
					void *client_data, URL_Struct *urls, MWContext *cx);
void			rdf_GetUrlExitFunc (URL_Struct *urls, int status, MWContext *cx);
int			rdfRetrievalType (RDFFile f);
int			rdf_GetURL (MWContext *cx,  int method,
					Net_GetUrlExitFunc *exit_routine, RDFFile rdfFile);
void			possiblyRereadRDFFiles (void* data);
void			RDFglueInitialize (void);
void			RDFglueExit (void);
void			*gRDFMWContext();
void			beginReadingRDFFile (RDFFile file);
void			readLocalFile (RDFFile file);

char			*unescapeURL(char *inURL);
char			*convertFileURLToNSPRCopaceticPath(char* inURL);
PRFileDesc		*CallPROpenUsingFileURL(char *fileURL, PRIntn flags, PRIntn mode);
PRDir			*CallPROpenDirUsingFileURL(char *fileURL);
int32			CallPRWriteAccessFileUsingFileURL(char *fileURL);
int32			CallPRDeleteFileUsingFileURL(char *fileURL);
int			CallPR_RmDirUsingFileURL(char *dirURL);
int32			CallPRMkDirUsingFileURL(char *dirURL, int mode);
DB			*CallDBOpenUsingFileURL(char *fileURL, int flags,int mode,
						DBTYPE type, const void *openinfo);

XP_END_PROTOS

#endif
