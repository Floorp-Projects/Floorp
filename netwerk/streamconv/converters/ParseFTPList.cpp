/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "plstr.h"
#include "nsDebug.h"

#include "ParseFTPList.h"

/* ==================================================================== */

static inline int ParsingFailed(struct list_state *state)
{
  if (state->parsed_one || state->lstyle) /* junk if we fail to parse */
    return '?';      /* this time but had previously parsed successfully */
  return '"';        /* its part of a comment or error message */
}

int ParseFTPList(const char *line, struct list_state *state,
                 struct list_result *result )
{
  unsigned int carry_buf_len; /* copy of state->carry_buf_len */
  unsigned int linelen, pos;
  const char *p;

  if (!line || !state || !result)
    return 0;

  memset( result, 0, sizeof(*result) );
  state->numlines++;

  /* carry buffer is only valid from one line to the next */
  carry_buf_len = state->carry_buf_len;
  state->carry_buf_len = 0;

  linelen = 0;

  /* strip leading whitespace */
  while (*line == ' ' || *line == '\t')
    line++;
    
  /* line is terminated at first '\0' or '\n' */
  p = line;
  while (*p && *p != '\n')
    p++;
  linelen = p - line;

  if (linelen > 0 && *p == '\n' && *(p-1) == '\r')
    linelen--;

  /* DON'T strip trailing whitespace. */

