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

/* 
   This file implements local store support for the rdf data model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

#include "nlcstore.h"
#include "glue.h"
#include "mcf.h"
#include "xpassert.h"

#include "utils.h"

	/* globals */
PRBool doingFirstTimeInitp = 0;
RDFT gOPSStore = 0;
RDFT gLocalStore = 0;


	/* externs */
extern char *profileDirURL;
extern char *gBookmarkURL;



/* 

To do : killing a unit

*/



PRBool
compareUnalignedUINT32Ptrs(void *v1, void *v2)
{
	uint32		val1, val2;

	memcpy(&val1,v1,sizeof(uint32));
	memcpy(&val2,v2,sizeof(uint32));
	return((val1==val2) ? PR_TRUE:PR_FALSE);
}



char *
makeRDFDBURL(char* directory, char* name)
{
  char		*ans;
  size_t		s;
  
  if (profileDirURL == NULL) return NULL;
  if ((ans = (char*) getMem(strlen(profileDirURL) + strlen(directory) + strlen(name) + 3)) != NULL) {
    s = strlen(profileDirURL);
    memcpy(ans, profileDirURL, s);
    if (ans[s-1] != '/') {
      ans[s++] = '/';
    }
    stringAppend(ans, directory);
    stringAppend(ans, "/");
    stringAppend(ans, name);
  }
  return(ans);
}



void
readInBookmarksOnInit(RDFFile f)
{
  /* RDF_Resource ptFolder; */
  PRFileDesc *fp;
  int32 len;
  char buf[512];
  fp = CallPROpenUsingFileURL(f->url, PR_RDONLY|PR_CREATE_FILE, 0644);
  if (fp == NULL) return;
  while((len=PR_Read(fp, buf, sizeof(buf))) >0) {
    parseNextBkBlob(f, buf, len);
  }

  if (f->db == gLocalStore)
  {
	/* if no personal toolbar was specified in bookmark file, create one */

	/*
	ptFolder = nlocalStoreGetSlotValue(f->db, gNavCenter->RDF_PersonalToolbarFolderCategory,
	gCoreVocab->RDF_instanceOf, RDF_RESOURCE_TYPE, true, true);

	if (ptFolder == NULL)
	{
	if ((ptFolder = createContainer("personaltoolbar.rdf")) != NULL)
	{
	addSlotValue(f, ptFolder, gCoreVocab->RDF_parent,
		gNavCenter->RDF_BookmarkFolderCategory,
		RDF_RESOURCE_TYPE, true);
	addSlotValue(f, ptFolder, gCoreVocab->RDF_name,
		copyString(XP_GetString(RDF_PERSONAL_TOOLBAR_NAME)),
		RDF_STRING_TYPE, true );
	RDFUtil_SetPTFolder(ptFolder);
	}
	}
	*/
  }
  PR_Close(fp);
#if 0
  if (f->line != NULL)		freeMem(f->line);
  if (f->currentSlot != NULL)	freeMem(f->currentSlot);
  if (f->holdOver != NULL)	freeMem(f->holdOver);
#endif
}


