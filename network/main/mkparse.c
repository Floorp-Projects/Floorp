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
/* URL/NET parsing routines
 * Additions/Changes by Judson Valeski 1997
 */
			   
#include "mkutils.h"
#include "netutils.h"
#include "xp.h"
#include "mkparse.h"
#include "xp_time.h"
#include "mkfsort.h"

extern int MK_OUT_OF_MEMORY;

#ifdef PROFILE
#pragma profile on
#endif

/* global */
XP_List * ExternalURLTypeList=0;

#define EXTERNAL_TYPE_URL  999  /* a special external URL type */

#define HEX_ESCAPE '%'

/* add an external URL type prefix 
 *
 * this prefix will be used to see if a URL is an absolute
 * or a relative URL.
 * 
 * you should just pass in the prefix of the URL
 *
 * for instance "http" or "ftp" or "foobar".
 *
 * do not pass in a colon or double slashes.
 */
PUBLIC void 
NET_AddExternalURLType(char * type)
{
	XP_List * list_ptr = ExternalURLTypeList;
	char * new_type=0;
	char * reg_type;

	while((reg_type = (char *)XP_ListNextObject(list_ptr)) != NULL)
	  {
		if(!PL_strcasecmp(reg_type, type))
			return;  /* ignore duplicates */
	  }

	StrAllocCopy(new_type, type);

	if(!ExternalURLTypeList)
		ExternalURLTypeList = XP_ListNew();

	XP_ListAddObject(ExternalURLTypeList, new_type);
	
}

/* remove an external URL type prefix 
 */
PUBLIC void 
NET_DelExternalURLType(char * type)
{
    XP_List * list_ptr = ExternalURLTypeList;
    char * reg_type;

    while((reg_type = (char *)XP_ListNextObject(list_ptr)) != NULL)
      {
        if(!PL_strcasecmp(reg_type, type))
		  {
			/* found it */
			XP_ListRemoveObject(ExternalURLTypeList, reg_type);
            return; /* done */ 
		  }
      }
}

/* check the passed in url to see if it is an
 * external type url
 */
PRIVATE int
net_CheckForExternalURLType(char * url)
{
    XP_List * list_ptr = ExternalURLTypeList;
    char * reg_type;
	int len;

    while((reg_type = (char *)XP_ListNextObject(list_ptr)) != NULL)
      {
		len = PL_strlen(reg_type);
        if(!PL_strncasecmp(reg_type, url, len) && url[len] == ':')
          { 
            /* found it */
            return EXTERNAL_TYPE_URL; /* done */ 
          }
      }

	return(0);
}

/* modifies a url of the form   /foo/../foo1  ->  /foo1
 *
 * it only operates on "file" "ftp" and "http" url's all others are returned
 * unmodified
 *
 * returns the modified passed in URL string
 */
PRIVATE char *
net_ReduceURL (char *url)
{
	int url_type = NET_URL_Type(url);
    char * fwd_ptr;
	char * url_ptr;
    char * path_ptr;

    if(!url)
        return(url);

	if(url_type == HTTP_TYPE_URL || url_type == FILE_TYPE_URL ||
	   url_type == FTP_TYPE_URL ||
	   url_type == SECURE_HTTP_TYPE_URL ||
	   url_type == MARIMBA_TYPE_URL)
	  {

		/* find the path so we only change that and not the host
		 */
		path_ptr = PL_strchr(url, '/');

		if(!path_ptr)
		    return(url);

		if(*(path_ptr+1) == '/')
		    path_ptr = PL_strchr(path_ptr+2, '/');
			
		if(!path_ptr)
		    return(url);

		fwd_ptr = path_ptr;
		url_ptr = path_ptr;

		for(; *fwd_ptr != '\0'; fwd_ptr++)
		  {
			
			if(*fwd_ptr == '/' && *(fwd_ptr+1) == '.' && *(fwd_ptr+2) == '/')
			  {
			    /* remove ./ 
		         */	
				fwd_ptr += 1;
			  }
			else if(*fwd_ptr == '/' && *(fwd_ptr+1) == '.' && *(fwd_ptr+2) == '.' && 
									(*(fwd_ptr+3) == '/' || *(fwd_ptr+3) == '\0'))
			  {
			    /* remove foo/.. 
		         */	
				/* reverse the url_ptr to the previous slash
				 */
				if(url_ptr != path_ptr) 
					url_ptr--; /* we must be going back at least one */
				for(;*url_ptr != '/' && url_ptr != path_ptr; url_ptr--)
					;  /* null body */
	
				/* forward the fwd_prt past the ../
				 */
				fwd_ptr += 2;
			  }
			else
			  {
				/* copy the url incrementaly 
				 */
				*url_ptr++ = *fwd_ptr;
			  }
		  }
		*url_ptr = '\0';  /* terminate the url */
	  }

	return(url);
}
		
/* Makes a relative URL into an absolute one.  
 *
 * If an absolute url is passed in it will be copied and returned.
 *
 * Always returns a malloc'd string or NULL on out of memory error
 */