  if (linelen > 0)
  {
    static const char *month_names = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *tokens[16]; /* 16 is more than enough */
    unsigned int toklen[(sizeof(tokens)/sizeof(tokens[0]))];
    unsigned int linelen_sans_wsp;  // line length sans whitespace
    unsigned int numtoks = 0;
    unsigned int tokmarker = 0; /* extra info for lstyle handler */
    unsigned int month_num = 0;
    char tbuf[4];
    int lstyle = 0;

    if (carry_buf_len) /* VMS long filename carryover buffer */
    {
      tokens[0] = state->carry_buf;
      toklen[0] = carry_buf_len;
      numtoks++;
    }

    pos = 0;
    while (pos < linelen && numtoks < (sizeof(tokens)/sizeof(tokens[0])) )
    {
      while (pos < linelen && 
            (line[pos] == ' ' || line[pos] == '\t' || line[pos] == '\r'))
        pos++;
      if (pos < linelen)
      {
        tokens[numtoks] = &line[pos];
        while (pos < linelen && 
           (line[pos] != ' ' && line[pos] != '\t' && line[pos] != '\r'))
          pos++;
        if (tokens[numtoks] != &line[pos])
        {
          toklen[numtoks] = (&line[pos] - tokens[numtoks]);
          numtoks++;  
        }
      }
    }    

    if (!numtoks)
      return ParsingFailed(state);

    linelen_sans_wsp = &(tokens[numtoks-1][toklen[numtoks-1]]) - tokens[0];
    if (numtoks == (sizeof(tokens)/sizeof(tokens[0])) )
    {
      pos = linelen;
      while (pos > 0 && (line[pos-1] == ' ' || line[pos-1] == '\t'))
        pos--;
      linelen_sans_wsp = pos;
    }

    /* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

#if defined(SUPPORT_EPLF)
    /* EPLF handling must come somewhere before /bin/dls handling. */
    if (!lstyle && (!state->lstyle || state->lstyle == 'E'))
    {
      if (*line == '+' && linelen > 4 && numtoks >= 2)
      {
        pos = 1;
        while (pos < (linelen-1))
        {
          p = &line[pos++];
          if (*p == '/') 
            result->fe_type = 'd'; /* its a dir */
          else if (*p == 'r')
            result->fe_type = 'f'; /* its a file */
          else if (*p == 'm')
          {
            if (isdigit(line[pos]))
            {
              while (pos < linelen && isdigit(line[pos]))
                pos++;
              if (pos < linelen && line[pos] == ',')
              {
                PRTime t;
                PRTime seconds;
                PR_sscanf(p+1, "%llu", &seconds);
                t = seconds * PR_USEC_PER_SEC;
                PR_ExplodeTime(t, PR_LocalTimeParameters, &(result->fe_time) );
              }
            }
          }
          else if (*p == 's')
          {
            if (isdigit(line[pos]))
            {
              while (pos < linelen && isdigit(line[pos]))
                pos++;
              if (pos < linelen && line[pos] == ',' &&
                 ((&line[pos]) - (p+1)) < int(sizeof(result->fe_size)-1) )
              {
                memcpy( result->fe_size, p+1, (unsigned)(&line[pos] - (p+1)) );
                result->fe_size[(&line[pos] - (p+1))] = '\0';
              }
            }
          }
          else if (isalpha(*p)) /* 'i'/'up' or unknown "fact" (property) */
          {
            while (pos < linelen && *++p != ',')
              pos++;
          }
          else if (*p != '\t' || (p+1) != tokens[1])
          {
            break; /* its not EPLF after all */
          }
          else
          {
            state->parsed_one = 1;
            state->lstyle = lstyle = 'E';

            p = &(line[linelen_sans_wsp]);
            result->fe_fname = tokens[1];
            result->fe_fnlen = p - tokens[1];

            if (!result->fe_type) /* access denied */
            {
              result->fe_type = 'f'; /* is assuming 'f'ile correct? */
              return '?';            /* NO! junk it. */
            }
            return result->fe_type;
          }
          if (pos >= (linelen-1) || line[pos] != ',')
            break;
          pos++;
        } /* while (pos < linelen) */
        memset( result, 0, sizeof(*result) );
      } /* if (*line == '+' && linelen > 4 && numtoks >= 2) */
    } /* if (!lstyle && (!state->lstyle || state->lstyle == 'E')) */
#endif /* SUPPORT_EPLF */

    /* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

#if defined(SUPPORT_VMS)
    if (!lstyle && (!state->lstyle || state->lstyle == 'V'))
    {                          /* try VMS Multinet/UCX/CMS server */
      /*
       * Legal characters in a VMS file/dir spec are [A-Z0-9$.-_~].
       * '$' cannot begin a filename and `-' cannot be used as the first 
       * or last character. '.' is only valid as a directory separator 
       * and <file>.<type> separator. A canonical filename spec might look 
       * like this: DISK$VOL:[DIR1.DIR2.DIR3]FILE.TYPE;123
       * All VMS FTP servers LIST in uppercase.
       *
       * We need to be picky about this in order to support
       * multi-line listings correctly.
      */
      if (!state->parsed_one &&
          (numtoks == 1 || (numtoks == 2 && toklen[0] == 9 &&
                            memcmp(tokens[0], "Directory", 9)==0 )))
      {
        /* If no dirstyle has been detected yet, and this line is a 
         * VMS list's dirname, then turn on VMS dirstyle.
         * eg "ACA:[ANONYMOUS]", "DISK$FTP:[ANONYMOUS]", "SYS$ANONFTP:" 
        */
        p = tokens[0];
        pos = toklen[0];
        if (numtoks == 2)
        {
          p = tokens[1];
          pos = toklen[1];
        }
        pos--;
        if (pos >= 3)
        {
          while (pos > 0 && p[pos] != '[')
          {
            pos--;
            if (p[pos] == '-' || p[pos] == '$')
            {
              if (pos == 0 || p[pos-1] == '[' || p[pos-1] == '.' ||
                  (p[pos] == '-' && (p[pos+1] == ']' || p[pos+1] == '.')))
                break;
            }
            else if (p[pos] != '.' && p[pos] != '~' && 
                     !isdigit(p[pos]) && !isalpha(p[pos]))
              break;
            else if (isalpha(p[pos]) && p[pos] != toupper(p[pos]))
              break;
          }
          if (pos > 0)
          {
            pos--;
            if (p[pos] != ':' || p[pos+1] != '[')
              pos = 0;
          }
        }
        if (pos > 0 && p[pos] == ':')
        {
          while (pos > 0)
          {
            pos--;
            if (p[pos] != '$' && p[pos] != '_' && p[pos] != '-' &&
                p[pos] != '~' && !isdigit(p[pos]) && !isalpha(p[pos]))
              break;
            else if (isalpha(p[pos]) && p[pos] != toupper(p[pos]))
              break;
          }
          if (pos == 0)
          {  
            state->lstyle = 'V';
            return '?'; /* its junk */
          }
        }
        /* fallthrough */ 
      }
      else if ((tokens[0][toklen[0]-1]) != ';')
      {
        if (numtoks == 1 && (state->lstyle == 'V' && !carry_buf_len))
          lstyle = 'V';
        else if (numtoks < 4)
          ;
        else if (toklen[1] >= 10 && memcmp(tokens[1], "%RMS-E-PRV", 10) == 0)
          lstyle = 'V';
        else if ((&line[linelen] - tokens[1]) >= 22 &&
                  memcmp(tokens[1], "insufficient privilege", 22) == 0)
          lstyle = 'V';
        else if (numtoks != 4 && numtoks != 6)
          ;
        else if (numtoks == 6 && (
                 toklen[5] < 4 || *tokens[5] != '(' ||        /* perms */
                           (tokens[5][toklen[5]-1]) != ')'  ))
          ;
        else if (  (toklen[2] == 10 || toklen[2] == 11) &&      
                        (tokens[2][toklen[2]-5]) == '-' &&
                        (tokens[2][toklen[2]-9]) == '-' &&
        (((toklen[3]==4 || toklen[3]==5 || toklen[3]==7 || toklen[3]==8) &&
                        (tokens[3][toklen[3]-3]) == ':' ) ||
         ((toklen[3]==10 || toklen[3]==11 ) &&
                        (tokens[3][toklen[3]-3]) == '.' )
        ) &&  /* time in [H]H:MM[:SS[.CC]] format */
                                    isdigit(*tokens[1]) && /* size */
                                    isdigit(*tokens[2]) && /* date */
                                    isdigit(*tokens[3])    /* time */
                )
        {
          lstyle = 'V';
        }
        if (lstyle == 'V')
        {
          /* 
          * MultiNet FTP:
          *   LOGIN.COM;2                 1   4-NOV-1994 04:09 [ANONYMOUS] (RWE,RWE,,)
          *   PUB.DIR;1                   1  27-JAN-1994 14:46 [ANONYMOUS] (RWE,RWE,RE,RWE)
          *   README.FTP;1        %RMS-E-PRV, insufficient privilege or file protection violation
          *   ROUSSOS.DIR;1               1  27-JAN-1994 14:48 [CS,ROUSSOS] (RWE,RWE,RE,R)
          *   S67-50903.JPG;1           328  22-SEP-1998 16:19 [ANONYMOUS] (RWED,RWED,,)
          * UCX FTP: 
          *   CII-MANUAL.TEX;1  213/216  29-JAN-1996 03:33:12  [ANONYMOU,ANONYMOUS] (RWED,RWED,,)
          * CMU/VMS-IP FTP
          *   [VMSSERV.FILES]ALARM.DIR;1 1/3 5-MAR-1993 18:09
          * TCPware FTP
          *   FOO.BAR;1 4 5-MAR-1993 18:09:01.12
          * Long filename example:
          *   THIS-IS-A-LONG-VMS-FILENAME.AND-THIS-IS-A-LONG-VMS-FILETYPE\r\n
          *                    213[/nnn]  29-JAN-1996 03:33[:nn]  [ANONYMOU,ANONYMOUS] (RWED,RWED,,)
          */
          tokmarker = 0;
          p = tokens[0];
          pos = 0;
          if (*p == '[' && toklen[0] >= 4) /* CMU style */
          {
            if (p[1] != ']') 
            {
              p++;
              pos++;
            }
            while (lstyle && pos < toklen[0] && *p != ']')
            {
              if (*p != '$' && *p != '.' && *p != '_' && *p != '-' &&
                  *p != '~' && !isdigit(*p) && !isalpha(*p))              
                lstyle = 0;
              pos++;
              p++;
            }
            if (lstyle && pos < (toklen[0]-1))
            {
              /* ']' was found and there is at least one character after it */
              NS_ASSERTION(*p == ']', "unexpected state");
              pos++;
              p++;
              tokmarker = pos; /* length of leading "[DIR1.DIR2.etc]" */
            } else {
              /* not a CMU style listing */
              lstyle = 0;
            }
          }
          while (lstyle && pos < toklen[0] && *p != ';')
          {
            if (*p != '$' && *p != '.' && *p != '_' && *p != '-' &&
                *p != '~' && !isdigit(*p) && !isalpha(*p))
              lstyle = 0;
            else if (isalpha(*p) && *p != toupper(*p))
              lstyle = 0;
            p++;
            pos++;
          }
          if (lstyle && *p == ';')
          {
            if (pos == 0 || pos == (toklen[0]-1))
              lstyle = 0;
            for (pos++;lstyle && pos < toklen[0];pos++)
            {
              if (!isdigit(tokens[0][pos]))
                lstyle = 0;
            }
          }
          pos = (p - tokens[0]); /* => fnlength sans ";####" */
          pos -= tokmarker;      /* => fnlength sans "[DIR1.DIR2.etc]" */
          p = &(tokens[0][tokmarker]); /* offset of basename */

          if (!lstyle || pos == 0 || pos > 80) /* VMS filenames can't be longer than that */
          {
            lstyle = 0;
          }
          else if (numtoks == 1)
          { 
            /* if VMS has been detected and there is only one token and that 
             * token was a VMS filename then this is a multiline VMS LIST entry.
            */
            if (pos >= (sizeof(state->carry_buf)-1))
              pos = (sizeof(state->carry_buf)-1); /* shouldn't happen */
            memcpy( state->carry_buf, p, pos );
            state->carry_buf_len = pos;
            return '?'; /* tell caller to treat as junk */
          }
          else if (isdigit(*tokens[1])) /* not no-privs message */
          {
            for (pos = 0; lstyle && pos < (toklen[1]); pos++)
            {
              if (!isdigit((tokens[1][pos])) && (tokens[1][pos]) != '/')
                lstyle = 0;
            }
            if (lstyle && numtoks > 4) /* Multinet or UCX but not CMU */
            {
              for (pos = 1; lstyle && pos < (toklen[5]-1); pos++)
              {
                p = &(tokens[5][pos]);
                if (*p!='R' && *p!='W' && *p!='E' && *p!='D' && *p!=',')
                  lstyle = 0;
              }
            }
          }
        } /* passed initial tests */
      } /* else if ((tokens[0][toklen[0]-1]) != ';') */    

      if (lstyle == 'V')
      {
        state->parsed_one = 1;
        state->lstyle = lstyle;

        if (isdigit(*tokens[1]))  /* not permission denied etc */
        {
          /* strip leading directory name */
          if (*tokens[0] == '[') /* CMU server */
          {
            pos = toklen[0]-1;
            p = tokens[0]+1;
            while (*p != ']')
            {
              p++;
              pos--;
            }
            toklen[0] = --pos;
            tokens[0] = ++p;
          }
          pos = 0;
          while (pos < toklen[0] && (tokens[0][pos]) != ';')
            pos++;
       
          result->fe_cinfs = 1;
          result->fe_type = 'f';
          result->fe_fname = tokens[0];
          result->fe_fnlen = pos;

          if (pos > 4)
          {
            p = &(tokens[0][pos-4]);
            if (p[0] == '.' && p[1] == 'D' && p[2] == 'I' && p[3] == 'R')
            {
              result->fe_fnlen -= 4;
              result->fe_type = 'd';
            }
          }

          if (result->fe_type != 'd')
          {
            /* #### or used/allocated form. If used/allocated form, then
             * 'used' is the size in bytes if and only if 'used'<=allocated.
             * If 'used' is size in bytes then it can be > 2^32
             * If 'used' is not size in bytes then it is size in blocks.
            */
            pos = 0;
            while (pos < toklen[1] && (tokens[1][pos]) != '/')
              pos++;
            
/*
 * I've never seen size come back in bytes, its always in blocks, and 
 * the following test fails. So, always perform the "size in blocks".
 * I'm leaving the "size in bytes" code if'd out in case we ever need
 * to re-instate it.
*/
#if 0
            if (pos < toklen[1] && ( (pos<<1) > (toklen[1]-1) ||
                 (strtoul(tokens[1], (char **)0, 10) > 
                  strtoul(tokens[1]+pos+1, (char **)0, 10))        ))
            {                                   /* size is in bytes */
              if (pos > (sizeof(result->fe_size)-1))
                pos = sizeof(result->fe_size)-1;
              memcpy( result->fe_size, tokens[1], pos );
              result->fe_size[pos] = '\0';
            }
            else /* size is in blocks */
#endif
            {
              /* size requires multiplication by blocksize. 
               *
               * We could assume blocksize is 512 (like Lynx does) and
               * shift by 9, but that might not be right. Even if it 
               * were, doing that wouldn't reflect what the file's 
               * real size was. The sanest thing to do is not use the
               * LISTing's filesize, so we won't (like ftpmirror).
               *
               * ulltoa(((unsigned long long)fsz)<<9, result->fe_size, 10);
               *
               * A block is always 512 bytes on OpenVMS, compute size.
               * So its rounded up to the next block, so what, its better
               * than not showing the size at all.
               * A block is always 512 bytes on OpenVMS, compute size.
               * So its rounded up to the next block, so what, its better
               * than not showing the size at all.
              */
              uint64_t fsz = uint64_t(strtoul(tokens[1], (char **)0, 10) * 512);
              PR_snprintf(result->fe_size, sizeof(result->fe_size), 
                          "%lld", fsz);
            } 

          } /* if (result->fe_type != 'd') */

          p = tokens[2] + 2;
          if (*p == '-')
            p++;
          tbuf[0] = p[0];
          tbuf[1] = tolower(p[1]);
          tbuf[2] = tolower(p[2]);
          month_num = 0;
          for (pos = 0; pos < (12*3); pos+=3)
          {
            if (tbuf[0] == month_names[pos+0] && 
                tbuf[1] == month_names[pos+1] && 
                tbuf[2] == month_names[pos+2])
              break;
            month_num++;
          }
          if (month_num >= 12)
            month_num = 0;
          result->fe_time.tm_month = month_num;
          result->fe_time.tm_mday = atoi(tokens[2]);
          result->fe_time.tm_year = atoi(p+4); // NSPR wants year as XXXX

          p = tokens[3] + 2;
          if (*p == ':')
            p++;
          if (p[2] == ':')
            result->fe_time.tm_sec = atoi(p+3);
          result->fe_time.tm_hour = atoi(tokens[3]);
          result->fe_time.tm_min  = atoi(p);
      
          return result->fe_type;

        } /* if (isdigit(*tokens[1])) */

        return '?'; /* junk */

      } /* if (lstyle == 'V') */
    } /* if (!lstyle && (!state->lstyle || state->lstyle == 'V')) */
#endif