void
DBM_OpenDBMStore (DBMRDF store, char* directory)
{
  HASHINFO hash_info = {128, 0, 0, 0, 0, 0};  
  PRBool createp = 0;
  char* dbPathname;
  char* dirPathname;
  int mode=(O_RDWR | O_CREAT);

  CHECK_VAR1(profileDirURL);
  dirPathname = makeDBURL(directory);
  CallPRMkDirUsingFileURL(dirPathname, 00700);  
  freeMem(dirPathname);

  dbPathname =  makeRDFDBURL(directory, "names.db");
  CHECK_VAR1(dbPathname);
  store->nameDB    = CallDBOpenUsingFileURL(dbPathname,
  			O_RDWR, 0644, DB_HASH, &hash_info);
  if (store->nameDB == NULL) {
    createp = 1;
    mode = (O_RDWR | O_CREAT | O_TRUNC);
    store->nameDB    = CallDBOpenUsingFileURL(dbPathname,
    			mode, 0644, DB_HASH, &hash_info);
  }
  freeMem(dbPathname);
  CHECK_VAR1(store->nameDB);
  dbPathname = makeRDFDBURL(directory, "child.db");
  CHECK_VAR1(dbPathname);
  hash_info.bsize = 2056;
  store->childrenDB    = CallDBOpenUsingFileURL(dbPathname, 
			 mode, 0644, DB_HASH, &hash_info);
  freeMem(dbPathname);
  CHECK_VAR1(store->childrenDB);

  dbPathname = makeRDFDBURL(directory, "lstr.db");
  hash_info.bsize = 1024  ;
  store->propDB   = CallDBOpenUsingFileURL(dbPathname,
		    mode, 0644, DB_HASH, &hash_info);
  freeMem(dbPathname);
  CHECK_VAR1(store->propDB);

  dbPathname =  makeRDFDBURL(directory, "ilstr.db");
  CHECK_VAR1(dbPathname);
  hash_info.bsize = 1024*16;
  store->invPropDB   = CallDBOpenUsingFileURL(dbPathname,
		       mode, 0644, DB_HASH, &hash_info);
  freeMem(dbPathname);
  CHECK_VAR1(store->invPropDB);
  
  if (strcmp(directory, "NavCen") == 0) {
    RDF_Resource bmk = RDF_GetResource(NULL, "NC:Bookmarks", true);
    if (createp) {
      RDFFile newFile;
      doingFirstTimeInitp = 1;
      newFile = makeRDFFile(gBookmarkURL, bmk, true);
      newFile->fileType = RDF_BOOKMARKS;
      newFile->db = gLocalStore;
      newFile->assert = nlocalStoreAssert1;
      readInBookmarksOnInit(newFile);

      doingFirstTimeInitp = 0;
      (*store->propDB->sync)(store->propDB, 0);
      (*store->invPropDB->sync)(store->invPropDB, 0);
      (*store->nameDB->sync)(store->nameDB, 0);
      (*store->childrenDB->sync)(store->childrenDB, 0); 
    } 
  }
}



RDF_Error
DBM_CloseRDFDBMStore (RDFT r)
{
  
  DBMRDF db = (DBMRDF)r->pdata;
   if (r->rdf) return 0;
  if (db->nameDB != NULL)		(*db->nameDB->close)(db->nameDB);
  if (db->childrenDB != NULL)	(*db->childrenDB->close)(db->childrenDB);
  if (db->propDB != NULL)		(*db->propDB->close)(db->propDB);
  if (db->invPropDB != NULL)	(*db->invPropDB->close)(db->invPropDB);
  freeMem(db);
  r->pdata = NULL;
  return 0;
}



char *
makeUSKey (RDF_Resource u, RDF_Resource s, PRBool inversep, size_t *size)
{
  if ((s == gCoreVocab->RDF_name) || (inversep && (s == gCoreVocab->RDF_parent))) {
    *size = strlen(resourceID(u));
    return  resourceID(u);
  } else {
    char* ans;
    *size =  strlen(resourceID(u)) + strlen(resourceID(s));
    ans = getMem(*size);  
    memcpy(ans, resourceID(u), strlen(resourceID(u)));
    memcpy(&ans[strlen(resourceID(u))],  resourceID(s), strlen(resourceID(s)));
    return ans;
  }
}



DB *
getUSDB (RDFT r, RDF_Resource u, RDF_Resource s, PRBool inversep)
{
  DBMRDF db = (DBMRDF)r->pdata;
  if (inversep) {
    if (s == gCoreVocab->RDF_parent) {
      return db->childrenDB;
    } else {
      return db->invPropDB;
    }
  } else if (s == gCoreVocab->RDF_name) {
    return db->nameDB;
  } else return db->propDB;
}



void
freeKey (char* keyData, RDF_Resource u, RDF_Resource s, PRBool inversep)
{
  if ((s == gCoreVocab->RDF_name) || (inversep && (s == gCoreVocab->RDF_parent))) return;
  freeMem(keyData);
}