PUBLIC char *
NET_MakeAbsoluteURL(char * absolute_url, char * relative_url)
{
	char * ret_url=0;
    int    new_length;
	char * cat_point=0;
	char   cat_point_char;
	int    url_type=0;
	int    base_type;

	/* if either is NULL
	 */
	if(!absolute_url || !relative_url)
	  {
		StrAllocCopy(ret_url, relative_url);
		return(ret_url);
	  }

	/* use the URL_Type function to figure
	 * out if it's a recognized URL method
	 */
    url_type = NET_URL_Type(relative_url);

	/* there are some extra cases we need to catch
	 */
	if(!url_type)
	  {
       switch(*relative_url) 
	    {
		  case 'i':
			if(!PL_strncmp(relative_url,"internal-icon-", 14)
		   		|| !PL_strncmp(relative_url,"internal-external-reconnect:", 28)
			 		|| !PL_strcmp(relative_url,"internal-external-plugin"))
            url_type = INTERNAL_IMAGE_TYPE_URL;
            break;
    	  case '/':
        	if(!PL_strncasecmp(relative_url, "/mc-icons/", 10) ||
               !PL_strncasecmp(relative_url, "/ns-icons/", 10))
          	  {
            	if(!PL_strcmp(relative_url+10, "menu.gif"))
                	url_type = INTERNAL_IMAGE_TYPE_URL;
            	else if(!PL_strcmp(relative_url+10, "unknown.gif"))
                	url_type = INTERNAL_IMAGE_TYPE_URL;
            	else if(!PL_strcmp(relative_url+10, "text.gif"))
                	url_type = INTERNAL_IMAGE_TYPE_URL;
            	else if(!PL_strcmp(relative_url+10, "image.gif"))
                	url_type = INTERNAL_IMAGE_TYPE_URL;
            	else if(!PL_strcmp(relative_url+10, "sound.gif"))
                	url_type = INTERNAL_IMAGE_TYPE_URL;
            	else if(!PL_strcmp(relative_url+10, "binary.gif"))
                	url_type = INTERNAL_IMAGE_TYPE_URL;
            	else if(!PL_strcmp(relative_url+10, "movie.gif"))
                	url_type = INTERNAL_IMAGE_TYPE_URL;
          	  }
        	break;
	    }
	  }
	else if(url_type == ABOUT_TYPE_URL)
	  {
		/* don't allow about:cache in a document */
		if(!PL_strncasecmp(relative_url, "about:cache", 11)
		   || !PL_strncasecmp(relative_url, "about:global", 12)
		   || !PL_strncasecmp(relative_url, "about:image-cache", 17)
		   || !PL_strncasecmp(relative_url, "about:memory-cache", 18))
		  {
			return PL_strdup("");
		  }
	  }

	if(!url_type)
		url_type = net_CheckForExternalURLType(relative_url);

    if(url_type)
      {
        /* it's either an absolute url
         * or a messed up one of the type proto:/path
         * but notice the missing host.
         */
		char * colon = PL_strchr(relative_url, ':'); /* must be there */

		if( (colon && *(colon+1) == '/' && *(colon+2) == '/') || 
			(url_type != GOPHER_TYPE_URL
			 && url_type != FTP_TYPE_URL
 			 && url_type != HTTP_TYPE_URL
			 && url_type != SECURE_HTTP_TYPE_URL
			 && url_type != RLOGIN_TYPE_URL
			 && url_type != TELNET_TYPE_URL
			 && url_type != TN3270_TYPE_URL
			 && url_type != WAIS_TYPE_URL) )
		  {
			/* it appears to have all it's parts.  Assume it's completely
			 * absolute
			 */
			StrAllocCopy(ret_url, relative_url);
			return(ret_url);
		  }
		else
		  {
			/* it's a screwed up relative url of the form http:[relative url]
			 * remove the stuff before and at the colon and treat it as a normal
			 * relative url
			 */
			char * colon = PL_strchr(relative_url, ':');

			relative_url = colon+1;
		  }
      }


	/* At this point, we know that `relative_url' is not absolute.
	   If the base URL, `absolute_url', is a "mailbox:" URL, then
	   we should not allow relative expansion: that is,
	   NET_MakeAbsoluteURL("mailbox:/A/B/C", "YYY") should not
	   return "mailbox:/A/B/YYY".

	   However, expansion of "#" and "?" parameters should work:
	   NET_MakeAbsoluteURL("mailbox:/A/B/C?id=XXX", "#part2")
	   should be allowed to expand to "mailbox:/A/B/C?id=XXX#part2".

	   If you allow random HREFs in attached HTML to mail messages to
	   expand to mailbox URLs, then bad things happen -- among other
	   things, an entry will be automatically created in the folders
	   list for each of these bogus non-files.

	   It's an open question as to whether relative expansion should
	   be allowed for news: and snews: URLs.

	   Reasons to allow it:

	     =  ClariNet has been using it

		 =  (Their reason:) it's the only way for an HTML news message
		    to refer to another HTML news message in a way which
			doesn't make assumptions about the name of the news host,
			and which also doesn't assume that the message is on the
			default news host.

	   Reasons to disallow it:

	     =  Consistency with "mailbox:"

		 =  If there is a news message which has type text/html, and
		    which has a relative URL like <IMG SRC="foo.gif"> in it,
			but which has no <BASE> tag, then we would expand that
			image to "news:foo.gif".  Which would fail, of course,
			but which might annoy news admins by generating bogus
			references.

	   So for now, let's allow 
	   NET_MakeAbsoluteURL("news:123@4", "456@7") => "news:456@7"; and
	   NET_MakeAbsoluteURL("news://h/123@4", "456@7") => "news://h/456@7".
	 */
	base_type = NET_URL_Type(absolute_url);
	if ((base_type == MAILBOX_TYPE_URL || base_type == IMAP_TYPE_URL || base_type == NEWS_TYPE_URL) &&
		*relative_url != '#' &&
		*relative_url != '?')
	  {
		return PL_strdup("");
	  }

	if(relative_url[0] == '/' && relative_url[1] == '/')
	  {
		/* a host absolute URL
		 */

		/* find the colon after the protocol
		 */
		cat_point = PL_strchr(absolute_url, ':');
		if (cat_point && base_type == WYSIWYG_TYPE_URL)
			cat_point = PL_strchr(cat_point + 1, ':');

		/* append after the colon
		 */
		if(cat_point)
		   cat_point++;

	  }
	else if(relative_url[0] == '/')
	  {
		/* a path absolute URL
         * append at the slash after the host part
		 */

		/* find the colon after the protocol 
         */
        char *colon = PL_strchr(absolute_url, ':');
		if (colon && base_type == WYSIWYG_TYPE_URL)
			colon = PL_strchr(colon + 1, ':');

		if(colon)
		  {
			if(colon[1] == '/' && colon[2] == '/')
			  {
				/* find the next slash 
				 */
				cat_point = PL_strchr(colon+3, '/');

				if(!cat_point)
				  {
				    /* if there isn't another slash then the cat point is the very end
				     */
					cat_point = &absolute_url[PL_strlen(absolute_url)];
				  }
			  }
			else
			  {
				/* no host was given so the cat_point is right after the colon
				 */
				cat_point = colon+1;
			  }

#if defined(XP_WIN) || defined(XP_OS2)
            /* this will allow drive letters to work right on windows 
             */
            if(XP_IS_ALPHA(*cat_point) && *(cat_point+1) == ':')
                cat_point += 2;
#endif /* XP_WIN */

		  }
	  }
	else if(relative_url[0] == '#')
	  {
		/* a positioning within the same document relative url
		 *
		 * add teh relative url to the full text of the absolute url minus
		 * any # punctuation the url might have
	 	 *
		 */
		char * hash = PL_strchr(absolute_url, '#');
	
		if(hash)
		  {
			char * ques_mark = PL_strchr(absolute_url, '?');
   
			if(ques_mark)
			  {
				/* this is a hack.
				 * copy things to try and make them more correct
				 */
				*hash = '\0';

				StrAllocCopy(ret_url, absolute_url);
				StrAllocCat(ret_url, relative_url);
				StrAllocCat(ret_url, ques_mark);

				*hash = '#';
				
				return(net_ReduceURL(ret_url));
			  }

			cat_point = hash;
		  }
		else
		  {
			cat_point = &absolute_url[PL_strlen(absolute_url)]; /* the end of the URL */
		  }
	  }
	else
	  {
		/* a completely relative URL
		 *
		 * append after the last slash
		 */
		char * ques = PL_strchr(absolute_url, '?');
		char * hash = PL_strchr(absolute_url, '#');

		if(ques)
			*ques = '\0';

		if(hash)
			*hash = '\0';

		cat_point = PL_strrchr(absolute_url, '/');

		/* if there are no slashes then append right after the colon after the protocol
		 */
		if(!cat_point)
		    cat_point = PL_strchr(absolute_url, ':');

		/* set the value back 
		 */
		if(ques)
			*ques = '?';

		if(hash)
			*hash = '#';

		if(cat_point)
			cat_point++;  /* append right after the slash or colon not on it */
	  }
	
    if(cat_point)
      {
		cat_point_char = *cat_point;  /* save the value */
        *cat_point = '\0';
        new_length = PL_strlen(absolute_url) + PL_strlen(relative_url) + 1;
        ret_url = (char *) PR_Malloc(new_length);
		if(!ret_url)
			return(NULL);  /* out of memory */

        PL_strcpy(ret_url, absolute_url);
        PL_strcat(ret_url, relative_url);
        *cat_point = cat_point_char;  /* set the absolute url back to its original state */
      } 
	else
	  {
		/* something went wrong.  just return a copy of the relative url
		 */
		StrAllocCopy(ret_url, relative_url);
	  }

	return(net_ReduceURL(ret_url));
}
		
