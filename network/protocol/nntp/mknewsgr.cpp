/* -*- Mode: C; tab-width: 4 -*- */
#include "mkutils.h"
#include "msgcom.h"
#include "xp_reg.h"
#include "xp_qsort.h"

/* UNIX doesn't need this stuff since it uses the file system
 * to store the mappings as filenames
 */

XP_List *HostMapList=0;
Bool NewsrcMapDirty=FALSE;     

typedef struct _NewsrcHostMap {
	char * filename;
	char * psuedo_name;
	Bool   is_newsgroups_file;	/* This was Bool * */
} NewsrcHostMap;

/* register a newsrc file mapping
 */
PUBLIC Bool
NET_RegisterNewsrcFile(char * filename, 
					   char *hostname, 
					   Bool bs,
					   Bool is_newsgroups_file)
{
	NewsrcHostMap *host_map_struct = PR_NEW(NewsrcHostMap);
    XP_List * list_prt;
	NewsrcHostMap *tmp_host_map_struct;

	if(!host_map_struct)
		return(FALSE);

	memset(host_map_struct, 0, sizeof(NewsrcHostMap));
	
	/* you losers, there's no such thing as XP_Assert in xfe!! */
	assert(filename);
	assert(hostname);

	if(bs)
		StrAllocCopy(host_map_struct->psuedo_name, "snewsrc-");
	else
		StrAllocCopy(host_map_struct->psuedo_name, "newsrc-");

	StrAllocCat(host_map_struct->psuedo_name, hostname);

	StrAllocCopy(host_map_struct->filename, filename);

	host_map_struct->is_newsgroups_file = is_newsgroups_file;

	if(!HostMapList)
		HostMapList = XP_ListNew();

	if(!HostMapList)
		return(FALSE);

	NewsrcMapDirty = TRUE;

	/* search for duplicate mappings */
	list_prt = HostMapList;
    while((tmp_host_map_struct = (NewsrcHostMap *)XP_ListNextObject(list_prt)) != NULL)
      {
        if(tmp_host_map_struct->is_newsgroups_file == is_newsgroups_file
			&& !PL_strcasecmp(tmp_host_map_struct->psuedo_name, 
					   		host_map_struct->psuedo_name))
		  {
			XP_ListRemoveObject(HostMapList, tmp_host_map_struct);
			FREEIF(tmp_host_map_struct->psuedo_name);
			FREEIF(tmp_host_map_struct->filename);
			FREE(tmp_host_map_struct);

			break; /* end search */
		  }
      }

	XP_ListAddObjectToEnd(HostMapList, host_map_struct);

	NET_SaveNewsrcFileMappings();

	return(TRUE);
}

/* removes a newshost to file mapping.  Removes both
 * the newsrc and the newsgroups mapping
 * this function does not remove the actual files,
 * that is left up to the caller
 */
PUBLIC void
NET_UnregisterNewsHost(const char *host, 
					   Bool bs)
{
    XP_List * list_prt = HostMapList;
    NewsrcHostMap *host_map_struct;
	char *hostname;

	PR_ASSERT(host);

	if(!host)
		return;

    /* Run through the entire list and remove all entries that
	 * match that hostname
	 */
    while((host_map_struct = (NewsrcHostMap *)XP_ListNextObject(list_prt)) != NULL)
      {

        if(bs && *host_map_struct->psuedo_name != 's')
            continue;

        if(!bs && *host_map_struct->psuedo_name == 's')
            continue;

        hostname = PL_strchr(host_map_struct->psuedo_name, '-');

        if(!hostname)
            continue;

        hostname++;

        if(PL_strcasecmp(hostname, host))
            continue;

		XP_ListRemoveObject(HostMapList, host_map_struct);

		NewsrcMapDirty = TRUE;

		/* free the data */
        FREEIF(host_map_struct->psuedo_name);
        FREEIF(host_map_struct->filename);
        FREE(host_map_struct);

		/* we must start over in the list because the
		 * act of removeing an item will cause purity
		 * errors if we don't start over
		 */
		list_prt = HostMapList;
      }
	NET_SaveNewsrcFileMappings();
}

/* map a filename and security status into a filename
 *
 * returns NULL on error or no mapping.
 */