    /* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

#if defined(SUPPORT_CMS)
    /* Virtual Machine/Conversational Monitor System (IBM Mainframe) */
    if (!lstyle && (!state->lstyle || state->lstyle == 'C'))  /* VM/CMS */
    {
      /* LISTing according to mirror.pl
       * Filename FileType  Fm Format Lrecl  Records Blocks Date      Time
       * LASTING  GLOBALV   A1 V      41     21     1       9/16/91   15:10:32
       * J43401   NETLOG    A0 V      77     1      1       9/12/91   12:36:04
       * PROFILE  EXEC      A1 V      17     3      1       9/12/91   12:39:07
       * DIRUNIX  SCRIPT    A1 V      77     1216   17      1/04/93   20:30:47
       * MAIL     PROFILE   A2 F      80     1      1       10/14/92  16:12:27
       * BADY2K   TEXT      A0 V      1      1      1       1/03/102  10:11:12
       * AUTHORS            A1 DIR    -      -      -       9/20/99   10:31:11
       *
       * LISTing from vm.marist.edu and vm.sc.edu
       * 220-FTPSERVE IBM VM Level 420 at VM.MARIST.EDU, 04:58:12 EDT WEDNESDAY 2002-07-10
       * AUTHORS           DIR        -          -          - 1999-09-20 10:31:11 -
       * HARRINGTON        DIR        -          -          - 1997-02-12 15:33:28 -
       * PICS              DIR        -          -          - 2000-10-12 15:43:23 -
       * SYSFILE           DIR        -          -          - 2000-07-20 17:48:01 -
       * WELCNVT  EXEC     V         72          9          1 1999-09-20 17:16:18 -
       * WELCOME  EREADME  F         80         21          1 1999-12-27 16:19:00 -
       * WELCOME  README   V         82         21          1 1999-12-27 16:19:04 -
       * README   ANONYMOU V         71         26          1 1997-04-02 12:33:20 TCP291
       * README   ANONYOLD V         71         15          1 1995-08-25 16:04:27 TCP291
      */
      if (numtoks >= 7 && (toklen[0]+toklen[1]) <= 16)
      {
        for (pos = 1; !lstyle && (pos+5) < numtoks; pos++)
        {
          p = tokens[pos];
          if ((toklen[pos] == 1 && (*p == 'F' || *p == 'V')) ||
              (toklen[pos] == 3 && *p == 'D' && p[1] == 'I' && p[2] == 'R'))
          {
            if (toklen[pos+5] == 8 && (tokens[pos+5][2]) == ':' &&
                                      (tokens[pos+5][5]) == ':'   )
            {
              p = tokens[pos+4];
              if ((toklen[pos+4] == 10 && p[4] == '-' && p[7] == '-') ||
                  (toklen[pos+4] >= 7 && toklen[pos+4] <= 9 && 
                            p[((p[1]!='/')?(2):(1))] == '/' && 
                            p[((p[1]!='/')?(5):(4))] == '/'))
               /* Y2K bugs possible ("7/06/102" or "13/02/101") */
              {
                if ( (*tokens[pos+1] == '-' &&
                      *tokens[pos+2] == '-' &&
                      *tokens[pos+3] == '-')  ||
                      (isdigit(*tokens[pos+1]) &&
                       isdigit(*tokens[pos+2]) &&
                       isdigit(*tokens[pos+3])) )
                {
                  lstyle = 'C';
                  tokmarker = pos;
                }
              }
            }
          }
        } /* for (pos = 1; !lstyle && (pos+5) < numtoks; pos++) */
      } /* if (numtoks >= 7) */

      /* extra checking if first pass */
      if (lstyle && !state->lstyle) 
      {
        for (pos = 0, p = tokens[0]; lstyle && pos < toklen[0]; pos++, p++)
        {  
          if (isalpha(*p) && toupper(*p) != *p)
            lstyle = 0;
        } 
        for (pos = tokmarker+1; pos <= tokmarker+3; pos++)
        {
          if (!(toklen[pos] == 1 && *tokens[pos] == '-'))
          {
            for (p = tokens[pos]; lstyle && p<(tokens[pos]+toklen[pos]); p++)
            {
              if (!isdigit(*p))
                lstyle = 0;
            }
          }
        }
        for (pos = 0, p = tokens[tokmarker+4]; 
             lstyle && pos < toklen[tokmarker+4]; pos++, p++)
        {
          if (*p == '/')
          { 
            /* There may be Y2K bugs in the date. Don't simplify to
             * pos != (len-3) && pos != (len-6) like time is done.
            */             
            if ((tokens[tokmarker+4][1]) == '/')
            {
              if (pos != 1 && pos != 4)
                lstyle = 0;
            }
            else if (pos != 2 && pos != 5)
              lstyle = 0;
          }
          else if (*p != '-' && !isdigit(*p))
            lstyle = 0;
          else if (*p == '-' && pos != 4 && pos != 7)
            lstyle = 0;
        }
        for (pos = 0, p = tokens[tokmarker+5]; 
             lstyle && pos < toklen[tokmarker+5]; pos++, p++)
        {
          if (*p != ':' && !isdigit(*p))
            lstyle = 0;
          else if (*p == ':' && pos != (toklen[tokmarker+5]-3)
                             && pos != (toklen[tokmarker+5]-6))
            lstyle = 0;
        }
      } /* initial if() */

      if (lstyle == 'C')
      {
        state->parsed_one = 1;
        state->lstyle = lstyle;

        p = tokens[tokmarker+4];
        if (toklen[tokmarker+4] == 10) /* newstyle: YYYY-MM-DD format */
        {
          result->fe_time.tm_year = atoi(p+0) - 1900;
          result->fe_time.tm_month  = atoi(p+5) - 1;
          result->fe_time.tm_mday = atoi(p+8);
        }
        else /* oldstyle: [M]M/DD/YY format */
        {
          pos = toklen[tokmarker+4];
          result->fe_time.tm_month  = atoi(p) - 1;
          result->fe_time.tm_mday = atoi((p+pos)-5);
          result->fe_time.tm_year = atoi((p+pos)-2);
          if (result->fe_time.tm_year < 70)
            result->fe_time.tm_year += 100;
        }

        p = tokens[tokmarker+5];
        pos = toklen[tokmarker+5];
        result->fe_time.tm_hour  = atoi(p);
        result->fe_time.tm_min = atoi((p+pos)-5);
        result->fe_time.tm_sec = atoi((p+pos)-2);

        result->fe_cinfs = 1;
        result->fe_fname = tokens[0];
        result->fe_fnlen = toklen[0];
        result->fe_type  = 'f';

        p = tokens[tokmarker];
        if (toklen[tokmarker] == 3 && *p=='D' && p[1]=='I' && p[2]=='R')
          result->fe_type  = 'd';

        if ((/*newstyle*/ toklen[tokmarker+4] == 10 && tokmarker > 1) ||
            (/*oldstyle*/ toklen[tokmarker+4] != 10 && tokmarker > 2))
        {                            /* have a filetype column */
          char *dot;
          p = &(tokens[0][toklen[0]]);
          memcpy( &dot, &p, sizeof(dot) ); /* NASTY! */
          *dot++ = '.';
          p = tokens[1];
          for (pos = 0; pos < toklen[1]; pos++)
            *dot++ = *p++;
          result->fe_fnlen += 1 + toklen[1];
        }

        /* oldstyle LISTING: 
         * files/dirs not on the 'A' minidisk are not RETRievable/CHDIRable 
        if (toklen[tokmarker+4] != 10 && *tokens[tokmarker-1] != 'A')
          return '?';
        */
        
        /* VM/CMS LISTings have no usable filesize field. 
         * Have to use the 'SIZE' command for that.
        */
        return result->fe_type;

      } /* if (lstyle == 'C' && (!state->lstyle || state->lstyle == lstyle)) */
    } /* VM/CMS */
#endif