/*
 *  Returns the number of the month given or -1 on error.
 */
MODULE_PRIVATE int NET_MonthNo (char * month)
{
    int ret;
    if (!PL_strncasecmp(month, "JAN", 3))
        ret = 0;
    else if (!PL_strncasecmp(month, "FEB", 3))
        ret = 1;
    else if (!PL_strncasecmp(month, "MAR", 3))
        ret = 2;
    else if (!PL_strncasecmp(month, "APR", 3))
        ret = 3;
    else if (!PL_strncasecmp(month, "MAY", 3))
        ret = 4;
    else if (!PL_strncasecmp(month, "JUN", 3))
        ret = 5;
    else if (!PL_strncasecmp(month, "JUL", 3))
        ret = 6;
    else if (!PL_strncasecmp(month, "AUG", 3))
        ret = 7;
    else if (!PL_strncasecmp(month, "SEP", 3))
        ret = 8;
    else if (!PL_strncasecmp(month, "OCT", 3))
        ret = 9;
    else if (!PL_strncasecmp(month, "NOV", 3))
        ret = 10;
    else if (!PL_strncasecmp(month, "DEC", 3))
        ret = 11;
    else 
	  {
        ret = -1;
        TRACEMSG(("net_convert_month. Couldn't resolve date.\n"));
      }
    return ret;
}


/* accepts an RFC 850 or RFC 822 date string and returns a
 * time_t
 */
MODULE_PRIVATE time_t 
NET_ParseDate(char *date_string)
{
#ifndef USE_OLD_TIME_FUNC
	time_t date;

    TRACEMSG(("Parsing date string: %s\n",date_string));

	/* try using XP_ParseTimeString instead
	 */
	date = XP_ParseTimeString(date_string, TRUE);

	if(date)
	  {
		TRACEMSG(("Parsed date as GMT: %s\n", asctime(gmtime(&date))));
		TRACEMSG(("Parsed date as local: %s\n", ctime(&date)));
	  }
	else
	  {
		TRACEMSG(("Could not parse date"));
	  }

	return(date);

#else
    struct tm time_info;         /* Points to static tm structure */
    char  *ip;
    char   mname[256];
    time_t rv;

    TRACEMSG(("Parsing date string: %s\n",date_string));

    memset(&time_info, 0, sizeof(struct tm));

    /* Whatever format we're looking at, it will start with weekday. */
    /* Skip to first space. */
    if(!(ip = PL_strchr(date_string,' ')))
        return 0;
    else
        while(XP_IS_SPACE(*ip))
            ++ip;

	/* make sure that the date is less than 256 
	 * That will keep mname from ever overflowing 
	 */
	if(255 < PL_strlen(ip))
		return(0);

    if(isalpha(*ip)) 
	  {
        /* ctime */
		sscanf(ip, (strstr(ip, "DST") ? "%s %d %d:%d:%d %*s %d"
					: "%s %d %d:%d:%d %d"),
			   mname,
			   &time_info.tm_mday,
			   &time_info.tm_hour,
			   &time_info.tm_min,
			   &time_info.tm_sec,
			   &time_info.tm_year);
		time_info.tm_year -= 1900;
      }
    else if(ip[2] == '-') 
	  {
        /* RFC 850 (normal HTTP) */
        char t[256];
        sscanf(ip,"%s %d:%d:%d", t,
								&time_info.tm_hour,
								&time_info.tm_min,
								&time_info.tm_sec);
        t[2] = '\0';
        time_info.tm_mday = atoi(t);
        t[6] = '\0';
        PL_strcpy(mname,&t[3]);
        time_info.tm_year = atoi(&t[7]);
        /* Prevent wraparound from ambiguity */
        if(time_info.tm_year < 70)
            time_info.tm_year += 100;
		else if(time_info.tm_year > 1900)
			time_info.tm_year -= 1900;
      }
    else 
      {
        /* RFC 822 */
        sscanf(ip,"%d %s %d %d:%d:%d",&time_info.tm_mday,
									mname,
									&time_info.tm_year,
									&time_info.tm_hour,
									&time_info.tm_min,
									&time_info.tm_sec);
	    /* since tm_year is years since 1900 and the year we parsed
 	     * is absolute, we need to subtract 1900 years from it
	     */
	    time_info.tm_year -= 1900;
      }
    time_info.tm_mon = NET_MonthNo(mname);

    if(time_info.tm_mon == -1) /* check for error */
		return(0);


	TRACEMSG(("Parsed date as: %s\n", asctime(&time_info)));

    rv = mktime(&time_info);
	
	if(time_info.tm_isdst)
		rv -= 3600;

	if(rv == -1)
	  {
		TRACEMSG(("mktime was unable to resolve date/time: %s\n", asctime(&time_info)));
        return(0);
	  }
	else
	  {
		TRACEMSG(("Parsed date %s\n", asctime(&time_info)));
		TRACEMSG(("\tas %s\n", ctime(&rv)));
        return(rv);
	  }
#endif
}

