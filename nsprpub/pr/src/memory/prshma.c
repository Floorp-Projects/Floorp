/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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

/*
** prshma.h -- NSPR Anonymous Shared Memory
**
** 
*/

#include "primpl.h"

extern PRLogModuleInfo *_pr_shma_lm;

#if defined(XP_UNIX)
/* defined in pr/src/md/unix/uxshm.c */
#elif defined(WIN32)
/* defined in pr/src/md/windows/w32shm.c */
#else
extern PRFileMap * _PR_MD_OPEN_ANON_FILE_MAP( const char *dirName, PRSize size, PRFileMapProtect prot )
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return NULL;
}
extern PRStatus _PR_MD_EXPORT_FILE_MAP_AS_STRING(PRFileMap *fm, PRSize bufSize, char *buf)
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}
extern PRFileMap * _PR_MD_IMPORT_FILE_MAP_FROM_STRING(const char *fmstring)
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return NULL;
}
#endif

/*
** PR_OpenAnonFileMap() -- Creates an anonymous file-mapped shared memory
**
*/
PR_IMPLEMENT(PRFileMap*)
PR_OpenAnonFileMap(
    const char *dirName,
    PRSize      size, 
    PRFileMapProtect prot
)
{
    return(_PR_MD_OPEN_ANON_FILE_MAP( dirName, size, prot ));
} /* end PR_OpenAnonFileMap() */

/*
** PR_ProcessAttrSetInheritableFileMap() -- Prepare FileMap for export  
**   to my children processes via PR_CreateProcess()
**
**
*/
PR_IMPLEMENT( PRStatus) 
PR_ProcessAttrSetInheritableFileMap( 
    PRProcessAttr   *attr,
    PRFileMap       *fm, 
    const char      *shmname
)
{
    PR_SetError( PR_NOT_IMPLEMENTED_ERROR, 0 );
    return( PR_FAILURE);
} /* end PR_ProcessAttrSetInheritableFileMap() */ 

/*
** PR_GetInheritedFileMap() -- Import a PRFileMap previously exported
**   by my parent process via PR_CreateProcess()
**
*/
PR_IMPLEMENT( PRFileMap *)
PR_GetInheritedFileMap( 
    const char *shmname 
)
{
    PRFileMap   *fm = NULL;
    PR_SetError( PR_NOT_IMPLEMENTED_ERROR, 0 );
    return( fm );
} /* end PR_GetInhteritedFileMap() */

/*
** PR_ExportFileMapAsString() -- Creates a string identifying a PRFileMap
**
*/
PR_IMPLEMENT( PRStatus )
PR_ExportFileMapAsString( 
    PRFileMap *fm,
    PRSize    bufSize,
    char      *buf
)
{
    return( _PR_MD_EXPORT_FILE_MAP_AS_STRING( fm, bufSize, buf ));
} /* end PR_ExportFileMapAsString() */

/*
** PR_ImportFileMapFromString() -- Creates a PRFileMap from the identifying string
**
**
*/
PR_IMPLEMENT( PRFileMap * )
PR_ImportFileMapFromString( 
    const char *fmstring
)
{
    return( _PR_MD_IMPORT_FILE_MAP_FROM_STRING(fmstring));
} /* end PR_ImportFileMapFromString() */
/* end prshma.c */