    /* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

#if defined(SUPPORT_DOS) /* WinNT DOS dirstyle */
    if (!lstyle && (!state->lstyle || state->lstyle == 'W'))
    {
      /*
       * "10-23-00  01:27PM       <DIR>          veronist"
       * "06-15-00  07:37AM       <DIR>          zoe"
       * "07-14-00  01:35PM              2094926 canprankdesk.tif"
       * "07-21-00  01:19PM                95077 Jon Kauffman Enjoys the Good Life.jpg"
       * "07-21-00  01:19PM                52275 Name Plate.jpg"
       * "07-14-00  01:38PM              2250540 Valentineoffprank-HiRes.jpg"
      */
      if ((numtoks >= 4) && toklen[0] == 8 && toklen[1] == 7 && 
          (*tokens[2] == '<' || isdigit(*tokens[2])) )
      {
        p = tokens[0];
        if ( isdigit(p[0]) && isdigit(p[1]) && p[2]=='-' && 
             isdigit(p[3]) && isdigit(p[4]) && p[5]=='-' &&
             isdigit(p[6]) && isdigit(p[7]) )
        {
          p = tokens[1];
          if ( isdigit(p[0]) && isdigit(p[1]) && p[2]==':' && 
               isdigit(p[3]) && isdigit(p[4]) && 
               (p[5]=='A' || p[5]=='P') && p[6]=='M')
          {
            lstyle = 'W';
            if (!state->lstyle)
            {            
              p = tokens[2];
              /* <DIR> or <JUNCTION> */
              if (*p != '<' || p[toklen[2]-1] != '>')
              {
                for (pos = 1; (lstyle && pos < toklen[2]); pos++)
                {
                  if (!isdigit(*++p))
                    lstyle = 0;
                }
              }
            }
          }
        }
      }

      if (lstyle == 'W')
      {
        state->parsed_one = 1;
        state->lstyle = lstyle;

        p = &(line[linelen]); /* line end */
        result->fe_cinfs = 1;
        result->fe_fname = tokens[3];
        result->fe_fnlen = p - tokens[3];
        result->fe_type = 'd';

        if (*tokens[2] != '<') /* not <DIR> or <JUNCTION> */
        {
          // try to handle correctly spaces at the beginning of the filename
          // filesize (token[2]) must end at offset 38
          if (tokens[2] + toklen[2] - line == 38) {
            result->fe_fname = &(line[39]);
            result->fe_fnlen = p - result->fe_fname;
          }
          result->fe_type = 'f';
          pos = toklen[2];
          while (pos > (sizeof(result->fe_size)-1))
            pos = (sizeof(result->fe_size)-1);
          memcpy( result->fe_size, tokens[2], pos );
          result->fe_size[pos] = '\0';
        }
        else {
          // try to handle correctly spaces at the beginning of the filename
          // token[2] must begin at offset 24, the length is 5 or 10
          // token[3] must begin at offset 39 or higher
          if (tokens[2] - line == 24 && (toklen[2] == 5 || toklen[2] == 10) &&
              tokens[3] - line >= 39) {
            result->fe_fname = &(line[39]);
            result->fe_fnlen = p - result->fe_fname;
          }

          if ((tokens[2][1]) != 'D') /* not <DIR> */
          {
            result->fe_type = '?'; /* unknown until junc for sure */
            if (result->fe_fnlen > 4)
            {
              p = result->fe_fname;
              for (pos = result->fe_fnlen - 4; pos > 0; pos--)
              {
                if (p[0] == ' ' && p[3] == ' ' && p[2] == '>' &&
                    (p[1] == '=' || p[1] == '-'))
                {
                  result->fe_type = 'l';
                  result->fe_fnlen = p - result->fe_fname;
                  result->fe_lname = p + 4;
                  result->fe_lnlen = &(line[linelen]) 
                                     - result->fe_lname;
                  break;
                }
                p++;
              }
            }
          }
        }

        result->fe_time.tm_month = atoi(tokens[0]+0);
        if (result->fe_time.tm_month != 0)
        {
          result->fe_time.tm_month--;
          result->fe_time.tm_mday = atoi(tokens[0]+3);
          result->fe_time.tm_year = atoi(tokens[0]+6);
          /* if year has only two digits then assume that
               00-79 is 2000-2079
               80-99 is 1980-1999 */
          if (result->fe_time.tm_year < 80)
            result->fe_time.tm_year += 2000;
          else if (result->fe_time.tm_year < 100)
            result->fe_time.tm_year += 1900;
        }

        result->fe_time.tm_hour = atoi(tokens[1]+0);
        result->fe_time.tm_min = atoi(tokens[1]+3);
        if ((tokens[1][5]) == 'P' && result->fe_time.tm_hour < 12)
          result->fe_time.tm_hour += 12;

        /* the caller should do this (if dropping "." and ".." is desired)
        if (result->fe_type == 'd' && result->fe_fname[0] == '.' &&
            (result->fe_fnlen == 1 || (result->fe_fnlen == 2 &&
                                      result->fe_fname[1] == '.')))
          return '?';
        */

        return result->fe_type;  
      } /* if (lstyle == 'W' && (!state->lstyle || state->lstyle == lstyle)) */
    } /* if (!lstyle && (!state->lstyle || state->lstyle == 'W')) */
#endif

    /* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

#if defined(SUPPORT_OS2)
    if (!lstyle && (!state->lstyle || state->lstyle == 'O')) /* OS/2 test */
    {
      /* 220 server IBM TCP/IP for OS/2 - FTP Server ver 23:04:36 on Jan 15 1997 ready.
      * fixed position, space padded columns. I have only a vague idea 
      * of what the contents between col 18 and 34 might be: All I can infer
      * is that there may be attribute flags in there and there may be 
      * a " DIR" in there.
      *
      *          1         2         3         4         5         6
      *0123456789012345678901234567890123456789012345678901234567890123456789
      *----- size -------|??????????????? MM-DD-YY|  HH:MM| nnnnnnnnn....
      *                 0  DIR            04-11-95   16:26  .
      *                 0  DIR            04-11-95   16:26  ..
      *                 0  DIR            04-11-95   16:26  ADDRESS
      *               612  RHSA           07-28-95   16:45  air_tra1.bag
      *               195  A              08-09-95   10:23  Alfa1.bag
      *                 0  RHS   DIR      04-11-95   16:26  ATTACH
      *               372  A              08-09-95   10:26  Aussie_1.bag
      *            310992                 06-28-94   09:56  INSTALL.EXE
      *                            1         2         3         4
      *                  01234567890123456789012345678901234567890123456789
      * dirlist from the mirror.pl project, col positions from Mozilla.
      */
      p = &(line[toklen[0]]);
      /* \s(\d\d-\d\d-\d\d)\s+(\d\d:\d\d)\s */
      if (numtoks >= 4 && toklen[0] <= 18 && isdigit(*tokens[0]) &&
         (linelen - toklen[0]) >= (53-18)                        &&
         p[18-18] == ' ' && p[34-18] == ' '                      &&
         p[37-18] == '-' && p[40-18] == '-' && p[43-18] == ' '   &&
         p[45-18] == ' ' && p[48-18] == ':' && p[51-18] == ' '   &&
         isdigit(p[35-18]) && isdigit(p[36-18])                  &&
         isdigit(p[38-18]) && isdigit(p[39-18])                  &&
         isdigit(p[41-18]) && isdigit(p[42-18])                  &&
         isdigit(p[46-18]) && isdigit(p[47-18])                  &&
         isdigit(p[49-18]) && isdigit(p[50-18])
      )
      {
        lstyle = 'O'; /* OS/2 */
        if (!state->lstyle)
        {            
          for (pos = 1; lstyle && pos < toklen[0]; pos++)
          {
            if (!isdigit(tokens[0][pos]))
              lstyle = 0;
          }
        }
      }

      if (lstyle == 'O')
      {
        state->parsed_one = 1;
        state->lstyle = lstyle;

        p = &(line[toklen[0]]);

        result->fe_cinfs = 1;
        result->fe_fname = &p[53-18];
        result->fe_fnlen = (&(line[linelen_sans_wsp]))
                           - (result->fe_fname);
        result->fe_type = 'f';

        /* I don't have a real listing to determine exact pos, so scan. */
        for (pos = (18-18); pos < ((35-18)-4); pos++)
        {
          if (p[pos+0] == ' ' && p[pos+1] == 'D' && 
              p[pos+2] == 'I' && p[pos+3] == 'R')
          {
            result->fe_type = 'd';
            break;
          }
        }
    
        if (result->fe_type != 'd')
        {
          pos = toklen[0];
          if (pos > (sizeof(result->fe_size)-1))
            pos = (sizeof(result->fe_size)-1);
          memcpy( result->fe_size, tokens[0], pos );
          result->fe_size[pos] = '\0';
        }  
    
        result->fe_time.tm_month = atoi(&p[35-18]) - 1;
        result->fe_time.tm_mday = atoi(&p[38-18]);
        result->fe_time.tm_year = atoi(&p[41-18]);
        if (result->fe_time.tm_year < 80)
          result->fe_time.tm_year += 100;
        result->fe_time.tm_hour = atoi(&p[46-18]);
        result->fe_time.tm_min = atoi(&p[49-18]);
   
        /* the caller should do this (if dropping "." and ".." is desired)
        if (result->fe_type == 'd' && result->fe_fname[0] == '.' &&
            (result->fe_fnlen == 1 || (result->fe_fnlen == 2 &&
                                      result->fe_fname[1] == '.')))
          return '?';
        */

        return result->fe_type;
      } /* if (lstyle == 'O') */

    } /* if (!lstyle && (!state->lstyle || state->lstyle == 'O')) */
#endif