PUBLIC char *
NET_MapNewsrcHostToFilename(char * host, 
							Bool bs, 
							Bool is_newsgroups_file)
{
    XP_List * list_prt = HostMapList;
	NewsrcHostMap *host_map_struct;
	char *hostname;

    while((host_map_struct = (NewsrcHostMap *)XP_ListNextObject(list_prt)) != NULL)
      {
		if(is_newsgroups_file != host_map_struct->is_newsgroups_file)
			continue;

		if(bs && *host_map_struct->psuedo_name != 's')
			continue;
		
		if(!bs && *host_map_struct->psuedo_name == 's')
			continue;

		hostname = PL_strchr(host_map_struct->psuedo_name, '-');
		
		if(!hostname)
			continue;

		hostname++;

		if(PL_strcasecmp(hostname, host))
			continue;

		return(host_map_struct->filename);

	  }

	return(NULL);
}

#define NEWSRC_MAP_FILE_COOKIE "netscape-newsrc-map-file"

/* save newsrc file mappings from memory onto disk
 */
PUBLIC Bool
NET_SaveNewsrcFileMappings()
{
	XP_List * list_prt = HostMapList;
	NewsrcHostMap *host_map_struct;
	XP_File fp;

	if(!NewsrcMapDirty)
		return(TRUE);

	if(!(fp = XP_FileOpen("", xpNewsrcFileMap, XP_FILE_WRITE_BIN)))
	  {
		return(FALSE);
	  }

	XP_FileWrite(NEWSRC_MAP_FILE_COOKIE, PL_strlen(NEWSRC_MAP_FILE_COOKIE), fp);
	XP_FileWrite(LINEBREAK, PL_strlen(LINEBREAK), fp);

	while((host_map_struct = (NewsrcHostMap *)XP_ListNextObject(list_prt)) != NULL)
	  {

		XP_FileWrite(host_map_struct->psuedo_name, 
					 PL_strlen(host_map_struct->psuedo_name),
					 fp);
		
		XP_FileWrite("\t", 1, fp);

		XP_FileWrite(host_map_struct->filename, 
					 PL_strlen(host_map_struct->filename),
					 fp);
		
		XP_FileWrite("\t", 1, fp);

		if(host_map_struct->is_newsgroups_file)
		    XP_FileWrite("TRUE", 4, fp);
		else
		    XP_FileWrite("FALSE", 5, fp);

		XP_FileWrite(LINEBREAK, PL_strlen(LINEBREAK), fp);
	  }

	XP_FileClose(fp);

	NewsrcMapDirty = FALSE;

	return(TRUE);
}

/* read newsrc file mappings from disk into memory
 */
PUBLIC Bool
NET_ReadNewsrcFileMappings()
{
	XP_File fp;
	char buffer[512];
	char psuedo_name[512];
	char filename[512];
	char is_newsgroup[512];
	NewsrcHostMap * hs_struct;

    if(!(fp = XP_FileOpen("", xpNewsrcFileMap, XP_FILE_READ_BIN)))
      {
        return(FALSE);
      }

    if(!HostMapList)
        HostMapList = XP_ListNew();

	/* get the cookie and ignore */
	XP_FileReadLine(buffer, sizeof(buffer), fp);

	while(XP_FileReadLine(buffer, sizeof(buffer), fp))
	  {
        char * p;
        int i;

		hs_struct = PR_NEW(NewsrcHostMap);

		if(!hs_struct)
			return(FALSE);

		memset(hs_struct, 0, sizeof(NewsrcHostMap));

        /*
            This used to be scanf() call which would incorrectly
            parse long filenames with spaces in them.  - JRE
        */

        filename[0] = '\0';
        is_newsgroup[0]='\0';

        for (i = 0, p = buffer; *p && *p != '\t' && i < 500; p++, i++)
            psuedo_name[i] = *p;
        psuedo_name[i] = '\0';
        if (*p) 
          {
            for (i = 0, p++; *p && *p != '\t' && i < 500; p++, i++)
                filename[i] = *p;
            filename[i]='\0';
            if (*p) 
              {
                for (i = 0, p++; *p && *p != '\r' && i < 500; p++, i++)
                    is_newsgroup[i] = *p;
                is_newsgroup[i]='\0';
              }
          }

		StrAllocCopy(hs_struct->psuedo_name, psuedo_name);
		StrAllocCopy(hs_struct->filename, filename);

		if(!XP_STRNCMP(is_newsgroup, "TRUE", 4))
			hs_struct->is_newsgroups_file = TRUE;

		XP_ListAddObjectToEnd(HostMapList, hs_struct);		
	  }


    XP_FileClose(fp);

	return(TRUE);
}

