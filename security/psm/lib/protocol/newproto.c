/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#include <string.h>
#include <assert.h>
#ifdef WIN32
#include <winsock.h>
#endif
#ifdef XP_MAC
#include "macsocket.h"
#endif
#ifdef XP_UNIX
#include "sys/types.h"
#include "netinet/in.h" /* for ntohl & htonl */
#endif
#ifdef XP_OS2_EMX
#include <sys/param.h>
#endif
#ifdef XP_OS2_VACPP
#include <utils.h>
#endif
#ifdef XP_BEOS
#include "ByteOrder.h"
#endif

#include "newproto.h"

char SSMVersionString[] = "1.5";

CMT_Alloc_fn cmt_alloc = malloc;
CMT_Free_fn cmt_free = free;

#define ASSERT(x) assert(x)

#define CM_ntohl ntohl
#define CM_htonl htonl

char* SSM_GetVersionString(void)
{
    char *newString;
    int strLen;

    strLen = strlen(SSMVersionString);
    newString = (char*)malloc(sizeof(char)*(strLen+1));
    if (newString) {
        memcpy(newString,SSMVersionString, strlen(SSMVersionString));
        newString[strLen]='\0';
    }
    return newString;
}


/*************************************************************
 *
 * CMT_Init
 *
 *
 ************************************************************/
void
CMT_Init(CMT_Alloc_fn allocfn, CMT_Free_fn freefn)
{
    cmt_alloc = allocfn;
    cmt_free = freefn;
}

static CMTStatus
decode_int(unsigned char **curptr, void *dest, CMInt32 *remaining)
{
    CMInt32 datalen = sizeof(CMInt32);

    if (*remaining < datalen)
        return CMTFailure;
    *(CMInt32 *)dest = ntohl(**(CMInt32 **)curptr);
    *remaining -= datalen;
    *curptr += datalen;
    return CMTSuccess;
}

static CMTStatus
decode_string(unsigned char **curptr, CMInt32 *len,
              unsigned char **data, CMInt32 *remaining)
{
    CMTStatus rv;
    CMInt32 datalen;

    rv = decode_int(curptr, len, remaining);
    if (rv != CMTSuccess)
        return CMTFailure;

    /* NULL string */
    if (*len == 0) {
        *data = NULL;
        goto done;
    }

    datalen = (*len + 3) & ~3;
    if (*remaining < datalen)
        return CMTFailure;

    *data = (unsigned char *) cmt_alloc(*len + 1);
    if (*data == NULL)
        return CMTFailure;

    memcpy(*data, *curptr, *len);
    (*data)[*len] = 0;
    *remaining -= datalen;
    *curptr += datalen;
done:
    return CMTSuccess;
}

/*************************************************************
 * CMT_DecodeMessage
 *
 * Decode msg into dest as specified by tmpl.
 *
 ************************************************************/