    /* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
    
#if defined(SUPPORT_LSL)
    if (!lstyle && (!state->lstyle || state->lstyle == 'U')) /* /bin/ls & co. */
    {
      /* UNIX-style listing, without inum and without blocks
       * "-rw-r--r--   1 root     other        531 Jan 29 03:26 README"
       * "dr-xr-xr-x   2 root     other        512 Apr  8  1994 etc"
       * "dr-xr-xr-x   2 root     512 Apr  8  1994 etc"
       * "lrwxrwxrwx   1 root     other          7 Jan 25 00:17 bin -> usr/bin"
       * Also produced by Microsoft's FTP servers for Windows:
       * "----------   1 owner    group         1803128 Jul 10 10:18 ls-lR.Z"
       * "d---------   1 owner    group               0 May  9 19:45 Softlib"
       * Also WFTPD for MSDOS:
       * "-rwxrwxrwx   1 noone    nogroup      322 Aug 19  1996 message.ftp"
       * Hellsoft for NetWare:
       * "d[RWCEMFA] supervisor            512       Jan 16 18:53    login"
       * "-[RWCEMFA] rhesus             214059       Oct 20 15:27    cx.exe"
       * Newer Hellsoft for NetWare: (netlab2.usu.edu)
       * - [RWCEAFMS] NFAUUser               192 Apr 27 15:21 HEADER.html
       * d [RWCEAFMS] jrd                    512 Jul 11 03:01 allupdates
       * Also NetPresenz for the Mac:
       * "-------r--         326  1391972  1392298 Nov 22  1995 MegaPhone.sit"
       * "drwxrwxr-x               folder        2 May 10  1996 network"
       * Protected directory:
       * "drwx-wx-wt  2 root  wheel  512 Jul  1 02:15 incoming"
       * uid/gid instead of username/groupname:
       * "drwxr-xr-x  2 0  0  512 May 28 22:17 etc"
      */
    
      bool is_old_Hellsoft = false;
    
      if (numtoks >= 6)
      {
        /* there are two perm formats (Hellsoft/NetWare and *IX strmode(3)).
         * Scan for size column only if the perm format is one or the other.
         */
        if (toklen[0] == 1 || (tokens[0][1]) == '[')
        {
          if (*tokens[0] == 'd' || *tokens[0] == '-')
          {
            pos = toklen[0]-1;
            p = tokens[0] + 1;
            if (pos == 0)
            {
              p = tokens[1];
              pos = toklen[1];
            }
            if ((pos == 9 || pos == 10)        && 
                (*p == '[' && p[pos-1] == ']') &&
                (p[1] == 'R' || p[1] == '-')   &&
                (p[2] == 'W' || p[2] == '-')   &&
                (p[3] == 'C' || p[3] == '-')   &&
                (p[4] == 'E' || p[4] == '-'))
            {
              /* rest is FMA[S] or AFM[S] */
              lstyle = 'U'; /* very likely one of the NetWare servers */
              if (toklen[0] == 10)
                is_old_Hellsoft = true;
            }
          }
        }
        else if ((toklen[0] == 10 || toklen[0] == 11) 
                   && strchr("-bcdlpsw?DFam", *tokens[0]))
        {
          p = &(tokens[0][1]);
          if ((p[0] == 'r' || p[0] == '-') &&
              (p[1] == 'w' || p[1] == '-') &&
              (p[3] == 'r' || p[3] == '-') &&
              (p[4] == 'w' || p[4] == '-') &&
              (p[6] == 'r' || p[6] == '-') &&
              (p[7] == 'w' || p[7] == '-'))
            /* 'x'/p[9] can be S|s|x|-|T|t or implementation specific */
          {
            lstyle = 'U'; /* very likely /bin/ls */
          }
        }
      }
      if (lstyle == 'U') /* first token checks out */
      {
        lstyle = 0;
        for (pos = (numtoks-5); !lstyle && pos > 1; pos--)
        {
          /* scan for: (\d+)\s+([A-Z][a-z][a-z])\s+
           *  (\d\d\d\d|\d\:\d\d|\d\d\:\d\d|\d\:\d\d\:\d\d|\d\d\:\d\d\:\d\d)
           *  \s+(.+)$
          */
          if (isdigit(*tokens[pos]) /* size */
              /* (\w\w\w) */
           && toklen[pos+1] == 3 && isalpha(*tokens[pos+1]) &&
              isalpha(tokens[pos+1][1]) && isalpha(tokens[pos+1][2])
              /* (\d|\d\d) */
           && isdigit(*tokens[pos+2]) &&
                (toklen[pos+2] == 1 || 
                  (toklen[pos+2] == 2 && isdigit(tokens[pos+2][1])))
           && toklen[pos+3] >= 4 && isdigit(*tokens[pos+3]) 
              /* (\d\:\d\d\:\d\d|\d\d\:\d\d\:\d\d) */
           && (toklen[pos+3] <= 5 || (
               (toklen[pos+3] == 7 || toklen[pos+3] == 8) &&
               (tokens[pos+3][toklen[pos+3]-3]) == ':'))
           && isdigit(tokens[pos+3][toklen[pos+3]-2])
           && isdigit(tokens[pos+3][toklen[pos+3]-1])
           && (
              /* (\d\d\d\d) */
                 ((toklen[pos+3] == 4 || toklen[pos+3] == 5) &&
                  isdigit(tokens[pos+3][1]) &&
                  isdigit(tokens[pos+3][2])  )
              /* (\d\:\d\d|\d\:\d\d\:\d\d) */
              || ((toklen[pos+3] == 4 || toklen[pos+3] == 7) && 
                  (tokens[pos+3][1]) == ':' &&
                  isdigit(tokens[pos+3][2]) && isdigit(tokens[pos+3][3]))
              /* (\d\d\:\d\d|\d\d\:\d\d\:\d\d) */
              || ((toklen[pos+3] == 5 || toklen[pos+3] == 8) && 
                  isdigit(tokens[pos+3][1]) && (tokens[pos+3][2]) == ':' &&
                  isdigit(tokens[pos+3][3]) && isdigit(tokens[pos+3][4])) 
              )
           )
          {
            lstyle = 'U'; /* assume /bin/ls or variant format */
            tokmarker = pos;

            /* check that size is numeric */
            p = tokens[tokmarker];
            unsigned int i;
            for (i = 0; i < toklen[tokmarker]; i++)
            {
              if (!isdigit(*p++))
              {
                lstyle = 0;
                break;
              }
            }
            if (lstyle)
            {
              month_num = 0;
              p = tokens[tokmarker+1];
              for (i = 0; i < (12*3); i+=3)
              {
                if (p[0] == month_names[i+0] && 
                    p[1] == month_names[i+1] && 
                    p[2] == month_names[i+2])
                  break;
                month_num++;
              }
              if (month_num >= 12)
                lstyle = 0;
            }
          } /* relative position test */
        } /* for (pos = (numtoks-5); !lstyle && pos > 1; pos--) */
      } /* if (lstyle == 'U') */

      if (lstyle == 'U')
      {
        state->parsed_one = 1;
        state->lstyle = lstyle;
    
        result->fe_cinfs = 0;
        result->fe_type = '?';
        if (*tokens[0] == 'd' || *tokens[0] == 'l')
          result->fe_type = *tokens[0];
        else if (*tokens[0] == 'D')
          result->fe_type = 'd';
        else if (*tokens[0] == '-' || *tokens[0] == 'F')
          result->fe_type = 'f'; /* (hopefully a regular file) */

        if (result->fe_type != 'd')
        {
          pos = toklen[tokmarker];
          if (pos > (sizeof(result->fe_size)-1))
            pos = (sizeof(result->fe_size)-1);
          memcpy( result->fe_size, tokens[tokmarker], pos );
          result->fe_size[pos] = '\0';
        }

        result->fe_time.tm_month  = month_num;
        result->fe_time.tm_mday = atoi(tokens[tokmarker+2]);
        if (result->fe_time.tm_mday == 0)
          result->fe_time.tm_mday++;

        p = tokens[tokmarker+3];
        pos = (unsigned int)atoi(p);
        if (p[1] == ':') /* one digit hour */
          p--;
        if (p[2] != ':') /* year */
        {
          result->fe_time.tm_year = pos;
        }
        else
        {
          result->fe_time.tm_hour = pos;
          result->fe_time.tm_min  = atoi(p+3);
          if (p[5] == ':')
            result->fe_time.tm_sec = atoi(p+6);
       
          if (!state->now_time)
          {
            state->now_time = PR_Now();
            PR_ExplodeTime((state->now_time), PR_LocalTimeParameters, &(state->now_tm) );
          }

          result->fe_time.tm_year = state->now_tm.tm_year;
          if ( (( state->now_tm.tm_month << 5) + state->now_tm.tm_mday) <
               ((result->fe_time.tm_month << 5) + result->fe_time.tm_mday) )
            result->fe_time.tm_year--;
       
        } /* time/year */
        
        // The length of the whole date string should be 12. On AIX the length
        // is only 11 when the year is present in the date string and there is
        // 1 padding space at the end of the string. In both cases the filename
        // starts at offset 13 from the start of the date string.
        // Don't care about leading spaces when the date string has different
        // format or when old Hellsoft output was detected.
        {
          const char *date_start = tokens[tokmarker+1];
          const char *date_end = tokens[tokmarker+3] + toklen[tokmarker+3];
          if (!is_old_Hellsoft && ((date_end - date_start) == 12 ||
              ((date_end - date_start) == 11 && date_end[1] == ' ')))
            result->fe_fname = date_start + 13;
          else
            result->fe_fname = tokens[tokmarker+4];
        }

        result->fe_fnlen = (&(line[linelen]))
                           - (result->fe_fname);

        if (result->fe_type == 'l' && result->fe_fnlen > 4)
        {
          /* First try to use result->fe_size to find " -> " sequence.
             This can give proper result for cases like "aaa -> bbb -> ccc". */
          uint32_t fe_size = atoi(result->fe_size);

          if (result->fe_fnlen > (fe_size + 4) &&
              PL_strncmp(result->fe_fname + result->fe_fnlen - fe_size - 4 , " -> ", 4) == 0)
          {
            result->fe_lname = result->fe_fname + (result->fe_fnlen - fe_size);
            result->fe_lnlen = (&(line[linelen])) - (result->fe_lname);
            result->fe_fnlen -= fe_size + 4;
          }
          else
          {
            /* Search for sequence " -> " from the end for case when there are
               more occurrences. F.e. if ftpd returns "a -> b -> c" assume
               "a -> b" as a name. Powerusers can remove unnecessary parts
               manually but there is no way to follow the link when some
               essential part is missing. */
            p = result->fe_fname + (result->fe_fnlen - 5);
            for (pos = (result->fe_fnlen - 5); pos > 0; pos--)
            {
              if (PL_strncmp(p, " -> ", 4) == 0)
              {
                result->fe_lname = p + 4;
                result->fe_lnlen = (&(line[linelen]))
                                 - (result->fe_lname);
                result->fe_fnlen = pos;
                break;
              }
              p--;
            }
          }
        }

#if defined(SUPPORT_LSLF) /* some (very rare) servers return ls -lF */
        if (result->fe_fnlen > 1)
        {
          p = result->fe_fname[result->fe_fnlen-1];
          pos = result->fe_type;
          if (pos == 'd') { 
             if (*p == '/') result->fe_fnlen--; /* directory */
          } else if (pos == 'l') { 
             if (*p == '@') result->fe_fnlen--; /* symlink */
          } else if (pos == 'f') { 
             if (*p == '*') result->fe_fnlen--; /* executable */
          } else if (*p == '=' || *p == '%' || *p == '|') {
            result->fe_fnlen--; /* socket, whiteout, fifo */
          }
        }
#endif
     
        /* the caller should do this (if dropping "." and ".." is desired)
        if (result->fe_type == 'd' && result->fe_fname[0] == '.' &&
            (result->fe_fnlen == 1 || (result->fe_fnlen == 2 &&
                                      result->fe_fname[1] == '.')))
          return '?';
        */

        return result->fe_type;  

      } /* if (lstyle == 'U') */

    } /* if (!lstyle && (!state->lstyle || state->lstyle == 'U')) */
#endif