DBMAs *
DBM_GetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep, size_t *size)
{
  size_t keySize;
  void* keyData = makeUSKey(u, s, inversep, &keySize);
  DBT key, data;
  DB *db;
  int status;
  CHECK_VAR(keyData, NULL);
  key.data = keyData;
  key.size = keySize;
  db = getUSDB(rdf, u, s, inversep);
  if (db == NULL) {
    *size = 0;
    freeKey(keyData, u, s, inversep);
    return NULL;
  }
  status = (*db->get)(db, &key, &data, 0);
  if (status != 0) {
    *size = 0;
    freeKey(keyData, u, s, inversep);
    return NULL;
  } else {
    void* ans = (char*)getMem(data.size);
    *size = data.size;
    memcpy(ans, data.data, *size);
    freeKey(keyData, u, s, inversep);
    return (DBMAs*) ans;
  }
}



void
DBM_PutSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep, void* value, size_t size)
{
  size_t keySize;
  void* keyData = makeUSKey(u, s, inversep, &keySize);
  DBT key, data;
  int status;
  DB* db;
  CHECK_VAR1(keyData);
  db = getUSDB(rdf, u, s, inversep);
  if (db == NULL) { 
    freeKey(keyData, u, s, inversep);
    return ;
  } 
  key.data = keyData;
  key.size = keySize;
  data.data = value;
  data.size = size; 
  status = (*db->del)(db, &key, 0);
  if (value != NULL) {
	 status = (*db->put)(db, &key, &data, 0);
  }  
  if ((status == 0) && (!doingFirstTimeInitp)) (*db->sync)(db, 0);
  freeKey(keyData, u, s, inversep);
}



PRBool
nlocalStoreHasAssertion (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv)
{
  size_t size  ;
  DBMAs   *data;    
  uint16  n    = 0;
  PRBool ans = 0;
  PRBool invp = (s == gCoreVocab->RDF_parent);

  XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char* )v)));
  data =  (invp ? DBM_GetSlotValue(rdf, (RDF_Resource)v, s, 1, &size) : 
	   DBM_GetSlotValue(rdf, u, s, 0, &size));
  if (data == NULL) return 0;
  while (n < size) {
    DBMAs nas = nthdbmas(data, n);
    if (nas == NULL) break;
    if ((type ==  valueTypeOfAs(nas)) && (tvOfAs(nas) == tv) && 
	(invp  ? valueEqual(type, dataOfDBMAs(nas), u) : valueEqual(type, dataOfDBMAs(nas), v))) {
      ans = 1;
      break;
    }
    n = dbmasSize(nas) + n;
  }
  freeMem(data);
  return ans;
}



void *
nlocalStoreGetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s,  
			       RDF_ValueType type, PRBool inversep, PRBool tv)
{
  size_t size  ;
  DBMAs   *data;    
  uint16  n    = 0;
  void* ans;
  data =  DBM_GetSlotValue(rdf, u, s, inversep, &size);
  if (data == NULL) return 0;
  while (n < size) {
    DBMAs       nas = nthdbmas(data, n);
    if (nas == NULL) break;
    if (type == valueTypeOfAs(nas)) {
      if (type == RDF_STRING_TYPE) {
	ans = copyString((char *)dataOfDBMAs(nas));
      } else if (type == RDF_RESOURCE_TYPE) {
	ans = RDF_GetResource(NULL, (char *)dataOfDBMAs(nas), true);
      } else if (type == RDF_INT_TYPE) {
        /* ans = dataOfDBMAs(nas); */
        memcpy((char*)&ans, dataOfDBMAs(nas), sizeof(uint32));
      }
      freeMem(data);
      XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char* )ans)));
      return ans;
    }
    n =  dbmasSize(nas) + n;
  }
  freeMem((void*)data);
  return NULL;
}