CMTStatus
CMT_DecodeMessage(CMTMessageTemplate *tmpl, void *dest, CMTItem *msg)
{
    unsigned char *curptr, *destptr, *list;
	void ** ptr;
    CMInt32 remaining, len, choiceID = 0, listSize, listCount = 0;
    CMBool inChoice = CM_FALSE, foundChoice = CM_FALSE, inList = CM_FALSE;
    CMInt32 listItemSize = 0;
    CMTStatus rv = CMTSuccess;
	CMTMessageTemplate *startOfList, *p;
	CMBool inStructList = CM_FALSE;

    curptr = msg->data;
    remaining = msg->len;

    while(tmpl->type != CMT_DT_END) {
        /* XXX Maybe this should be a more formal state machine? */
        if (inChoice) {
            if (tmpl->type == CMT_DT_END_CHOICE) {
                if (!foundChoice)
                    goto loser;
                inChoice = CM_FALSE;
                foundChoice = CM_FALSE;
                tmpl++;
                continue;
            }
            if (choiceID != tmpl->choiceID) {
                tmpl++;
                continue;  /* Not this option */
            } else {
                foundChoice = CM_TRUE;
            }
        }
        if (inList) {
            destptr = &list[listCount * listItemSize];
            listCount++;
        } else {
			if (inStructList) {
				destptr = tmpl->offset + list;
			} else {
				destptr = tmpl->offset + (unsigned char *)dest;
			}
        }
        switch (tmpl->type) {
          case CMT_DT_RID:
          case CMT_DT_INT:
          case CMT_DT_BOOL:
            rv = decode_int(&curptr, destptr, &remaining);
            if (rv != CMTSuccess)
                goto loser;
            break;
          case CMT_DT_STRING:
            rv = decode_string(&curptr, &len, (unsigned char **)destptr,
                               &remaining);
            if (rv != CMTSuccess)
                goto loser;
            break;
          case CMT_DT_ITEM:
            rv = decode_string(&curptr, (long *) &((CMTItem *)destptr)->len,
                               &((CMTItem *)destptr)->data, &remaining);
            if (rv != CMTSuccess)
                goto loser;
            break;
          case CMT_DT_LIST:
            /* XXX This is too complicated */
            rv = decode_int(&curptr, destptr, &remaining);
            if (rv != CMTSuccess)
                goto loser;
            listSize = *(CMInt32 *)destptr;
            tmpl++;
            if (tmpl->type == CMT_DT_STRING) {
                listItemSize = sizeof(unsigned char *);
            } else if (tmpl->type == CMT_DT_ITEM) {
                listItemSize = sizeof(CMTItem);
            } else {
                listItemSize = sizeof(CMInt32);
            }
            if (listSize == 0) {
                list = NULL;
            } else {
                list = (unsigned char *) cmt_alloc(listSize * listItemSize);
            }
            *(void **)(tmpl->offset + (unsigned char *)dest) = list;
            inList = CM_TRUE;
            listCount = 0;
            break;
          case CMT_DT_STRUCT_LIST:
            /* XXX This is too complicated */
            rv = decode_int(&curptr, destptr, &remaining);
            if (rv != CMTSuccess)
                goto loser;
            listSize = *(CMInt32 *)destptr;
            tmpl++;
			if (tmpl->type != CMT_DT_STRUCT_PTR) {
				goto loser;
			}
			ptr = (void**)(tmpl->offset + (unsigned char *)dest);
			startOfList = tmpl;
			p = tmpl;
			listItemSize = 0;
			while (p->type != CMT_DT_END_STRUCT_LIST) {
				if (p->type == CMT_DT_STRING) {
					listItemSize += sizeof(unsigned char *);
				} else if (p->type == CMT_DT_ITEM) {
					listItemSize += sizeof(CMTItem);
				} else if (p->type == CMT_DT_INT) {
	                listItemSize += sizeof(CMInt32);
		        }
				p++;
			}
            if (listSize == 0) {
                list = NULL;
            } else {
                list = (unsigned char *) cmt_alloc(listSize * listItemSize);
            }
            *ptr = list;
            inStructList = CM_TRUE;
            listCount = 0;
            break;
		  case CMT_DT_END_STRUCT_LIST:
			listCount++;
			if (listCount == listSize) {
				inStructList = CM_FALSE;
			} else {
				list += listItemSize;
				tmpl = startOfList;
			}
			break;
          case CMT_DT_CHOICE:
            rv = decode_int(&curptr, destptr, &remaining);
            if (rv != CMTSuccess)
                goto loser;
            choiceID = *(CMInt32 *)destptr;
            inChoice = CM_TRUE;
            foundChoice = CM_FALSE;
            break;
          case CMT_DT_END_CHOICE: /* Loop should exit before we see these. */
          case CMT_DT_END:
          default:
            ASSERT(0);
            break;
        }
        if (inList) {
            if (listCount == listSize) {
                inList = CM_FALSE;
                tmpl++;
            }
        } else {
            tmpl++;
        }
    }
loser:
    /* Free the data buffer */
    if (msg->data) {
        cmt_free(msg->data);
        msg->data = NULL;
    }
    return rv;
}