MODULE_PRIVATE char * 
NET_ParseURL (const char *url, int parts_requested)
{
	char *rv=0,*colon, *slash, *ques_mark, *hash_mark, *atSign, *host, *passwordColon;

	assert(url);

	if(!url)
		return(StrAllocCat(rv, ""));
	colon = PL_strchr(url, ':'); /* returns a const char */

	/* Get the protocol part, not including anything beyond the colon */
	if (parts_requested & GET_PROTOCOL_PART) {
		if(colon) {
			char val = *(colon+1);
			*(colon+1) = '\0';
			StrAllocCopy(rv, url);
			*(colon+1) = val;

			/* If the user wants more url info, tack on extra slashes. */
			if( (parts_requested & GET_HOST_PART)
				|| (parts_requested & GET_USERNAME_PART)
				|| (parts_requested & GET_PASSWORD_PART)
				)
			  {
				if( *(colon+1) == '/' && *(colon+2) == '/')
				   StrAllocCat(rv, "//");
				/* If there's a third slash consider it file:/// and tack on the last slash. */
				if( *(colon+3) == '/' )
					StrAllocCat(rv, "/");
			  }
		  }
	  }


	/* Get the username if one exists */
	if (parts_requested & GET_USERNAME_PART) {
		if (colon 
			&& (*(colon+1) == '/')
			&& (*(colon+2) == '/')
			&& (*(colon+3) != '\0')) {

			if ( (slash = PL_strchr(colon+3, '/')) != NULL)
				*slash = '\0';
			if ( (atSign = PL_strchr(colon+3, '@')) != NULL) {
				*atSign = '\0';
				if ( (passwordColon = PL_strchr(colon+3, ':')) != NULL)
					*passwordColon = '\0';
				StrAllocCat(rv, colon+3);

				/* Get the password if one exists */
				if (parts_requested & GET_PASSWORD_PART) {
					if (passwordColon) {
						StrAllocCat(rv, ":");
						StrAllocCat(rv, passwordColon+1);
					}
				}
				if (parts_requested & GET_HOST_PART)
					StrAllocCat(rv, "@");
				if (passwordColon)
					*passwordColon = ':';
				*atSign = '@';
			}
			if (slash)
				*slash = '/';
		}
	}

	/* Get the host part */
    if (parts_requested & GET_HOST_PART) {
        if(colon) {
			if(*(colon+1) == '/' && *(colon+2) == '/')
			  {
				slash = PL_strchr(colon+3, '/');

				if(slash)
					*slash = '\0';

				if( (atSign = PL_strchr(colon+3, '@')) != NULL)
					host = atSign+1;
				else
					host = colon+3;
				
				ques_mark = PL_strchr(host, '?');

				if(ques_mark)
					*ques_mark = '\0';

/*
 * Protect systems whose header files forgot to let the client know when
 * gethostbyname would trash their stack.
 */
#ifndef MAXHOSTNAMELEN
#ifdef XP_OS2
#error Managed to get into NET_ParseURL without defining MAXHOSTNAMELEN !!!
#endif
#endif
                /* limit hostnames to within MAXHOSTNAMELEN characters to keep
                 * from crashing
                 */
                if(PL_strlen(host) > MAXHOSTNAMELEN)
                  {
                    char * cp;
                    char old_char;

                    cp = host+MAXHOSTNAMELEN;
                    old_char = *cp;

                    *cp = '\0';

                    StrAllocCat(rv, host);

                    *cp = old_char;
                  }
				else
				  {
					StrAllocCat(rv, host);
				  }

				if(slash)
					*slash = '/';

				if(ques_mark)
					*ques_mark = '?';
			  }
          }
	  }
	
	/* Get the path part */
    if (parts_requested & GET_PATH_PART) {
        if(colon) {
            if(*(colon+1) == '/' && *(colon+2) == '/')
              {
				/* skip host part */
                slash = PL_strchr(colon+3, '/');
			  }
			else 
              {
				/* path is right after the colon
				 */
                slash = colon+1;
			  }
                
			if(slash)
			  {
			    ques_mark = PL_strchr(slash, '?');
			    hash_mark = PL_strchr(slash, '#');

			    if(ques_mark)
				    *ques_mark = '\0';
	
			    if(hash_mark)
				    *hash_mark = '\0';

                StrAllocCat(rv, slash);

			    if(ques_mark)
				    *ques_mark = '?';
	
			    if(hash_mark)
				    *hash_mark = '#';
			  }
		  }
      }
		
    if(parts_requested & GET_HASH_PART) {
		hash_mark = PL_strchr(url, '#'); /* returns a const char * */

		if(hash_mark)
		  {
			ques_mark = PL_strchr(hash_mark, '?');

			if(ques_mark)
				*ques_mark = '\0';

            StrAllocCat(rv, hash_mark);

			if(ques_mark)
				*ques_mark = '?';
		  }
	  }

    if(parts_requested & GET_SEARCH_PART) {
        ques_mark = PL_strchr(url, '?'); /* returns a const char * */

        if(ques_mark)
          {
            hash_mark = PL_strchr(ques_mark, '#');

            if(hash_mark)
                *hash_mark = '\0';

            StrAllocCat(rv, ques_mark);

            if(hash_mark)
                *hash_mark = '#';
          }
      }

	/* copy in a null string if nothing was copied in
	 */
	if(!rv)
	   StrAllocCopy(rv, "");
    
	/* XP_OS2_FIX IBM-MAS: long URLS blowout tracemsg buffer, 
		set max to 1900 chars */
    TRACEMSG(("mkparse: ParseURL: parsed - %-.1900s",rv));

    return rv;
}

/* DANGER DANGER DANGER DANGER DANGER DANGER DANGER DANGER DANGER DANGER
   DANGER DANGER DANGER DANGER DANGER DANGER DANGER DANGER DANGER DANGER
   DANGER DANGER DANGER DANGER DANGER DANGER DANGER DANGER DANGER DANGER

								   TOXIC!

	 Do not even think of changing IS_OK or anything anywhere near it.

	 There is much spooky voodoo involving signed and unsigned chars
	 when using gcc on SunOS 4.1.3.

	 If you think you understand it, the smart money says you're wrong.

								   TOXIC!

   DANGER DANGER DANGER DANGER DANGER DANGER DANGER DANGER DANGER DANGER
   DANGER DANGER DANGER DANGER DANGER DANGER DANGER DANGER DANGER DANGER
   DANGER DANGER DANGER DANGER DANGER DANGER DANGER DANGER DANGER DANGER
 */

/* encode illegal characters into % escaped hex codes.
 *
 * mallocs and returns a string that must be freed
 */
PRIVATE CONST 
int netCharType[256] =
/*	Bit 0		xalpha		-- the alphas
**	Bit 1		xpalpha		-- as xalpha but 
**                             converts spaces to plus and plus to %20
**	Bit 3 ...	path		-- as xalphas but doesn't escape '/'
*/
    /*   0 1 2 3 4 5 6 7 8 9 A B C D E F */
    {    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* 0x */
		 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* 1x */
		 0,0,0,0,0,0,0,0,0,0,7,4,0,7,7,4,	/* 2x   !"#$%&'()*+,-./	 */
         7,7,7,7,7,7,7,7,7,7,0,0,0,0,0,0,	/* 3x  0123456789:;<=>?	 */
	     7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,	/* 4x  @ABCDEFGHIJKLMNO  */
	     7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,7,	/* 5X  PQRSTUVWXYZ[\]^_	 */
	     0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,	/* 6x  `abcdefghijklmno	 */
	     7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,0,	/* 7X  pqrstuvwxyz{\}~	DEL */
		 0, };