RDF_Cursor
nlocalStoreGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s, 
				    RDF_ValueType type, 
				    PRBool inversep, PRBool tv)
{
  RDF_Cursor c;
  void* val;
  size_t size;
  if (resourceType(u) == LFS_RT) return NULL;
  if (!tv && (s != gCoreVocab->RDF_parent)) return NULL;
  val = DBM_GetSlotValue(rdf, u, s, inversep, &size);
  if (val == NULL) return NULL;  
  c = (RDF_Cursor) getMem(sizeof(struct RDF_CursorStruct));
  if (c == NULL) {
    freeMem(val);
    return NULL;
  }
  c->u = u;
  c->s = s;
  c->inversep = inversep;
  c->type = type; 
  c->tv = tv;
  c->count = 0;
  c->pdata = val;
  c->size = size;
  return c;
}



void *
nlocalStoreNextValue (RDFT rdf, RDF_Cursor c)
{
  void* ans;
  void* data;
  if ((c == NULL) || (c->pdata == NULL)) return NULL;
  if ((c->type == RDF_ARC_LABELS_IN_QUERY) || (c->type == RDF_ARC_LABELS_OUT_QUERY)) 
    return  nlcStoreArcsInOutNextValue(rdf, c);
  data = c->pdata;  
  while (c->count < c->size) {
    DBMAs     nas = nthdbmas(data, c->count);
    if (nas == NULL) break;
    if ((c->tv == tvOfAs(nas)) && (c->type == valueTypeOfAs(nas))) {
      if (c->type == RDF_RESOURCE_TYPE) {
        RDF_Resource nu = RDF_GetResource(NULL, (char *)dataOfDBMAs(nas), 1);

        if (nu  && startsWith("http:", resourceID(nu)) && strstr(resourceID(nu), ".rdf")) {
          RDFL rl = rdf->rdf;
          char* dburl = getBaseURL(resourceID(nu));
          while (rl) {
            RDF_AddDataSource(rl->rdf, dburl);
            rl = rl->next;
          }
          freeMem(dburl);
        }

	ans = nu;
	c->count =  dbmasSize(nas) + c->count;
	return nu;
      } else {
	ans = dataOfDBMAs(nas);
	c->count =  dbmasSize(nas) + c->count;
	return ans;
      }
    }
    c->count =  dbmasSize(nas) + c->count;
  }
  return NULL;
}



RDF_Error
nlocalStoreDisposeCursor (RDFT rdf, RDF_Cursor c)
{
  if (c != NULL) {
    if (c->pdata)	freeMem(c->pdata);
    c->pdata = NULL;
    freeMem(c);
  }
  return noRDFErr;
}



DBMAs
makeAsBlock (void* v, RDF_ValueType type, PRBool tv,  size_t *size)
{
  size_t vsize=0;
  DBMAs ans;
  int rem = 0;
/*
  ldiv_t  cdiv ;
*/
  XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char* )v)));
  if (type == RDF_STRING_TYPE) {
    vsize = strlen(v);
  } else if (type == RDF_RESOURCE_TYPE) {
    vsize = strlen( resourceID((RDF_Resource)v));
  } else if (type == RDF_INT_TYPE) {
    vsize = 4;
  }
  *size = 4 + vsize + 1;
  rem = *size % 4;
  if (rem) {
    *size += 4 - rem;
  }
  ans = (DBMAs) getMem(*size);
  if (ans == NULL) return NULL;
  ans->size[0] = (uint8)(((*size) & 0x00FF0000) >> 16);
  ans->size[1] = (uint8)(((*size) & 0x0000FF00) >> 8);
  ans->size[2] = (uint8)((*size) & 0x000000FF);
  *(((unsigned char *)ans)+3) = (tv ? 0x10 : 0) | (type & 0x0F);
  if (type == RDF_STRING_TYPE) {
    memcpy((char*)ans+4, (char*) v, vsize);
  } else if (type == RDF_RESOURCE_TYPE) {
    memcpy((char*)ans+4,  resourceID((RDF_Resource)v), vsize);
  } else if (type == RDF_INT_TYPE) {
    memcpy((char*)ans+4, (char*)v, vsize);
  }