static CMTStatus
calc_msg_len(CMTMessageTemplate *tmpl, void *src, CMInt32 *len_out)
{
    CMInt32 len = 0, choiceID = 0, listSize, listItemSize, listCount;
    unsigned char *srcptr, *list;
    CMBool inChoice = CM_FALSE, inList = CM_FALSE, foundChoice = CM_FALSE;
	CMTMessageTemplate *startOfList, *p;
	CMBool inStructList = CM_FALSE;

    while(tmpl->type != CMT_DT_END) {
        if (inChoice) {
            if (tmpl->type == CMT_DT_END_CHOICE) {
                if (!foundChoice)
                    goto loser;
                inChoice = CM_FALSE;
                foundChoice = CM_FALSE;
                tmpl++;
                continue;
            }
            if (choiceID != tmpl->choiceID) {
                tmpl++;
                continue;  /* Not this option */
            } else {
                foundChoice = CM_TRUE;
            }
        }
        if (inList) {
            srcptr = &list[listCount * listItemSize];
            listCount++;
        } else if (inStructList) {
			srcptr = tmpl->offset + list;
		} else {
            srcptr = tmpl->offset + (unsigned char *)src;
        }
        switch(tmpl->type) {
          case CMT_DT_RID:
          case CMT_DT_INT:
          case CMT_DT_BOOL:
            len += sizeof(CMInt32);
            break;
          case CMT_DT_STRING:
            len += sizeof(CMInt32);
            /* Non NULL string */
            if (*(char**)srcptr) {
                len += (strlen(*(char**)srcptr) + 4) & ~3;
            }
            break;
          case CMT_DT_ITEM:
            len += sizeof(CMInt32);
            len += (((CMTItem *)srcptr)->len + 3) & ~3;
            break;
          case CMT_DT_LIST:
            len += sizeof(CMInt32);
            listSize = *(CMInt32 *)srcptr;
            tmpl++;
            if (tmpl->type == CMT_DT_STRING) {
                listItemSize = sizeof(unsigned char *);
            } else if (tmpl->type == CMT_DT_ITEM) {
                listItemSize = sizeof(CMTItem);
            } else {
                listItemSize = sizeof(CMInt32);
            }
            list = *(unsigned char **)(tmpl->offset + (unsigned char *)src);
            listCount = 0;
            inList = CM_TRUE;
            break;
		  case CMT_DT_STRUCT_LIST:
			len += sizeof(CMInt32);
			listSize = *(CMInt32 *)srcptr;
			tmpl++;
			if (tmpl->type != CMT_DT_STRUCT_PTR) {
				goto loser;
			}
			list = *(unsigned char**)(tmpl->offset + (unsigned char*)src);
			startOfList = tmpl;
			p = tmpl;
			listItemSize = 0;
			while (p->type != CMT_DT_END_STRUCT_LIST) {
				if (p->type == CMT_DT_STRING) {
					listItemSize += sizeof(unsigned char *);
				} else if (p->type == CMT_DT_ITEM) {
					listItemSize += sizeof(CMTItem);
				} else if (p->type == CMT_DT_INT) {
					listItemSize += sizeof(CMInt32);
				}
				p++;
			}
			listCount = 0;
			inStructList = CM_TRUE;
			break;
		  case CMT_DT_END_STRUCT_LIST:
			listCount++;
			if (listCount == listSize) {
				inStructList = CM_FALSE;
			} else {
				list += listItemSize;
				tmpl = startOfList;
			}
			break;
          case CMT_DT_CHOICE:
            len += sizeof(CMInt32);
            choiceID = *(CMInt32 *)srcptr;
            inChoice = CM_TRUE;
            foundChoice = CM_FALSE;
            break;
          case CMT_DT_END_CHOICE: /* Loop should exit before we see these. */
          case CMT_DT_END:
          default:
            ASSERT(0);
            break;
        }
        if (inList) {
            if (listCount == listSize) {
                inList = CM_FALSE;
                tmpl++;
            }
        } else {
            tmpl++;
        }
    }
    *len_out = len;
    return CMTSuccess;
loser:
    return CMTFailure;
}

static CMTStatus
encode_int(unsigned char **curptr, void *src, CMInt32 *remaining)
{
    CMInt32 datalen = sizeof(CMInt32);

    if (*remaining < datalen)
        return CMTFailure;
    **(CMInt32 **)curptr = CM_htonl(*(CMInt32 *)src);
    *remaining -= datalen;
    *curptr += datalen;
    return CMTSuccess;
}

static CMTStatus
encode_string(unsigned char **curptr, CMInt32 len,
              unsigned char *data, CMInt32 *remaining)
{
    CMTStatus rv;
    CMInt32 datalen;

    rv = encode_int(curptr, &len, remaining);
    if (rv != CMTSuccess)
        return CMTFailure;

    /* NULL string */
    if (len == 0) {
        goto done;
    }

    datalen = (len + 3) & ~3;
    if (*remaining < datalen)
        return CMTFailure;

    memcpy(*curptr, data, len);
    *remaining -= datalen;
    *curptr += datalen;
done:
    return CMTSuccess;
}

/*************************************************************
 * CMT_EncodeMessage
 *
 * Encode src into msg as specified by tmpl.
 *
 ************************************************************/