#define IS_OK(C) (netCharType[((unsigned int) (C))] & (mask))

PUBLIC char *
NET_Escape (const char * str, int mask)
{
    if(!str)
        return NULL;
    return NET_EscapeBytes (str, (int32)PL_strlen(str), mask, NULL);
}

PUBLIC char *
NET_EscapeBytes (const char * str, int32 len, int mask, int32 * out_len)
{
    register CONST unsigned char *src;
    register unsigned char *dst;
    char        *result;
    int32       i, extra = 0;
    char        *hexChars = "0123456789ABCDEF";

	if(!str)
		return(0);

	src = (unsigned char *) str;
    for(i = 0; i < len; i++)
	  {
        if (!IS_OK(src[i]))
            extra+=2; /* the escape, plus an extra byte for each nibble */
	  }

    if(!(result = (char *) PR_Malloc(len + extra + 1)))
        return(0);

    dst = (unsigned char *) result;
    for(i = 0; i < len; i++)
	  {
		unsigned char c = src[i];
		if (IS_OK(c))
		  {
			*dst++ = c;
		  }
		else if(mask == URL_XPALPHAS && c == ' ')
		  {
			*dst++ = '+'; /* convert spaces to pluses */
		  }
		else 
		  {
			*dst++ = HEX_ESCAPE;
			*dst++ = hexChars[c >> 4];		/* high nibble */
			*dst++ = hexChars[c & 0x0f];	/* low nibble */
		  }
	  }

    *dst = '\0';     /* tack on eos */
	if(out_len)
		*out_len = dst - (unsigned char *) result;
    return result;
}

/* return the size of a string if it were to be escaped.
 */
MODULE_PRIVATE int32
NET_EscapedSize (const char * str, int mask)
{
	int32                         extra = 0;
	register CONST unsigned char * src;

	if(!str)
		return(0);

	for(src=((unsigned char *)str); *src; src++)
        if (!IS_OK(*src))
            extra+=2; /* the escape, plus an extra byte for each nibble */

	return((int32)(src - ((unsigned char *)str)) + extra + 1);
}


/* decode % escaped hex codes into character values
 */
#define UNHEX(C) \
    ((C >= '0' && C <= '9') ? C - '0' : \
     ((C >= 'A' && C <= 'F') ? C - 'A' + 10 : \
     ((C >= 'a' && C <= 'f') ? C - 'a' + 10 : 0)))

/* In-place rewrite of str, code copied from NET_UnEscapeCnt to avoid imposing
 * an PL_strlen and making that function call this one.
 */
PUBLIC int32
NET_UnEscapeBytes (char * str, int32 len)
{
	int32 i = 0;
    register char *dst = str;
	char c, d;

	while (i < len)
	  {
		c = str[i++];
        if (c == HEX_ESCAPE && i < len)
		  {
			d = str[i++];
			c = UNHEX(d);
			if (i < len)
			  {
				d = str[i++];
				c = (c << 4) | UNHEX(d);
			  }
		  }
		*dst++ = c;
	  }
    *dst = '\0';

    return (int32)(dst - str);
}

PUBLIC int
NET_UnEscapeCnt (char * str)
{
    register char *src = str;
    register char *dst = str;

    while(*src)
        if (*src != HEX_ESCAPE)
		  {
        	*dst++ = *src++;
		  }
        else 	
		  {
        	src++; /* walk over escape */
        	if (*src)
              {
            	*dst = UNHEX(*src) << 4;
            	src++;
              }
        	if (*src)
              {
            	*dst = (*dst + UNHEX(*src));
            	src++;
              }
        	dst++;
          }

    *dst = 0;

    return (int)(dst - str);

} /* NET_UnEscapeCnt */

/* Returns TRUE if URL type is HTTP_TYPE_URL or SECURE_HTTP_TYPE_URL */
PUBLIC XP_Bool
NET_IsHTTP_URL(const char *URL)
{
    int type = NET_URL_Type (URL);
    return ( type == HTTP_TYPE_URL || type == SECURE_HTTP_TYPE_URL );
}

/* Note: Should I move XP_ConvertUrlToLocalFile and XP_BackupFileName,
 *       (currently in winfe\fegui.cpp) to new file of EDT/XP stuff???
*/