/*
  cdiv   = ldiv(*size, 256);
  ans->size[0] = (uint8)(cdiv.quot);
  ans->size[1] = (uint8)(cdiv.rem);
  ans->tag  = (tv ? 0x10 : 0) | (type & 0x0F);
  if (type == RDF_STRING_TYPE) {
    memcpy((char*)ans+3, (char*) v, vsize);
  } else if (type == RDF_RESOURCE_TYPE) {
    memcpy((char*)ans+3,  resourceID((RDF_Resource)v), vsize);
  } else if (type == RDF_INT_TYPE) {
    memcpy((char*)ans+3, (char*)v, vsize);
  }
*/
  return ans;
}



PRBool
nlocalStoreAssert (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, 
			 RDF_ValueType type, PRBool tv)
{
    size_t size  ;
    DBMAs*   data;
    char* ndata;
    DBMAs temp;
    uint16  n    = 0;
    size_t tsize;
    PRBool ans = 0;
    XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char* )v)));
    
    /* don't store RDF Commands in the local store */
    if (s == gNavCenter->RDF_Command)	return 0;
    
    data =  DBM_GetSlotValue(rdf, u, s, 0, &size);
    if (((data == NULL) && (size != 0)) || ((size == 0) && (data != NULL))) return 0;
    while (n < size) {
      DBMAs  nas = nthdbmas(data, n);
      if (nas == NULL) {freeMem(data); return 0;}
      if (type == valueTypeOfAs(nas) && (valueEqual(type, dataOfDBMAs(nas), v))) {
	  ans = 1;
	  break;
	}
      n =  dbmasSize(nas) + n;
    }
    if (ans) {
      freeMem(data);
      return 1;
    } else {
      temp = makeAsBlock(v, type, tv,  &tsize);
      if (temp == NULL) {freeMem(data);return 0;}
      if (data == NULL) {
	DBM_PutSlotValue(rdf, u, s, 0, (void*)temp, tsize);
	/* addSlotsHere(rdf, u, s); */
	freeMem(temp);
	temp = NULL;
      } else {
	ndata = (char*)getMem(size + tsize);
	if (ndata == NULL) {freeMem(data); freeMem(temp);return 0;}
	memcpy(ndata, data, size);
	memcpy(&ndata[size], (char*)temp, tsize);
	DBM_PutSlotValue(rdf, u,s, 0, ndata, size+tsize);
	freeMem(data);
	freeMem(ndata);
	freeMem(temp);
      }

      if (type == RDF_RESOURCE_TYPE) {
	temp = makeAsBlock(u, RDF_RESOURCE_TYPE,  tv,  &tsize);
	if (temp == NULL) return 0;
	data = DBM_GetSlotValue(rdf, (RDF_Resource)v, s, 1, &size);
	if (data == NULL) {
	  DBM_PutSlotValue(rdf, (RDF_Resource)v, s, 1, (void*) temp, tsize);
	  freeMem(temp);
	  /*   addSlotsIn(rdf, (RDF_Resource)v, s);*/
	} else {
	  ndata = (char*)getMem(size + tsize);
	  if (ndata == NULL) {freeMem(data); freeMem(temp);return 0;}
	  memcpy(ndata, data, size);
	  memcpy(&ndata[size], (char*)temp, tsize);
	  DBM_PutSlotValue(rdf, (RDF_Resource)v, s, 1, ndata, size+tsize);
	  freeMem(data);
	  freeMem(ndata);
	  freeMem(temp);
	}
      }
    }
    sendNotifications2(rdf, RDF_INSERT_NOTIFY, u, s, v, type, tv);
    return 1;
}



PRBool
nlocalStoreAssert1 (RDFFile f, RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, 
			 RDF_ValueType type, PRBool tv)
{
        XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char* )v)));
	return nlocalStoreAssert(rdf, u, s, v, type, tv);
}



