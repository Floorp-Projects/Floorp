/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef	_RDF_GLUE_H_
#define	_RDF_GLUE_H_


#include "xp.h"
#include "xp_mem.h"
#include "net.h"
#include "ntypes.h"
#include "fe_proto.h"

#include "prinit.h"
#include "prthread.h"
#include "libevent.h"

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

void ht_fprintf(PRFileDesc *file, const char *fmt, ...);

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
void			*gRDFMWContext(RDFT db);
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