    /* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

#if defined(SUPPORT_W16) /* 16bit Windows */
    if (!lstyle && (!state->lstyle || state->lstyle == 'w'))
    {       /* old SuperTCP suite FTP server for Win3.1 */
            /* old NetManage Chameleon TCP/IP suite FTP server for Win3.1 */
      /*
      * SuperTCP dirlist from the mirror.pl project
      * mon/day/year separator may be '/' or '-'.
      * .               <DIR>           11-16-94        17:16
      * ..              <DIR>           11-16-94        17:16
      * INSTALL         <DIR>           11-16-94        17:17
      * CMT             <DIR>           11-21-94        10:17
      * DESIGN1.DOC          11264      05-11-95        14:20
      * README.TXT            1045      05-10-95        11:01
      * WPKIT1.EXE          960338      06-21-95        17:01
      * CMT.CSV                  0      07-06-95        14:56
      *
      * Chameleon dirlist guessed from lynx
      * .               <DIR>      Nov 16 1994 17:16   
      * ..              <DIR>      Nov 16 1994 17:16   
      * INSTALL         <DIR>      Nov 16 1994 17:17
      * CMT             <DIR>      Nov 21 1994 10:17
      * DESIGN1.DOC     11264      May 11 1995 14:20   A
      * README.TXT       1045      May 10 1995 11:01
      * WPKIT1.EXE     960338      Jun 21 1995 17:01   R
      * CMT.CSV             0      Jul 06 1995 14:56   RHA
      */
      if (numtoks >= 4 && toklen[0] < 13 && 
          ((toklen[1] == 5 && *tokens[1] == '<') || isdigit(*tokens[1])) )
      {
        if (numtoks == 4
         && (toklen[2] == 8 || toklen[2] == 9)
         && (((tokens[2][2]) == '/' && (tokens[2][5]) == '/') ||
             ((tokens[2][2]) == '-' && (tokens[2][5]) == '-'))
         && (toklen[3] == 4 || toklen[3] == 5)
         && (tokens[3][toklen[3]-3]) == ':'
         && isdigit(tokens[2][0]) && isdigit(tokens[2][1])
         && isdigit(tokens[2][3]) && isdigit(tokens[2][4])
         && isdigit(tokens[2][6]) && isdigit(tokens[2][7])
         && (toklen[2] < 9 || isdigit(tokens[2][8]))
         && isdigit(tokens[3][toklen[3]-1]) && isdigit(tokens[3][toklen[3]-2])
         && isdigit(tokens[3][toklen[3]-4]) && isdigit(*tokens[3]) 
         )
        {
          lstyle = 'w';
        }
        else if ((numtoks == 6 || numtoks == 7)
         && toklen[2] == 3 && toklen[3] == 2
         && toklen[4] == 4 && toklen[5] == 5
         && (tokens[5][2]) == ':'
         && isalpha(tokens[2][0]) && isalpha(tokens[2][1])
         &&                          isalpha(tokens[2][2])
         && isdigit(tokens[3][0]) && isdigit(tokens[3][1])
         && isdigit(tokens[4][0]) && isdigit(tokens[4][1])
         && isdigit(tokens[4][2]) && isdigit(tokens[4][3])
         && isdigit(tokens[5][0]) && isdigit(tokens[5][1])
         && isdigit(tokens[5][3]) && isdigit(tokens[5][4])
         /* could also check that (&(tokens[5][5]) - tokens[2]) == 17 */
        )
        {
          lstyle = 'w';
        }
        if (lstyle && state->lstyle != lstyle) /* first time */
        {
          p = tokens[1];   
          if (toklen[1] != 5 || p[0] != '<' || p[1] != 'D' || 
                 p[2] != 'I' || p[3] != 'R' || p[4] != '>')
          {
            for (pos = 0; lstyle && pos < toklen[1]; pos++)
            {
              if (!isdigit(*p++))
                lstyle = 0;
            }
          } /* not <DIR> */
        } /* if (first time) */
      } /* if (numtoks == ...) */

      if (lstyle == 'w')
      {
        state->parsed_one = 1;
        state->lstyle = lstyle;

        result->fe_cinfs = 1;
        result->fe_fname = tokens[0];
        result->fe_fnlen = toklen[0];
        result->fe_type = 'd';

        p = tokens[1];
        if (isdigit(*p))
        {
          result->fe_type = 'f';
          pos = toklen[1];
          if (pos > (sizeof(result->fe_size)-1))
            pos = sizeof(result->fe_size)-1;
          memcpy( result->fe_size, p, pos );
          result->fe_size[pos] = '\0';
        }

        p = tokens[2];
        if (toklen[2] == 3) /* Chameleon */
        {
          tbuf[0] = toupper(p[0]);
          tbuf[1] = tolower(p[1]);
          tbuf[2] = tolower(p[2]);
          for (pos = 0; pos < (12*3); pos+=3)
          {
            if (tbuf[0] == month_names[pos+0] &&
                tbuf[1] == month_names[pos+1] && 
                tbuf[2] == month_names[pos+2])
            {
              result->fe_time.tm_month = pos/3;
              result->fe_time.tm_mday = atoi(tokens[3]);
              result->fe_time.tm_year = atoi(tokens[4]) - 1900;
              break;
            }
          }          
          pos = 5; /* Chameleon toknum of date field */
        }
        else
        {
          result->fe_time.tm_month = atoi(p+0)-1;
          result->fe_time.tm_mday = atoi(p+3);
          result->fe_time.tm_year = atoi(p+6);
          if (result->fe_time.tm_year < 80) /* SuperTCP */
            result->fe_time.tm_year += 100;

          pos = 3; /* SuperTCP toknum of date field */
        }

        result->fe_time.tm_hour = atoi(tokens[pos]);
        result->fe_time.tm_min = atoi(&(tokens[pos][toklen[pos]-2]));

        /* the caller should do this (if dropping "." and ".." is desired)
        if (result->fe_type == 'd' && result->fe_fname[0] == '.' &&
            (result->fe_fnlen == 1 || (result->fe_fnlen == 2 &&
                                      result->fe_fname[1] == '.')))
          return '?';
        */

        return result->fe_type;
      } /* (lstyle == 'w') */

    } /* if (!lstyle && (!state->lstyle || state->lstyle == 'w'))  */
#endif