PRBool
nlocalStoreUnassert (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, 
			   RDF_ValueType type)
{
  size_t size  ;
  DBMAs*   data;
  char* temp;
  uint16  n    = 0;
  size_t tsize;
  PRBool ans = 0;
  DBMAs nas;
  XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char* )v)));
  data =  DBM_GetSlotValue(rdf, u, s, 0, &size);
  if (data == NULL) return 1;
  while (n < size) {
    nas =  nthdbmas(data, n);
    if (type == valueTypeOfAs(nas) && (valueEqual(type, dataOfDBMAs(nas), v))) {
      ans = 1;
      break;
    }
    n =  dbmasSize(nas) + n;
  }
  if (!ans) {
    freeMem(data);
    return 1;
  } else {
    if (size ==  dbmasSize(nas)) {
      DBM_PutSlotValue(rdf, u, s, 0, NULL, 0);
      /*deleteSlotsHere(rdf, u, s);*/
    } else {
      tsize = size -  dbmasSize(nas);
      temp = (char*)getMem(tsize);
      if (temp == NULL) {
	freeMem(data);
	return 0;
      }
      if (n != 0) memcpy(temp, data, n);
      memcpy(((char*)temp+n), ((char*)data + n +  dbmasSize(nas)), tsize-n);
      DBM_PutSlotValue(rdf, u, s, 0, temp, tsize);
      freeMem(temp);
    }
    freeMem(data);
    
    if (type == RDF_RESOURCE_TYPE) {
      data = DBM_GetSlotValue(rdf, ((RDF_Resource)v), s, 1, &size);
      ans = n = 0;
      if (data == NULL) {
	return 1;
      } else {
	while (n < size) {
	  nas =   nthdbmas(data, n);
	  if (valueEqual(RDF_RESOURCE_TYPE, dataOfDBMAs(nas), u)){
	    ans = 1;
	    break;
	  }
	  n =  dbmasSize(nas) + n;
	}
	if (!ans) {
	  return 1;
	} else {
	  if (size ==  dbmasSize(nas)) {
	      DBM_PutSlotValue(rdf, (RDF_Resource)v, s, 1, NULL, 0);
	      /*	      deleteSlotsIn(rdf, (RDF_Resource)v, s); */
	  } else {
	    tsize = size -  dbmasSize(nas);
	    temp = (char*)getMem(tsize);
	    if (temp == NULL)  {
	      freeMem(data);
	      return 0;
	    }
	    if (n) memcpy(temp, data, n);
	    memcpy(((char*)temp+n), ((char*)data + n +  dbmasSize(nas)), tsize-n);
	    DBM_PutSlotValue(rdf, ((RDF_Resource)v), s, 1, temp, tsize);
	    freeMem(temp);
	  }
	  freeMem(data);
	}
      }
    }
  }
  sendNotifications2(rdf, RDF_DELETE_NOTIFY, u, s, v, type, 1);
  return 1;
}



void
addSlotsHere (RDFT rdf, RDF_Resource u, RDF_Resource s)
{
  if ((s != gCoreVocab->RDF_name) && (s != gCoreVocab->RDF_parent) && 
      (s != gCoreVocab->RDF_slotsHere) && (s != gCoreVocab->RDF_slotsIn)) {
    nlocalStoreAssert (rdf, u, gCoreVocab->RDF_slotsHere, s, RDF_RESOURCE_TYPE, 1);
  }
}



void
deleteSlotsHere (RDFT rdf, RDF_Resource u, RDF_Resource s)
{
  if ((s != gCoreVocab->RDF_name) && (s != gCoreVocab->RDF_parent) && 
      (s != gCoreVocab->RDF_slotsHere) &&  (s != gCoreVocab->RDF_slotsIn)) {
    nlocalStoreUnassert (rdf, u, gCoreVocab->RDF_slotsHere, s, RDF_RESOURCE_TYPE);
  }
}