/* Converts absolute URL into an URL relative to the base URL
 * BasePath must be an HTTP: or FILE: URL type
 * input_url should be absolute url of the same type,
 *  but we pass it through NET_MakeAbsoluteURL so we can use
 *  relative input - ASSUMES SAME TYPE!
 * Return values defined in \include\net.h
 * Caller must PR_Free the string
*/
int NET_MakeRelativeURL( char *base_url,
                         char *input_url,
                         char **relative_url )
{
    int Result = NET_URL_FAIL;
    char *base_host = NULL;
    char *url_host = NULL;
    char *base_path = NULL;
    char *url_path = NULL;
    char *base_ptr = NULL;
    char *url_ptr = NULL;
    int  base_type, url_type;
    char *relative = NULL;    
    char *absolute_url;
    int  new_length;
    int  subdir_count = 0;
    int  i;
    char *colon;
    char *slash;

    if( base_url == NULL || input_url == NULL ||
        PL_strlen(base_url) == 0 || 
        PL_strlen(input_url) == 0  ){
        return NET_URL_FAIL;
    }
    
    /* Convert to absolute URL */
    absolute_url = NET_MakeAbsoluteURL(base_url, input_url);
    
    if(absolute_url == NULL){
        return NET_URL_FAIL;
    }    

    /* We can only hande HTTP and FILE types,
    */
    /* Extended to handle FTP urls, for remote editing of ftp documents */
    base_type = NET_URL_Type (base_url);
    url_type = NET_URL_Type (absolute_url);
    if ( url_type == 0 ||
         ! ( base_type == FILE_TYPE_URL ||
             base_type == HTTP_TYPE_URL ||
             base_type == SECURE_HTTP_TYPE_URL ||
             base_type == FTP_TYPE_URL) ||
         ! ( url_type == FILE_TYPE_URL ||
             url_type == HTTP_TYPE_URL ||
             url_type == SECURE_HTTP_TYPE_URL ||
             url_type == FTP_TYPE_URL ) ) {
        goto CLEAN_UP;
    }


    /* If not both of same type,
     *  we return as if different device
    */
    if ( url_type != base_type ) {
        Result = NET_URL_NOT_ON_SAME_DEVICE;
        goto CLEAN_UP;
    }


#ifdef XP_WIN
    /* Check for UNC filenames.  E.g. file:////wongfoo/vol1/test.html  
     * (\\wongfoo\vol1\test.html).
     * This is the cheap and dirty hack so things don't completely break.
     * We just leave the url absolute if either base or absolute URL is a
     * UNC filename. 
     *
     * We should really do the following:
     *    1. If both URLs are UNC file URLs *AND* the machine name is the same, 
     *       make the URL relative, else NET_URL_NOT_ON_SAME_DEVICE.
     *
     *    2. If only one is a UNC filename, NET_URL_NOT_ON_SAME_DEVICE
     */
    if (url_type == FILE_TYPE_URL) {  /* already checked that types are the same. */
      /* Looking for "file:////" */
      if (PL_strlen(base_url) >= 9 &&
          !PL_strncmp(base_url + 5,"////",4)) {
        Result = NET_URL_NOT_ON_SAME_DEVICE;
        goto CLEAN_UP;
      }  
      if (PL_strlen(absolute_url) >= 9 &&
          !PL_strncmp(absolute_url + 5,"////",4)) {
        Result = NET_URL_NOT_ON_SAME_DEVICE;
        goto CLEAN_UP;
      }  
    }
#endif


    /*  Different hosts are also = different devices */
    base_host = NET_ParseURL(base_url, GET_HOST_PART);
    url_host = NET_ParseURL(absolute_url, GET_HOST_PART);
    if (PL_strcmp(base_host,url_host)){
        Result = NET_URL_NOT_ON_SAME_DEVICE;
        goto CLEAN_UP;
    }

    /* Similar to NET_ParseURL(GET_PATH_PART),
     *  but we don't strip named anchors and search strings
     */
    colon = (char*) PL_strchr(base_url, ':'); /* returns a const char * */

    if(colon) {
        if(*(colon+1) == '/' && *(colon+2) == '/') {
			/* skip host part */
            slash = PL_strchr(colon+3, '/');
        } else {
		    /* path is right after the colon */
            slash = colon+1;
        }
        if ( slash ) {
            base_path = PL_strdup(slash);
        }
	}
    if ( base_path == NULL ){
        goto CLEAN_UP;
    }

    colon = (char*) PL_strchr(absolute_url, ':');

    /* For url_path, find beginning of path in our 
     * already-allocated "absolute_url" string
     * Don't free url_path!
    */
    if(colon) {
        if(*(colon+1) == '/' && *(colon+2) == '/') {
            url_path = PL_strchr(colon+3, '/');
        } else {
            url_path = colon+1;
        }
    }
    if ( url_path == NULL ){
        goto CLEAN_UP;
    }

    /* TODO: MODIFY FOR NETWORK FILENAMES: //netname//blah */

    /* Find location of drive separator */
    base_ptr = (char*) PL_strchr(base_path, '|');
    url_ptr = (char*) PL_strchr(url_path, '|');

    /* Test if drive is different */
    if ( base_ptr && url_ptr &&
#ifdef XP_UNIX
        /* Case-sensitive for UNIX */
        *(url_ptr-1) != *(base_ptr-1) )
#else
        NET_TO_LOWER(*(url_ptr-1)) != NET_TO_LOWER(*(base_ptr-1)) )
#endif
    {
        Result = NET_URL_NOT_ON_SAME_DEVICE;
        goto CLEAN_UP;
    }
    
    if (base_ptr){
        base_ptr++; /* Move to first "/" after drive separator */
    } else {
        /* No '|', so start at begining of path */
        base_ptr = base_path;
    }

    if(url_ptr){
        url_ptr++;
    }else{
        /* We'll just work with copied string here */
        url_ptr = url_path;
    }

    /*Find first mismatched character */
    while ( 
#ifdef XP_UNIX
            /* Case-sensitive for UNIX */
            *base_ptr == *url_ptr && 
#else
            NET_TO_LOWER(*base_ptr) == NET_TO_LOWER(*url_ptr) &&
#endif
            *base_ptr != '\0' && *url_ptr != '\0' ){
        base_ptr++;
        url_ptr++;
    }
    
    /* If we next char is "#" and no base is empty, 
     *   then we have a target in the same document 
     *   We are done.
     */
    if( *url_ptr == '#' && *base_ptr == '\0' ){
        Result = NET_URL_SAME_DIRECTORY;
    } else {
        /* Backup to just after previous slash */
        while ( *(url_ptr-1) != '/' && url_ptr != url_path){ url_ptr--; }

        /* If we have more left on the base:
         *   either the base filename or more subdirectories,
         *   so count slashes to get number of subdirectories
         */
        while ( *base_ptr != '\0' ){
            if (*base_ptr == '/' ){
                subdir_count++;
            }
            base_ptr++;
        }

        /* We are in the same directory if there are no slashes
         *  in the final relative path
         */
        if ( subdir_count == 0 && NULL == PL_strchr(url_ptr, '/') ){
            Result = NET_URL_SAME_DIRECTORY;
        } else {
            Result = NET_URL_SAME_DEVICE;
        }
    }

    /* Don't supply relative URL string - just compare result */
    if ( ! relative_url ){
        goto CLEAN_UP;
    }

    /* We can now figure out length of relative URL */
    new_length = PL_strlen(url_ptr) + (subdir_count*3) + 1;
    relative = (char *) PR_Malloc(new_length);
    if ( !relative ){
        /* out of memory */
        Result = NET_URL_FAIL;
        goto CLEAN_UP;
    }

    *relative = '\0';

    /* Add "../" for each subdir left in the base */
    for ( i = 0; i < subdir_count; i++ ){
        PL_strcat(relative, "../");
    }

    /* Append the relative path that doesn't match */
    PL_strcat(relative, url_ptr);

CLEAN_UP:
    if ( relative_url ){
        if ( relative ){
            /* We allocced a relative URL - return it */
            *relative_url = relative;
        } else {
            /* For all other cases, return absolute URL */
            *relative_url = PL_strdup(absolute_url);
        }
    }
    PR_Free(absolute_url);

    if( base_path ){
        PR_Free(base_path);
    }
    if( base_host ){
        PR_Free(base_host);
    }
    if( url_host ){
        PR_Free(url_host);
    }
    return Result;
}