PUBLIC void 
NET_FreeNewsrcFileMappings(void)
{
    NewsrcHostMap *host_map_struct;

    while((host_map_struct = (NewsrcHostMap *)XP_ListRemoveTopObject(HostMapList)) != NULL)
      {
        FREEIF(host_map_struct->psuedo_name);
        FREEIF(host_map_struct->filename);
        FREE(host_map_struct);
      }
}


#if defined(DEBUG) && defined(XP_UNIX)

PUBLIC void
TestNewsrcFileMappings(void)
{

  HG04206;
  printf("insec flop maps to: %s\n", NET_MapNewsrcHostToFilename("flop", FALSE, FALSE));
  printf("insec news maps to: %s\n", NET_MapNewsrcHostToFilename("news", FALSE, FALSE));

  NET_RegisterNewsrcFile("newsrc-flop", "flop", FALSE, FALSE);
  NET_RegisterNewsrcFile("snewsrc-flop", "flop", TRUE, FALSE);
  NET_RegisterNewsrcFile("newsrc-news", "news", FALSE, FALSE);

  HG04206;
  printf("insec flop maps to: %s\n", NET_MapNewsrcHostToFilename("flop", FALSE, FALSE));
  printf("insec news maps to: %s\n", NET_MapNewsrcHostToFilename("news", FALSE, FALSE));

	printf("saving mappings\n");
  NET_SaveNewsrcFileMappings();
	printf("freeing mappings\n");
  NET_FreeNewsrcFileMappings();
	printf("reading mappings\n");
  NET_ReadNewsrcFileMappings();
  
  printf("adding duplicates\n");
  NET_RegisterNewsrcFile("newsrc-flop", "flop", FALSE, FALSE);
  NET_RegisterNewsrcFile("snewsrc-flop", "flop", TRUE, FALSE);
  NET_RegisterNewsrcFile("newsrc-news", "news", FALSE, FALSE);

  HG04206;
  printf("insec flop maps to: %s\n", NET_MapNewsrcHostToFilename("flop", FALSE, FALSE));
  printf("insec news maps to: %s\n", NET_MapNewsrcHostToFilename("news", FALSE, FALSE));
}
#endif

/* XP_GetNewsRCFiles returns a null terminated array
 * of pointers to malloc'd filename's.  Each filename
 * represents a different newsrc file.
 *
 * return only the filename since the path is not needed
 * or wanted.
 *
 * Netlib is expecting a string of the form:
 *  [s]newsrc-host.name.domain[:port]
 *
 * examples:
 *    newsrc-news.mcom.com
 *    snewsrc-flop.mcom.com
 *    newsrc-news.mcom.com:118
 *    snewsrc-flop.mcom.com:1191
 *
 * the port number is optional and should only be
 * there when different from the default.
 * the "s" in front of newsrc signifies that
 * security is to be used.
 *
 * return NULL on critical error or no files
 */                                   
#if !defined(XP_UNIX) 
PUBLIC char ** XP_GetNewsRCFiles(void)
{
	/* @@@ max number of newsrc's is 512
	 */
	char ** rv = (char **)PL_Malloc(sizeof(char*)*512);
    XP_List * list_prt = HostMapList;
	NewsrcHostMap *host_map_struct;
	int count=0;

	if(!rv)
		return(NULL);

	memset(rv, 0, sizeof(char*)*512);

	while((host_map_struct = (NewsrcHostMap *)XP_ListNextObject(list_prt)) != NULL)
      {

        if(host_map_struct->is_newsgroups_file)
            continue;

		StrAllocCopy(rv[count++], host_map_struct->psuedo_name);

		if(count >= 510)
			break;
	  }

	return(rv);
}
#endif /* UNIX */