void
addSlotsIn (RDFT rdf, RDF_Resource u, RDF_Resource s)
{
  if ((s != gCoreVocab->RDF_name) && (s != gCoreVocab->RDF_parent) && 
      (s != gCoreVocab->RDF_slotsHere) &&  (s != gCoreVocab->RDF_slotsIn)) {
    nlocalStoreAssert (rdf, u, gCoreVocab->RDF_slotsIn, s, RDF_RESOURCE_TYPE, 1);
  }
}



void
deleteSlotsIn (RDFT rdf, RDF_Resource u, RDF_Resource s)
{
  if ((s != gCoreVocab->RDF_name) && (s != gCoreVocab->RDF_parent) && 
      (s != gCoreVocab->RDF_slotsHere) &&  (s != gCoreVocab->RDF_slotsIn)) {
    nlocalStoreUnassert (rdf, u, gCoreVocab->RDF_slotsIn, s, RDF_RESOURCE_TYPE);
  }
}



void
nlclStoreKill (RDFT rdf, RDF_Resource u)
{
  size_t size  ;
  DBMAs*   data;
  uint16  n    = 0;
  data =  DBM_GetSlotValue(rdf, u, gCoreVocab->RDF_slotsHere, 0, &size);
  while (n < size) {
    DBMAs nas =  nthdbmas(data, n);
    RDF_Resource s;
    s = RDF_GetResource(NULL, (char*)dataOfDBMAs(nas), 1);
    DBM_PutSlotValue(rdf, u, s, 0, NULL, 0);
    n =  dbmasSize(nas) + n;
  }
  DBM_PutSlotValue(rdf, u, gCoreVocab->RDF_name, 0, NULL, 0) ;
  DBM_PutSlotValue(rdf, u, gCoreVocab->RDF_parent, 1, NULL, 0) ;
  DBM_PutSlotValue(rdf, u, gCoreVocab->RDF_parent, 0, NULL, 0) ;
  data =  DBM_GetSlotValue(rdf, u, gCoreVocab->RDF_slotsIn, 0, &size);
  while (n < size) {
    DBMAs nas =  nthdbmas(data, n);
    RDF_Resource s;
    s = RDF_GetResource(NULL, (char*)dataOfDBMAs(nas), 1);
    DBM_PutSlotValue(rdf, u, s, 1, NULL, 0);
    n =  dbmasSize(nas) + n;
  }
}



PRBool
nlocalStoreAddChildAt(RDFT rdf, RDF_Resource parent, RDF_Resource ref, 
			      RDF_Resource new, PRBool beforep)
{
    size_t size  ;
    DBMAs*   data;
    char* ndata;
    RDF_Resource s = gCoreVocab->RDF_parent;
    DBMAs temp;
    uint16  n    = 0;
    size_t tsize;
    PRBool ans = 0;
    DBMAs  nas;
    data =  DBM_GetSlotValue(rdf, parent, s, 1, &size);
    if (!data) return 0;
    while (n < size) {
      nas = nthdbmas(data, n);
      if (valueEqual(RDF_RESOURCE_TYPE, dataOfDBMAs(nas), ref)) {
	  ans = 1;
	  if (!beforep) {	    
	    n =  dbmasSize(nas) + n;
	  }	    
	  break;
	}
      n =  dbmasSize(nas) + n;
    }
    if (!ans) {
      freeMem(data);
      return 0;
    } else {
      char* dx = (char*)data;
	  
      temp = makeAsBlock(new, RDF_RESOURCE_TYPE, 1,  &tsize);
      ndata = (char*)getMem(size + tsize);
      if ((temp == NULL) || (ndata == NULL)) {freeMem(data);freeMem(temp);freeMem(ndata);return 1;}
      memcpy(ndata, dx, n);
      memcpy(&ndata[n], (char*)temp, tsize);
      memcpy(&ndata[n+tsize], &dx[n], size-n);
      DBM_PutSlotValue(rdf, parent, s, 1, ndata, size+tsize);
      freeMem(data);
      freeMem(ndata);
      freeMem(temp);
    }

    temp = makeAsBlock(parent, RDF_RESOURCE_TYPE,  1,  &tsize);
    if (temp == NULL) return 0;
    data = DBM_GetSlotValue(rdf, new, s, 0, &size);
    if (data == NULL) {
      DBM_PutSlotValue(rdf, new, s, 0, (void*) temp, tsize);     
    } else {
      ndata = (char*)getMem(size + tsize);
      if (ndata == NULL) {freeMem(data);freeMem(temp);return 0;}
      memcpy(ndata, data, size);
      memcpy(&ndata[size], (char*)temp, tsize);
      DBM_PutSlotValue(rdf, (RDF_Resource)new, s, 0, ndata, size+tsize);
      freeMem(data);
      freeMem(ndata);
      freeMem(temp);
    }
    sendNotifications2(rdf, RDF_INSERT_NOTIFY, new, s, parent, RDF_RESOURCE_TYPE, 1);
    return 1;
}