    /* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

#if defined(SUPPORT_DLS) /* dls -dtR */
    if (!lstyle && 
       (state->lstyle == 'D' || (!state->lstyle && state->numlines == 1)))
       /* /bin/dls lines have to be immediately recognizable (first line) */
    {
      /* I haven't seen an FTP server that delivers a /bin/dls listing,
       * but can infer the format from the lynx and mirror.pl projects.
       * Both formats are supported.
       *
       * Lynx says:
       * README              763  Information about this server\0
       * bin/                  -  \0
       * etc/                  =  \0
       * ls-lR                 0  \0
       * ls-lR.Z               3  \0
       * pub/                  =  Public area\0
       * usr/                  -  \0
       * morgan               14  -> ../real/morgan\0
       * TIMIT.mostlikely.Z\0
       *                   79215  \0
       *
       * mirror.pl says:
       * filename:  ^(\S*)\s+
       * size:      (\-|\=|\d+)\s+
       * month/day: ((\w\w\w\s+\d+|\d+\s+\w\w\w)\s+
       * time/year: (\d+:\d+|\d\d\d\d))\s+
       * rest:      (.+) 
       *
       * README              763  Jul 11 21:05  Information about this server
       * bin/                  -  Apr 28  1994
       * etc/                  =  11 Jul 21:04
       * ls-lR                 0   6 Aug 17:14
       * ls-lR.Z               3  05 Sep 1994
       * pub/                  =  Jul 11 21:04  Public area
       * usr/                  -  Sep  7 09:39
       * morgan               14  Apr 18 09:39  -> ../real/morgan
       * TIMIT.mostlikely.Z
       *                   79215  Jul 11 21:04
      */
      if (!state->lstyle && line[linelen-1] == ':' && 
          linelen >= 2 && toklen[numtoks-1] != 1)
      { 
        /* code in mirror.pl suggests that a listing may be preceded
         * by a PWD line in the form "/some/dir/names/here:"
         * but does not necessarily begin with '/'. *sigh*
        */
        pos = 0;
        p = line;
        while (pos < (linelen-1))
        {
          /* illegal (or extremely unusual) chars in a dirspec */
          if (*p == '<' || *p == '|' || *p == '>' ||
              *p == '?' || *p == '*' || *p == '\\')
            break;
          if (*p == '/' && pos < (linelen-2) && p[1] == '/')
            break;
          pos++;
          p++;
        }
        if (pos == (linelen-1))
        {
          state->lstyle = 'D';
          return '?';
        }
      }

      if (!lstyle && numtoks >= 2)
      {
        pos = 22; /* pos of (\d+|-|=) if this is not part of a multiline */
        if (state->lstyle && carry_buf_len) /* first is from previous line */
          pos = toklen[1]-1; /* and is 'as-is' (may contain whitespace) */

        if (linelen > pos)
        {
          p = &line[pos];
          if ((*p == '-' || *p == '=' || isdigit(*p)) &&
              ((linelen == (pos+1)) || 
               (linelen >= (pos+3) && p[1] == ' ' && p[2] == ' ')) )
          {
            tokmarker = 1;
            if (!carry_buf_len)
            {
              pos = 1;
              while (pos < numtoks && (tokens[pos]+toklen[pos]) < (&line[23]))
                pos++;
              tokmarker = 0;
              if ((tokens[pos]+toklen[pos]) == (&line[23]))
                tokmarker = pos;
            }
            if (tokmarker)  
            {
              lstyle = 'D';
              if (*tokens[tokmarker] == '-' || *tokens[tokmarker] == '=')
              {
                if (toklen[tokmarker] != 1 ||
                   (tokens[tokmarker-1][toklen[tokmarker-1]-1]) != '/')
                  lstyle = 0;
              }              
              else
              {
                for (pos = 0; lstyle && pos < toklen[tokmarker]; pos++) 
                {
                  if (!isdigit(tokens[tokmarker][pos]))
                    lstyle = 0; 
                }
              }
              if (lstyle && !state->lstyle) /* first time */
              {
                /* scan for illegal (or incredibly unusual) chars in fname */
                for (p = tokens[0]; lstyle &&
                     p < &(tokens[tokmarker-1][toklen[tokmarker-1]]); p++)
                {
                  if (*p == '<' || *p == '|' || *p == '>' || 
                      *p == '?' || *p == '*' || *p == '/' || *p == '\\')
                    lstyle = 0;
                }
              }

            } /* size token found */
          } /* expected chars behind expected size token */
        } /* if (linelen > pos) */
      } /* if (!lstyle && numtoks >= 2) */

      if (!lstyle && state->lstyle == 'D' && !carry_buf_len)
      {
        /* the filename of a multi-line entry can be identified
         * correctly only if dls format had been previously established.
         * This should always be true because there should be entries
         * for '.' and/or '..' and/or CWD that precede the rest of the
         * listing.
        */
        pos = linelen;
        if (pos > (sizeof(state->carry_buf)-1))
          pos = sizeof(state->carry_buf)-1;
        memcpy( state->carry_buf, line, pos );
        state->carry_buf_len = pos;
        return '?';
      }

      if (lstyle == 'D')
      {
        state->parsed_one = 1;
        state->lstyle = lstyle;

        p = &(tokens[tokmarker-1][toklen[tokmarker-1]]);
        result->fe_fname = tokens[0];
        result->fe_fnlen = p - tokens[0];
        result->fe_type  = 'f';

        if (result->fe_fname[result->fe_fnlen-1] == '/')
        {
          if (result->fe_lnlen == 1)
            result->fe_type = '?';
          else
          {
            result->fe_fnlen--;
            result->fe_type  = 'd';
          }
        }
        else if (isdigit(*tokens[tokmarker]))
        {
          pos = toklen[tokmarker];
          if (pos > (sizeof(result->fe_size)-1))
            pos = sizeof(result->fe_size)-1;
          memcpy( result->fe_size, tokens[tokmarker], pos );
          result->fe_size[pos] = '\0';
        }

        if ((tokmarker+3) < numtoks && 
              (&(tokens[numtoks-1][toklen[numtoks-1]]) - 
               tokens[tokmarker+1]) >= (1+1+3+1+4) )
        {
          pos = (tokmarker+3);
          p = tokens[pos];
          pos = toklen[pos];

          if ((pos == 4 || pos == 5)
          &&  isdigit(*p) && isdigit(p[pos-1]) && isdigit(p[pos-2])
          &&  ((pos == 5 && p[2] == ':') ||  
               (pos == 4 && (isdigit(p[1]) || p[1] == ':')))
             )
          {
            month_num = tokmarker+1; /* assumed position of month field */
            pos = tokmarker+2;       /* assumed position of mday field */
            if (isdigit(*tokens[month_num])) /* positions are reversed */
            {
              month_num++;
              pos--;
            }
            p = tokens[month_num];
            if (isdigit(*tokens[pos]) 
            && (toklen[pos] == 1 || 
                  (toklen[pos] == 2 && isdigit(tokens[pos][1])))
            && toklen[month_num] == 3
            && isalpha(*p) && isalpha(p[1]) && isalpha(p[2])  )
            {
              pos = atoi(tokens[pos]);
              if (pos > 0 && pos <= 31)
              {
                result->fe_time.tm_mday = pos;
                month_num = 1;
                for (pos = 0; pos < (12*3); pos+=3)
                {
                  if (p[0] == month_names[pos+0] &&
                      p[1] == month_names[pos+1] &&
                      p[2] == month_names[pos+2])
                    break;
                  month_num++;
                }
                if (month_num > 12)
                  result->fe_time.tm_mday = 0;
                else
                  result->fe_time.tm_month = month_num - 1;
              }
            }
            if (result->fe_time.tm_mday)
            {
              tokmarker += 3; /* skip mday/mon/yrtime (to find " -> ") */
              p = tokens[tokmarker];

              pos = atoi(p);
              if (pos > 24)
                result->fe_time.tm_year = pos-1900;
              else
              {
                if (p[1] == ':')
                  p--;
                result->fe_time.tm_hour = pos;
                result->fe_time.tm_min = atoi(p+3);
                if (!state->now_time)
                {
                  state->now_time = PR_Now();
                  PR_ExplodeTime((state->now_time), PR_LocalTimeParameters, &(state->now_tm) );
                }
                result->fe_time.tm_year = state->now_tm.tm_year;
                if ( (( state->now_tm.tm_month  << 4) + state->now_tm.tm_mday) <
                     ((result->fe_time.tm_month << 4) + result->fe_time.tm_mday) )
                  result->fe_time.tm_year--;
              } /* got year or time */
            } /* got month/mday */
          } /* may have year or time */
        } /* enough remaining to possibly have date/time */

        if (numtoks > (tokmarker+2))
        {
          pos = tokmarker+1;
          p = tokens[pos];
          if (toklen[pos] == 2 && *p == '-' && p[1] == '>')
          {
            p = &(tokens[numtoks-1][toklen[numtoks-1]]);
            result->fe_type  = 'l';
            result->fe_lname = tokens[pos+1];
            result->fe_lnlen = p - result->fe_lname;
            if (result->fe_lnlen > 1 &&
                result->fe_lname[result->fe_lnlen-1] == '/')
              result->fe_lnlen--;
          }
        } /* if (numtoks > (tokmarker+2)) */

        /* the caller should do this (if dropping "." and ".." is desired)
        if (result->fe_type == 'd' && result->fe_fname[0] == '.' &&
            (result->fe_fnlen == 1 || (result->fe_fnlen == 2 &&
                                      result->fe_fname[1] == '.')))
          return '?';
        */

        return result->fe_type;

      } /* if (lstyle == 'D') */
    } /* if (!lstyle && (!state->lstyle || state->lstyle == 'D')) */
#endif