/* Extract the filename from a source URL 
 *  and construct add it the same path as 
 *  a base URL (replacing file in base)
 * BasePath should be an HTTP: or FILE: URL type,
 *  but we don't check it.
 * src_url may be a relative URL
 * If a pointer to a string is supplied,
 *  the new relative URL is returned
 *  (just the "file" portion of src_url)
 * Note: relative_url can be &src_url,
 *  thus replacing the source
 * Caller must PR_Free the string(s)
*/
char * NET_MakeTargetURL( char *base_url,
                          char *src_url,
                          char **relative_url )
{
    char *target = NULL;
    char *targ_ptr;
    char *src_ptr;
    char *relative_start;
    char *base_end;
    char *src_start;
    char *src_end = NULL;
    char *question;
    int   base_length;
    int   src_length;
    int   i;

    if( base_url == NULL || src_url == NULL ||
        PL_strlen(base_url) == 0 || 
        PL_strlen(src_url) == 0  ){
        return NULL;
    }

    /* Find where to concatenate the source
     *  we must have a slash or our base isn't valid!
    */
    base_end = PL_strrchr(base_url, '/');
    if ( !base_end ){
        return NULL;
    } 
    base_end++;


    /* Find start of source (usually filename) right after the last slash */
    src_start = PL_strrchr(src_url, '/');
    if (src_start){
        src_start++;
    } else {
        /* No slash, start at beginning */
        src_start = src_url;
    }

    /* Find end at neareast '#' or '?'  */
    src_end = PL_strchr(src_start, '#');
    question = PL_strchr(src_start, '?');

    if ( question &&
        ( !src_end || question < src_end ) ){
        src_end = question;
    }

    /* Calculate length of segments we will copy */
    if ( src_end ){
        src_length = src_end - src_start;
    } else {
        src_length = PL_strlen(src_start);
    }
    base_length = base_end - base_url;

    target = (char *) PR_Malloc(base_length + src_length + 1);
    if ( target ){
        targ_ptr = target;
        src_ptr = base_url;

        /* Copy the base until we reach file part */
        for ( i = 0; i < base_length; i++ ){
            *targ_ptr++ = *src_ptr++;
        }

        /* Save the start of the new relative URL */
        relative_start = targ_ptr;

        /* Concatenate the source file */
        for ( i = 0; i < src_length; i++ ){
            *targ_ptr++ = *src_start++;
        }
        *targ_ptr = '\0';

        /* Supply the new relative URL if requested */
        if ( relative_url ) {
            /* Free existing string */
            if ( *relative_url ){
                PR_Free(*relative_url);
            }
            *relative_url = PL_strdup(relative_start);
        }
    }

    return target;
}    


PUBLIC char *
NET_UnEscape(char * str)
{
	(void)NET_UnEscapeCnt(str);

	return str;
}

/*******************************************************
 * Routines to parse the application/http-index-format
 */
#define MAX_200_FIELDS 50
#define ARRAY_GROWTH_RATE 50

#define UNKNOWN_TOKEN			0
#define FILENAME_TOKEN			1
#define CONTENT_LENGTH_TOKEN	2
#define LAST_MODIFIED_TOKEN		3
#define CONTENT_TYPE_TOKEN		4
#define FILE_TYPE_TOKEN			5
#define PERMISSIONS_TOKEN		6

struct _HTTPIndexParserData {
	NET_FileEntryInfo   ** data_list;
	int32				  data_list_length;
	int32                 number_of_files;
	char          		* line_buffer;
	int32          		  line_buffer_size;
	char		  		* text_user_message;
	char		  		* html_user_message;
	char 				* base_url;
	int			    	  mapping_array[MAX_200_FIELDS];
};

PUBLIC HTTPIndexParserData *
NET_HTTPIndexParserInit()
{
	/* create and init parser data */
	HTTPIndexParserData * rv = PR_NEW(HTTPIndexParserData);

	if(!rv)
		return(NULL);

	memset(rv, 0, sizeof(HTTPIndexParserData));
	
	return(rv);
}

PUBLIC char *
strchr_in_buf(char *string, int32 string_len, char find_char)
{
	char *cp;
	char *end = string+string_len;

	for(cp = string; cp < end; cp++)
		if(*cp == find_char)
			return(cp);

	return(NULL);
}

/* return the next space or quote separated token */
PRIVATE char *
net_get_next_http_index_token(char * line)
{
	static char *next_token_start=NULL;
	char *start, *end;
	
	if(line == NULL)
		start = next_token_start;
	else
		start = line;

	if(start == NULL)
		return(NULL);

	while(XP_IS_SPACE(*start)) start++;

	if(*start == '"')
	  {
		/* end of token is next quote */
		start++;

		end = start;

		while(*end != '\0' && *end != '"') end++;
	  }
	else
	  {
		/* end of token is next space */
		end = start;

		while(*end != '\0' && *end != ' ') end++;
	  }

	if(*end == '\0')
		next_token_start = end;
	else
		next_token_start = end+1;
	*end = '\0';

	if(*start == '\0')
		return(NULL);
	else
		return(start);
}

PRIVATE int
net_parse_http_index_200_line(HTTPIndexParserData *obj, char *data_line)
{
	int32 position=0;
    char *token = net_get_next_http_index_token(data_line);

	while(token)
	  {
		if(!PL_strcasecmp("FILENAME", token))
		  {
			obj->mapping_array[position] = FILENAME_TOKEN;
		  }
		else if(!PL_strcasecmp("CONTENT-LENGTH", token))
		  {
			obj->mapping_array[position] = CONTENT_LENGTH_TOKEN;
		  }
		else if(!PL_strcasecmp("LAST-MODIFIED", token))
		  {
			obj->mapping_array[position] = LAST_MODIFIED_TOKEN;
		  }
		else if(!PL_strcasecmp("CONTENT-TYPE", token))
		  {
			obj->mapping_array[position] = CONTENT_TYPE_TOKEN;
		  }
		else if(!PL_strcasecmp("FILE-TYPE", token))
		  {
			obj->mapping_array[position] = FILE_TYPE_TOKEN;
		  }
		else if(!PL_strcasecmp("PERMISSIONS", token))
		  {
			obj->mapping_array[position] = PERMISSIONS_TOKEN;
		  }
		else
		  {
			obj->mapping_array[position] = UNKNOWN_TOKEN;
		  }

		token = net_get_next_http_index_token(NULL);
		position++;
		if(position > MAX_200_FIELDS)
			break;
	  }

	return(0);
}