RDF_Cursor 
nlcStoreArcsIn (RDFT rdf, RDF_Resource u)
{
  RDF_Cursor c = (RDF_Cursor) getMem(sizeof(struct RDF_CursorStruct));
  c->u = u;
  c->queryType = RDF_ARC_LABELS_IN_QUERY;
  c->inversep = 1;
  c->count = 0;
  return c;
}



RDF_Cursor 
nlcStoreArcsOut (RDFT rdf, RDF_Resource u)
{
  RDF_Cursor c = (RDF_Cursor) getMem(sizeof(struct RDF_CursorStruct));
  c->u = u;
  c->queryType =  RDF_ARC_LABELS_OUT_QUERY;
  c->count = 0;
  return c;
}



RDF_Resource 
nlcStoreArcsInOutNextValue (RDFT rdf, RDF_Cursor c)
{
  while (c->count < (int16) gCoreVocabSize) {
    RDF_Resource s = *(gAllVocab + c->count);
    size_t size;
    void* data = DBM_GetSlotValue(rdf, c->u, s, c->inversep, &size);
    c->count++;
    if (data) {
      freeMem(data);
      return s;
    } else {
      freeMem(data);
    }
  }
  return NULL;
}



RDFT
MakeLocalStore (char* url)
{
  if (startsWith(url, "rdf:localStore") && (gLocalStore))  {
    return gLocalStore;
  } else if (startsWith(url, "rdf:ops") && (gOPSStore))  {
    return gOPSStore;
  } else if (startsWith(url, "rdf:ops") || startsWith(url, "rdf:localStore")) {
    RDFT ntr = (RDFT)getMem(sizeof(struct RDF_TranslatorStruct));
    DBMRDF db  = (DBMRDF)getMem(sizeof(struct _DBMRDFStruct));
    CHECK_VAR(ntr, NULL);
    CHECK_VAR(db, NULL);
    if (startsWith(url, "rdf:localStore")) {
      gLocalStore = ntr;
    } else {
      gOPSStore = ntr;
    }
    ntr->url  = copyString(url);
    ntr->assert = nlocalStoreAssert;
    ntr->unassert = nlocalStoreUnassert;
    ntr->getSlotValue = nlocalStoreGetSlotValue;
    ntr->getSlotValues = nlocalStoreGetSlotValues;
    ntr->hasAssertion = nlocalStoreHasAssertion;
    ntr->nextValue = nlocalStoreNextValue;
    ntr->disposeCursor = nlocalStoreDisposeCursor;
    ntr->destroy = DBM_CloseRDFDBMStore;
    ntr->arcLabelsIn = nlcStoreArcsIn;
    ntr->arcLabelsOut = nlcStoreArcsOut;
    ntr->pdata = db;
    DBM_OpenDBMStore(db, (startsWith(url, "rdf:localStore") ? "NavCen" : &url[4]));
    nlocalStoreAssert(ntr,  gNavCenter->RDF_BookmarkFolderCategory,  gCoreVocab->RDF_name, 
                      copyString("Bookmarks"), RDF_STRING_TYPE, 1);
    return ntr;
  } 
  else return NULL;
}