    /* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

  } /* if (linelen > 0) */

  return ParsingFailed(state);
}

/* ==================================================================== */
/* standalone testing                                                   */
/* ==================================================================== */
#if 0

#include <stdio.h>

static int do_it(FILE *outfile, 
                 char *line, size_t linelen, struct list_state *state,
                 char **cmnt_buf, unsigned int *cmnt_buf_sz,
                 char **list_buf, unsigned int *list_buf_sz )
{
  struct list_result result;
  char *p;
  int rc;

  rc = ParseFTPList( line, state, &result );

  if (!outfile)
  {
    outfile = stdout;
    if (rc == '?')
      fprintf(outfile, "junk: %.*s\n", (int)linelen, line );
    else if (rc == '"')
      fprintf(outfile, "cmnt: %.*s\n", (int)linelen, line );
    else
      fprintf(outfile, 
              "list: %02u-%02u-%02u  %02u:%02u%cM %20s %.*s%s%.*s\n",
              (result.fe_time.tm_mday ? (result.fe_time.tm_month + 1) : 0),
              result.fe_time.tm_mday,
              (result.fe_time.tm_mday ? (result.fe_time.tm_year % 100) : 0),
              result.fe_time.tm_hour - 
                ((result.fe_time.tm_hour > 12)?(12):(0)),
              result.fe_time.tm_min,
              ((result.fe_time.tm_hour >= 12) ? 'P' : 'A'),
              (rc == 'd' ? "<DIR>         " : 
              (rc == 'l' ? "<JUNCTION>    " : result.fe_size)),
              (int)result.fe_fnlen, result.fe_fname,
              ((rc == 'l' && result.fe_lnlen) ? " -> " : ""),
              (int)((rc == 'l' && result.fe_lnlen) ? result.fe_lnlen : 0),
              ((rc == 'l' && result.fe_lnlen) ? result.fe_lname : "") );
  }
  else if (rc != '?') /* NOT junk */
  { 
    char **bufp = list_buf;
    unsigned int *bufz = list_buf_sz;
    
    if (rc == '"') /* comment - make it a 'result' */
    {
      memset( &result, 0, sizeof(result));
      result.fe_fname = line;
      result.fe_fnlen = linelen;
      result.fe_type = 'f';
      if (line[linelen-1] == '/')
      {
        result.fe_type = 'd';
        result.fe_fnlen--; 
      }
      bufp = cmnt_buf;
      bufz = cmnt_buf_sz;
      rc = result.fe_type;
    }  

    linelen = 80 + result.fe_fnlen + result.fe_lnlen;
    p = (char *)realloc( *bufp, *bufz + linelen );
    if (!p)
      return -1;
    sprintf( &p[*bufz], 
             "%02u-%02u-%04u  %02u:%02u:%02u %20s %.*s%s%.*s\n",
              (result.fe_time.tm_mday ? (result.fe_time.tm_month + 1) : 0),
              result.fe_time.tm_mday,
              (result.fe_time.tm_mday ? (result.fe_time.tm_year + 1900) : 0),
              result.fe_time.tm_hour,
              result.fe_time.tm_min,
              result.fe_time.tm_sec,
              (rc == 'd' ? "<DIR>         " : 
              (rc == 'l' ? "<JUNCTION>    " : result.fe_size)),
              (int)result.fe_fnlen, result.fe_fname,
              ((rc == 'l' && result.fe_lnlen) ? " -> " : ""),
              (int)((rc == 'l' && result.fe_lnlen) ? result.fe_lnlen : 0),
              ((rc == 'l' && result.fe_lnlen) ? result.fe_lname : "") );
    linelen = strlen(&p[*bufz]);
    *bufp = p;
    *bufz = *bufz + linelen;
  }
  return 0;
}

int main(int argc, char *argv[])
{
  FILE *infile = (FILE *)0;
  FILE *outfile = (FILE *)0;
  int need_close_in = 0;
  int need_close_out = 0;

  if (argc > 1)
  {
    infile = stdin;
    if (strcmp(argv[1], "-") == 0)
      need_close_in = 0;
    else if ((infile = fopen(argv[1], "r")) != ((FILE *)0))
      need_close_in = 1;
    else
      fprintf(stderr, "Unable to open input file '%s'\n", argv[1]);
  }
  if (infile && argc > 2)
  {
    outfile = stdout;
    if (strcmp(argv[2], "-") == 0)
      need_close_out = 0;
    else if ((outfile = fopen(argv[2], "w")) != ((FILE *)0))
      need_close_out = 1;
    else
    {
      fprintf(stderr, "Unable to open output file '%s'\n", argv[2]);
      fclose(infile);
      infile = (FILE *)0;
    }
  }

  if (!infile)
  {
    char *appname = &(argv[0][strlen(argv[0])]);
    while (appname > argv[0])
    {
      appname--;
      if (*appname == '/' || *appname == '\\' || *appname == ':')
      {
        appname++;
        break;
      } 
    }
    fprintf(stderr, 
        "Usage: %s <inputfilename> [<outputfilename>]\n"
        "\nIf an outout file is specified the results will be"
        "\nbe post-processed, and only the file entries will appear"
        "\n(or all comments if there are no file entries)."
        "\nNot specifying an output file causes %s to run in \"debug\""
        "\nmode, ie results are printed as lines are parsed."
        "\nIf a filename is a single dash ('-'), stdin/stdout is used."
        "\n", appname, appname );
  }
  else
  {
    char *cmnt_buf = (char *)0;
    unsigned int cmnt_buf_sz = 0;
    char *list_buf = (char *)0;
    unsigned int list_buf_sz = 0;

    struct list_state state;
    char line[512];

    memset( &state, 0, sizeof(state) );
    while (fgets(line, sizeof(line), infile))
    {
      size_t linelen = strlen(line);
      if (linelen < (sizeof(line)-1))
      {
        if (linelen > 0 && line[linelen-1] == '\n')
          linelen--;
        if (do_it( outfile, line, linelen, &state, 
                   &cmnt_buf, &cmnt_buf_sz, &list_buf, &list_buf_sz) != 0)
        {
          fprintf(stderr, "Insufficient memory. Listing may be incomplete.\n"); 
          break;
        }
      }
      else
      {
        /* no '\n' found. drop this and everything up to the next '\n' */
        fprintf(stderr, "drop: %.*s", (int)linelen, line );
        while (linelen == sizeof(line))
        {
          if (!fgets(line, sizeof(line), infile))
            break;
          linelen = 0;
          while (linelen < sizeof(line) && line[linelen] != '\n')
            linelen++;
          fprintf(stderr, "%.*s", (int)linelen, line );
        }  
        fprintf(stderr, "\n");
      }
    }
    if (outfile)
    {
      if (list_buf)
        fwrite( list_buf, 1, list_buf_sz, outfile );
      else if (cmnt_buf)
        fwrite( cmnt_buf, 1, cmnt_buf_sz, outfile );
    }
    if (list_buf) 
      free(list_buf);
    if (cmnt_buf)
      free(cmnt_buf);

    if (need_close_in)
      fclose(infile);
    if (outfile && need_close_out)
      fclose(outfile);
  }

  return 0;
}
#endif