CMTStatus
CMT_EncodeMessage(CMTMessageTemplate *tmpl, CMTItem *msg, void *src)
{
    CMInt32 choiceID = 0, listSize, listItemSize, listCount, remaining;
    unsigned char *srcptr, *curptr, *list;
    CMBool inChoice = CM_FALSE, inList = CM_FALSE, foundChoice = CM_FALSE;
    CMTStatus rv = CMTSuccess;
	CMTMessageTemplate *startOfList, *p;
	CMBool inStructList = CM_FALSE;

    rv = calc_msg_len(tmpl, src, (long *) &msg->len);
    if (rv != CMTSuccess)
        goto loser;
    curptr = msg->data = (unsigned char *) cmt_alloc(msg->len);
    if(msg->data == NULL)
        goto loser;
    remaining = msg->len;

    while(tmpl->type != CMT_DT_END) {
        if (inChoice) {
            if (tmpl->type == CMT_DT_END_CHOICE) {
                if (!foundChoice)
                    goto loser;
                inChoice = CM_FALSE;
                foundChoice = CM_FALSE;
                tmpl++;
                continue;
            }
            if (choiceID != tmpl->choiceID) {
                tmpl++;
                continue;  /* Not this option */
            } else {
                foundChoice = CM_TRUE;
            }
        }
        if (inList) {
            srcptr = &list[listCount * listItemSize];
            listCount++;
        } else {
			if (inStructList) {
				srcptr = tmpl->offset + list;
			} else {
				srcptr = tmpl->offset + (unsigned char *)src;
			}
        }
        switch(tmpl->type) {
          case CMT_DT_RID:
          case CMT_DT_INT:
          case CMT_DT_BOOL:
            rv = encode_int(&curptr, srcptr, &remaining);
            if (rv != CMTSuccess)
                goto loser;
            break;
          case CMT_DT_STRING:
             if (*(char**)srcptr) {
                 /* Non NULL string */
                 rv = encode_string(&curptr, (long) strlen(*(char**)srcptr), 
                 	                *(unsigned char**)srcptr, &remaining);
             } else {
                 /* NULL string */
                 rv = encode_string(&curptr, 0L, *(unsigned char**)srcptr, &remaining);
             }
             if (rv != CMTSuccess)
                goto loser;
             break;
          case CMT_DT_ITEM:
            rv = encode_string(&curptr, ((CMTItem *)srcptr)->len,
                               ((CMTItem *)srcptr)->data, &remaining);
            if (rv != CMTSuccess)
                goto loser;
            break;
          case CMT_DT_LIST:
            rv = encode_int(&curptr, srcptr, &remaining);
            if (rv != CMTSuccess)
                goto loser;
            listSize = *(CMInt32 *)srcptr;
            tmpl++;
            if (tmpl->type == CMT_DT_STRING) {
                listItemSize = sizeof(unsigned char *);
            } else if (tmpl->type == CMT_DT_ITEM) {
                listItemSize = sizeof(CMTItem);
            } else {
                listItemSize = sizeof(CMInt32);
            }
            list = *(unsigned char **)(tmpl->offset + (unsigned char *)src);
            listCount = 0;
            inList = CM_TRUE;
            break;
		  case CMT_DT_STRUCT_LIST:
            rv = encode_int(&curptr, srcptr, &remaining);
            if (rv != CMTSuccess)
                goto loser;
            listSize = *(CMInt32 *)srcptr;
			tmpl++;
			if (tmpl->type != CMT_DT_STRUCT_PTR) {
				goto loser;
			}
			list = *(unsigned char**)(tmpl->offset + (unsigned char*)src);
			startOfList = tmpl;
			p = tmpl;
			listItemSize = 0;
			while (p->type != CMT_DT_END_STRUCT_LIST) {
				if (p->type == CMT_DT_STRING) {
					listItemSize += sizeof(unsigned char *);
				} else if (p->type == CMT_DT_ITEM) {
					listItemSize += sizeof(CMTItem);
				} else if (p->type == CMT_DT_INT) {
					listItemSize += sizeof(CMInt32);
				}
				p++;
			}
			listCount = 0;
			inStructList = CM_TRUE;
			break;
		  case CMT_DT_END_STRUCT_LIST:
			listCount++;
			if (listCount == listSize) {
				inStructList = CM_FALSE;
			} else {
				list += listItemSize;
				tmpl = startOfList;
			}
			break;
          case CMT_DT_CHOICE:
            rv = encode_int(&curptr, srcptr, &remaining);
            if (rv != CMTSuccess)
                goto loser;
            choiceID = *(CMInt32 *)srcptr;
            inChoice = CM_TRUE;
            foundChoice = CM_FALSE;
            break;
          case CMT_DT_END_CHOICE: /* Loop should exit before we see these. */
          case CMT_DT_END:
          default:
            ASSERT(0);
            break;
        }
        if (inList) {
            if (listCount == listSize) {
                inList = CM_FALSE;
                tmpl++;
            }
        } else {
            tmpl++;
        }
    }
    return CMTSuccess;
loser:
    return CMTFailure;
}