PRIVATE int
net_parse_http_index_201_line(HTTPIndexParserData *obj, char *data_line)
{
	int32 position=0;
    char *token = net_get_next_http_index_token(data_line);
	NET_FileEntryInfo *file_struct;

	/* make sure there is enough space for the file
	 * entry in the array
	 */
	if(obj->data_list_length < obj->number_of_files+1)
	  {
		/* malloc some more space for the array
		 */
		if(obj->data_list)
			obj->data_list = (NET_FileEntryInfo **) PR_Realloc(obj->data_list, 
													(obj->data_list_length 
													+ ARRAY_GROWTH_RATE)
													* sizeof(NET_FileEntryInfo*));
		else
			obj->data_list = (NET_FileEntryInfo **) PR_Malloc(ARRAY_GROWTH_RATE 
													* sizeof(NET_FileEntryInfo*));
		obj->data_list_length += ARRAY_GROWTH_RATE;
	  }

	if(!obj->data_list)
		return(MK_OUT_OF_MEMORY);

	file_struct = NET_CreateFileEntryInfoStruct();
	if(!file_struct)
		return(MK_OUT_OF_MEMORY);

    while(token)
      {
		NET_UnEscape(token);
		switch(obj->mapping_array[position])
		  {
			case UNKNOWN_TOKEN:
				/* ignore unknown token types */
				break;

			case FILENAME_TOKEN:
				file_struct->filename = PL_strdup(token);
				break;

			case CONTENT_LENGTH_TOKEN:
				file_struct->size = atoi(token);
				break;

			case LAST_MODIFIED_TOKEN:
				file_struct->date = XP_ParseTimeString(token, TRUE);
				break;

			case CONTENT_TYPE_TOKEN:
				file_struct->content_type = PL_strdup(token);
				break;

			case FILE_TYPE_TOKEN:
				if(!PL_strcasecmp(token, "FILE"))
				  {
					file_struct->special_type = NET_FILE_TYPE;
				  }
				else if(!PL_strcasecmp(token, "DIRECTORY"))
				  {
					file_struct->special_type = NET_DIRECTORY;
				  }
				else if(!PL_strcasecmp(token, "SYMBOLIC-LINK"))
				  {
					file_struct->special_type = NET_SYM_LINK;
				  }
				else if(!PL_strcasecmp(token, "SYM-FILE"))
				  {
					file_struct->special_type = NET_SYM_LINK_TO_FILE;
				  }
				else if(!PL_strcasecmp(token, "SYM-DIRECTORY"))
				  {
					file_struct->special_type = NET_SYM_LINK_TO_DIR;
				  }
				break;

			case PERMISSIONS_TOKEN:
				if(PL_strlen(token) == 3)
				  {
					file_struct->permissions = 0;
					if(token[0] == 'R')
						file_struct->permissions |= NET_READ_PERMISSION;
					if(token[1] == 'W')
						file_struct->permissions |= NET_WRITE_PERMISSION;
					if(token[2] == 'E')
						file_struct->permissions |= NET_EXECUTE_PERMISSION;
				  }
				break;

			default:
				PR_ASSERT(0);
				break;
		  }
		

        token = net_get_next_http_index_token(NULL);
		position++;
		if(position > MAX_200_FIELDS)
			break;
      }

	/* every struct must have a filename.  If it doesn't
	 * destroy it
	 */	
	if(!file_struct->filename || !*file_struct->filename)
		NET_FreeEntryInfoStruct(file_struct);
	else
		obj->data_list[obj->number_of_files++] = file_struct;

	return(0);
}


PUBLIC int
NET_HTTPIndexParserPut(HTTPIndexParserData *obj, char *data, int32 len)
{
	char *data_line, *new_line, *colon;
	int32 string_len;
	int32 line_code, status;

	if(!obj)
		return(MK_OUT_OF_MEMORY);

	/* buffer until we have a line */
	BlockAllocCat(obj->line_buffer, obj->line_buffer_size, data, len);
	obj->line_buffer_size += len;
	
	/* see if we have a line */
	while(((new_line = strchr_in_buf(obj->line_buffer, 
									obj->line_buffer_size, 
									CR)) != NULL)
		  || ((new_line = strchr_in_buf(obj->line_buffer, 
									obj->line_buffer_size, 
									LF)) != NULL) )
	  {
		if(new_line[0] == CR && new_line[1] == LF)
		{
			*new_line = '\0'; /* get rid of the LF too */
			new_line++;
		}
			
		/* terminate the line */
		*new_line = '\0';

		data_line = XP_StripLine(obj->line_buffer);

		/* get the line code */
		colon = PL_strchr(data_line, ':');

		if(!colon)
			goto end_of_line;

		*colon = '\0';

		line_code = atoi(XP_StripLine(data_line));
		
		/* reset data_line to the start of data after the colon */
		data_line = colon+1;
		
		status = 0;

		switch(line_code)
		  {
			case 100:
				/* comment line, ignore */
				break;

			case 101:
				StrAllocCat(obj->text_user_message, data_line);
				StrAllocCat(obj->text_user_message, "\n");
				break;

			case 102:
				StrAllocCat(obj->html_user_message, data_line);
				StrAllocCat(obj->html_user_message, "\n");
				break;

			case 200:
				status = net_parse_http_index_200_line(obj,	data_line);
				break;

			case 201:
				status = net_parse_http_index_201_line(obj,	data_line);
				break;

			case 300:
				StrAllocCat(obj->base_url, data_line);
				break;

			default:
				/* fall through and ignore the line */
				break;
		  }

		if(status < 0)
			return(status);

end_of_line:
		/* remove the parsed line from obj->line_buffer */
		string_len = (new_line - obj->line_buffer) + 1;
		memcpy(obj->line_buffer, 
				  new_line+1, 
				  obj->line_buffer_size-string_len);
		obj->line_buffer_size -= string_len;

	  }
		
	return(0);
}

PUBLIC int
NET_HTTPIndexParserSort(HTTPIndexParserData *data_obj, int sort_method)
{
/* @@@@@@ soon */
	return(0);
}

PUBLIC char *
NET_HTTPIndexParserGetTextMessage(HTTPIndexParserData *data_obj)
{
	return(data_obj->text_user_message);
}

PUBLIC char *
NET_HTTPIndexParserGetHTMLMessage(HTTPIndexParserData *data_obj)
{
	return(data_obj->html_user_message);
}

PUBLIC char *
NET_HTTPIndexParserGetBaseURL(HTTPIndexParserData *data_obj)
{
	return(data_obj->base_url);
}

PUBLIC NET_FileEntryInfo *
NET_HTTPIndexParserGetFileNum(HTTPIndexParserData *data_obj, int32 num)
{
	if(num >= data_obj->number_of_files || num < 0)
		return(NULL);

	return(data_obj->data_list[num]);
}

PUBLIC int32
NET_HTTPIndexParserGetTotalFiles(HTTPIndexParserData *data_obj)
{
	return(data_obj->number_of_files);
}

PUBLIC void
NET_HTTPIndexParserFree(HTTPIndexParserData *obj)
{
	int32 i;

	if(!obj)
		return;

	/* FREE file data structures and array */
	for(i=0; i<obj->number_of_files; i++)
		NET_FreeEntryInfoStruct(obj->data_list[i]);

	FREEIF(obj->data_list);  /* array */
	FREEIF(obj->line_buffer);
	FREEIF(obj->text_user_message);
	FREEIF(obj->html_user_message);
	FREE(obj);
}


#ifdef PROFILE
#pragma profile off
#endif
