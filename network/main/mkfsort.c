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
/*
 *
 * routines to sort an array of file objects
 * uses qsort alg
 *
 * Designed and implemented by Lou Montulli '94
 */

#include "mkutils.h"
#include "mkfsort.h"
#include "mkgeturl.h"

#ifdef PROFILE
#pragma profile on
#endif

PRIVATE int net_file_sort_method = SORT_BY_NAME;

/* print a text string in place of the NET_FileEntryInfo special_type int */
PUBLIC char *
NET_PrintFileType(int special_type)
{
	switch(special_type)
	  {
		case NET_FILE_TYPE:
			return("FILE");
		case NET_DIRECTORY:
			return("DIRECTORY");
		case NET_SYM_LINK:
			return("SYMBOLIC-LINK");
		case NET_SYM_LINK_TO_DIR:
			return("SYM-DIRECTORY");
		case NET_SYM_LINK_TO_FILE:
			return("SYM-FILE");
		default:
			PR_ASSERT(0);
			return("FILE");
	  }
}

PRIVATE void 
NET_SetFileSortMethod(int method)
{
	net_file_sort_method = method;
}

MODULE_PRIVATE void NET_FreeEntryInfoStruct(NET_FileEntryInfo *entry_info)
{
    if(entry_info) 
      {
        FREEIF(entry_info->filename);
        /* free the struct */
        PR_Free(entry_info);
      }
}

MODULE_PRIVATE NET_FileEntryInfo * NET_CreateFileEntryInfoStruct (void)
{
    NET_FileEntryInfo * new_entry = PR_NEW(NET_FileEntryInfo);

    if(!new_entry) 
       return(NULL);

	memset(new_entry, 0, sizeof(NET_FileEntryInfo));

	new_entry->permissions = -1;

	return(new_entry);

}

/* This function is used as a comparer function for the Qsort routine.
 * It uses a function FE_FileSortMethod() to determine the
 * field to sort on.
 *
 */
PRIVATE int
NET_CompareFileEntryInfoStructs (const void *ent2, const void *ent1)
{
    int status;
    const NET_FileEntryInfo *entry1 = *(NET_FileEntryInfo **) ent1;
    const NET_FileEntryInfo *entry2 = *(NET_FileEntryInfo **) ent2;

	if(!entry1 || !entry2)
		return(-1);

    switch(net_file_sort_method)
      {
        case SORT_BY_SIZE:
                        /* both equal or both 0 */
                        if(entry1->size == entry2->size)
                            return(PL_strcmp(entry2->filename, entry1->filename));
                        else
                            if(entry1->size > entry2->size)
                                return(-1);
                            else
                                return(1);
                        /* break; NOT NEEDED */
        case SORT_BY_TYPE:
                        if(entry1->cinfo && entry1->cinfo->desc && 
										entry2->cinfo && entry2->cinfo->desc) 
                          {
                            status = PL_strcmp(entry1->cinfo->desc, entry2->cinfo->desc);
                            if(status)
                                return(status);
                            /* else fall to filename comparison */
                          }
                        return (PL_strcmp(entry2->filename, entry1->filename));
                        /* break; NOT NEEDED */
        case SORT_BY_DATE:
                        if(entry1->date == entry2->date) 
                            return(PL_strcmp(entry2->filename, entry1->filename));
                        else
                            if(entry1->size > entry2->size)
                                return(-1);
                            else
                                return(1);
                        /* break; NOT NEEDED */
        case SORT_BY_NAME:
        default:
                        return (PL_strcmp(entry2->filename, entry1->filename));
      }
}

/* sort the files
 */
MODULE_PRIVATE void
NET_DoFileSort(SortStruct * sort_list)
{
    NET_DoSort(sort_list, NET_CompareFileEntryInfoStructs);
}

#define PD_PUTS(s)  \
do { \
if(status > -1) \
	status = (*stream->put_block)(stream, s, PL_strlen(s)); \
} while(0)

PUBLIC int
NET_PrintDirectory(SortStruct **sort_base, NET_StreamClass * stream, char * path, URL_Struct *URL_s)
{
    NET_FileEntryInfo * file_entry;
    char out_buf[3096];
	char *esc_path = NET_Escape(path, URL_PATH);
    int i;
	int status=0;

    NET_DoFileSort(*sort_base);

	/* emit 300: URL CRLF */
	PL_strcpy(out_buf, "300: ");
    PD_PUTS(out_buf);
    PR_snprintf(out_buf, sizeof(out_buf), URL_s->address);
    PD_PUTS(out_buf);

	PL_strcpy(out_buf, CRLF);
    PD_PUTS(out_buf);

	/* emit 200: Filename Size Content-Type File-type Last-Modified */
	PL_strcpy(out_buf, "200: Filename Content-Length Content-Type File-type Last-Modified"CRLF);
    PD_PUTS(out_buf);

    for(i=0; status > -1 && (file_entry = (NET_FileEntryInfo *) 
                                NET_SortRetrieveNumber(*sort_base, i)) != 0;
			 i++)
      {


		char * esc_time = NET_Escape(ctime(&file_entry->date), URL_XALPHAS);

		strtok(file_entry->filename, "/");
        PR_snprintf(out_buf, sizeof(out_buf), "201: %s %ld %s %s %s"CRLF, 
                    file_entry->filename, 
                    file_entry->size,
                    file_entry->special_type == NET_DIRECTORY ? "application/http-index-format" :
                        file_entry->cinfo ? file_entry->cinfo->type : TEXT_PLAIN,
                    file_entry->special_type == NET_DIRECTORY ? "Directory" : "File",
                    esc_time);

		PR_Free(esc_time);
        
        PD_PUTS(out_buf);

        NET_FreeEntryInfoStruct(file_entry);
      }

    NET_SortFree(*sort_base);
    *sort_base = 0;

	PR_FREEIF(esc_path);
    if(status < 0)
        return(status);
	return(MK_DATA_LOADED);
}

#ifdef PROFILE
#pragma profile off
#endif

