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
   This file implements LDAP support for the rdf data model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

/*
  XXX Someone needs to get this up to speed again
*/
#if 0
#ifdef MOZ_LDAP

#include "ldap2rdf.h"
#include "utils.h"

	/* statics */
static PRHashTable	*ldap2rdfHash;
static PRHashTable	*invldap2rdfHash;
static RDFT		gRDFDB;
static PRBool		ldap2rdfInitedp = 0;



RDFT
MakeLdapStore (char* url)
{
  RDFT ntr = (RDFT)getMem(sizeof(RDF_TranslatorStruct));
  ntr->assert = ldapAssert;
  ntr->unassert = ldapUnassert;
  ntr->getSlotValue = ldapGetSlotValue;
  ntr->getSlotValues = ldapGetSlotValues;
  ntr->hasAssertion = ldapHasAssertion;
  ntr->nextValue = ldapNextValue;
  ntr->disposeCursor = ldapDisposeCursor;
  ldap2rdfInit(ntr);
  return ntr;
}



RDF_Error
LdapInit (RDFT ntr)
{
  ntr->assert = ldapAssert;
  ntr->unassert = ldapUnassert;
  ntr->getSlotValue = ldapGetSlotValue;
  ntr->getSlotValues = ldapGetSlotValues;
  ntr->hasAssertion = ldapHasAssertion;
  ntr->nextValue = ldapNextValue;
  ntr->disposeCursor = ldapDisposeCursor;
  ldap2rdfInit(ntr);
  return 0;
}



void
ldap2rdfInit (RDFT rdf)
{
  if (!ldap2rdfInitedp) {
    ldap2rdfHash = PR_NewHashTable(500, idenHash, idenEqual, idenEqual, null, null);
    invldap2rdfHash = PR_NewHashTable(500, idenHash, idenEqual, idenEqual, null, null);
    gRDFDB  = rdf;
    ldap2rdfInitedp = 1;
  }
}



Assertion
ldaparg1 (RDF_Resource u)
{
	return (Assertion) PR_HashTableLookup(ldap2rdfHash, u);
}



Assertion
setldaparg1 (RDF_Resource u, Assertion as)
{
	return (Assertion) PR_HashTableAdd(ldap2rdfHash, u, as);
}



Assertion
ldaparg2 (RDF_Resource u)
{
	return (Assertion) PR_HashTableLookup(invldap2rdfHash, u);
}



Assertion
setldaparg2 (RDF_Resource u, Assertion as)
{
	return (Assertion) PR_HashTableAdd(invldap2rdfHash, u, as);
}



PRBool
ldapAssert (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, 
		   RDF_ValueType type, PRBool tv)
{
  if (!ldap2rdfInitedp) ldap2rdfInit(rdf);
  if ((s == gCoreVocab->RDF_parent) && (type == RDF_RESOURCE_TYPE)  &&
      (tv) && (ldapContainerp(v))) {
    return (ldapAddChild(rdf, (RDF_Resource)v, u));
  } else {
    return 0;
  }
}



PRBool
ldapUnassert (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, 
		   RDF_ValueType type)
{
  if (!ldap2rdfInitedp) ldap2rdfInit(rdf);
  if ((s == gCoreVocab->RDF_parent) && (type == RDF_RESOURCE_TYPE)  &&
      (ldapContainerp(v))) {
    return (ldapRemoveChild(rdf, (RDF_Resource)v, u));
  } else {
    return 0;
  }
}



PRBool
ldapDBAdd (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type)
{
  Assertion nextAs, prevAs, newAs; 
  if ((s == gCoreVocab->RDF_instanceOf) && (v == gWebData->RDF_Container)) {
    setContainerp(u, true);
    return 1;
  }
 	
  nextAs = prevAs = ldaparg1(u);
  while (nextAs != null) {
    if (asEqual(nextAs, u, s, v, type)) return 1;
    prevAs = nextAs;
    nextAs = nextAs->next;
  }
  newAs = makeNewAssertion(u, s, v, type, 1);
  if (prevAs == null) {
    setldaparg1(u, newAs);
  } else {
    prevAs->next = newAs;
  }
  if (type == RDF_RESOURCE_TYPE) {
    nextAs = prevAs = ldaparg2((RDF_Resource)v);
    while (nextAs != null) {
      prevAs = nextAs;
      nextAs = nextAs->invNext;
    }
    if (prevAs == null) {
      setldaparg2((RDF_Resource)v, newAs);
    } else {
      prevAs->invNext = newAs;
    }
  }
  sendNotifications2(rdf, RDF_ASSERT_NOTIFY, u, s, v, type, 1);
  return true;
}



PRBool
ldapDBRemove (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type)
{
  Assertion nextAs, prevAs,  ans;
  PRBool found = false;
  nextAs = prevAs = ldaparg1(u);
  while (nextAs != null) {
    if (asEqual(nextAs, u, s, v, type)) {
      if (prevAs == null) {
	setldaparg1(u, nextAs->next);
      } else {
	prevAs->next = nextAs->next;
      }
      found = true;
      ans = nextAs;
      break;
    }
    prevAs = nextAs;
    nextAs = nextAs->next; 
  }
  if (found == false) return false;
  if (type == RDF_RESOURCE_TYPE) {
    nextAs = prevAs = ldaparg2((RDF_Resource)v);
    while (nextAs != null) {
      if (nextAs == ans) {
	if (prevAs == nextAs) {
	setldaparg2((RDF_Resource)v,  nextAs->invNext);
	} else {
	  prevAs->invNext = nextAs->invNext;
	}
      }
      prevAs = nextAs;
      nextAs = nextAs->invNext;
    }
  }
  sendNotifications2(rdf, RDF_DELETE_NOTIFY, u, s, v, type, 1);
  return true;
}



PRBool
ldapHasAssertion (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv)
{
  Assertion nextAs;
  if (!ldap2rdfInitedp) ldap2rdfInit(rdf);

  nextAs = ldaparg1(u);
  while (nextAs != null) {
    if (asEqual(nextAs, u, s, v, type) && (nextAs->tv == tv)) return true;
    nextAs = nextAs->next;
  }

  possiblyAccessldap(rdf, u, s, false);

  nextAs = ldaparg1(u);
  while (nextAs != null) {
    if (asEqual(nextAs, u, s, v, type) && (nextAs->tv == tv)) return true;
    nextAs = nextAs->next;
  }
  return false;
}



void *
ldapGetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep,  PRBool tv)
{
  Assertion nextAs;
  if (!ldap2rdfInitedp) ldap2rdfInit(rdf);

  nextAs = (inversep ? ldaparg2(u) : ldaparg1(u));
  while (nextAs != null) {
    if ((nextAs->s == s) && (nextAs->tv == tv) && (nextAs->type == type)) {
      return (inversep ? nextAs->u : nextAs->value);
    }
    nextAs = (inversep ? nextAs->invNext : nextAs->next);
  }

  possiblyAccessldap(rdf, u, s, inversep);

  nextAs = (inversep ? ldaparg2(u) : ldaparg1(u));
  while (nextAs != null) {
    if ((nextAs->s == s) && (nextAs->tv == tv) && (nextAs->type == type)) {
      return (inversep ? nextAs->u : nextAs->value);
    }
    nextAs = (inversep ? nextAs->invNext : nextAs->next);
  }

  return null;
}



RDF_Cursor
ldapGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv)
{
  Assertion as;
  RDF_Cursor c;
  if (!ldap2rdfInitedp) ldap2rdfInit(rdf);
  as  = (inversep ? ldaparg2(u) : ldaparg1(u));
  if (as == null) {
    possiblyAccessldap(rdf, u, s, inversep);
    as  = (inversep ? ldaparg2(u) : ldaparg1(u));
    if (as == null)
	return null;
  }
  c = (RDF_CursorStruc*)getMem(sizeof(RDF_CursorStruc));
  c->u = u;
  c->s = s;
  c->type = type;
  c->inversep = inversep;
  c->tv = tv;
  c->count = 0;
  c->pdata = as;
  return c;
}



void *
ldapNextValue (RDFT mcf, RDF_Cursor c)
{
  while (c->pdata != null) {
    Assertion as = (Assertion) c->pdata;
    if ((as->s == c->s) && (as->tv == c->tv) && (c->type == as->type)) {
      c->value = (c->inversep ? as->u : as->value);
      c->pdata = (c->inversep ? as->invNext : as->next);
      return c->value;
    }
    c->pdata = (c->inversep ? as->invNext : as->next);
  }
  return null;
}



RDF_Error
ldapDisposeCursor (RDFT mcf, RDF_Cursor c)
{
  freeMem(c);
  return noRDFErr;
}



RDF_Resource
ldapNewContainer(char *id)
{
	return(NULL);
}



int
ldapModifyEntry (RDFT rdf, RDF_Resource parent, RDF_Resource child, PRBool addFlag)
{
	RDF_Cursor	c;
	RDF_Resource	newParent, r;
	char		*urivals[2];
	LDAP		*ld;
	LDAPMod		urimod, *mods[2];
	LDAPURLDesc	*ldURL=NULL;
	int		err;
	char		*errStr, *parentID;

	parentID = resourceID(parent);

	if (containerp(child))
	{
		if (newParent = ldapNewContainer(parentID))
		{
			if ((c = RDF_GetSources(rdf->rdf->rdf, child,
				gCoreVocab->RDF_parent, RDF_RESOURCE_TYPE, 1)) != NULL)
			{
				while ((r = RDF_NextValue(c)) != NULL)
				{
					err = ldapModifyEntry(rdf, newParent, r, addFlag);
					if (err)
					{
						/* XXX MAJOR rollback issues!
							Punt for now! */

						return(err);
					}
				}
			}
			else
			{
				return(-1);
			}
		}
		else
		{
			return(-1);
		}
	}

	ldap_url_parse(parentID, &ldURL);
	if (ldURL == NULL)	return(-1);
	ld = ldap_init (ldURL->lud_host, ldURL->lud_port);
	if (ld == NULL)
	{
		ldap_free_urldesc(ldURL);
		return(-1);
	}
	if ((err = ldap_simple_bind_s(ld, ADMIN_ID, ADMIN_PW))		/* XXX */
		!= LDAP_SUCCESS)
	{
		if ((errStr = ldap_err2string(err)) != NULL)
		{
			XP_MakeHTMLAlert(NULL, errStr);
		}

		ldap_unbind(ld);
		ldap_free_urldesc(ldURL);
		return(-1);
	}

	urivals[0] = resourceID(child);
	urivals[1] = NULL;

	urimod.mod_op = ((addFlag == true) ? LDAP_MOD_ADD : LDAP_MOD_DELETE);
	urimod.mod_type = "labeledURI";
	urimod.mod_values = urivals;

	mods[0] = &urimod;
	mods[1] = NULL;

	err = ldap_modify_s(ld, ldURL->lud_dn, mods);
	if (err != LDAP_SUCCESS)
	{
		if ((errStr = ldap_err2string(err)) != NULL)
		{
			XP_MakeHTMLAlert(NULL, errStr);
		}
	}

	ldap_unbind(ld);
	ldap_free_urldesc(ldURL);
	return(err);
}



PRBool
ldapAddChild (RDFT rdf, RDF_Resource parent, RDF_Resource child)
{
	return (ldapModifyEntry(rdf, parent, child, true) != LDAP_SUCCESS);
}



PRBool
ldapRemoveChild (RDFT rdf, RDF_Resource parent, RDF_Resource child)
{
	return (ldapModifyEntry(rdf, parent, child, false) != LDAP_SUCCESS);
}



void
possiblyAccessldap(RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep)
{
  /** try to get the values of u.s from the directory **/

	LDAP		*ld;
	LDAPMessage	*result=NULL, *entry;
	LDAPURLDesc	*ldURL=NULL;
	RDF_Resource	node;
	char		*attrs[2], **vals;
	int		err, i;
	char		*title = NULL;

	/*
		Note: a labeledURI is a url followed by an optional [space title-string]
         */

  if (!stringEquals(resourceID(s), resourceID(gCoreVocab->RDF_parent)) || (!inversep) || (!ldap_is_ldap_url(resourceID(u))))	return;

  ldap_url_parse(resourceID(u), &ldURL);
  if (ldURL == NULL)	return;
  ld = ldap_init (ldURL->lud_host, ldURL->lud_port);
  if (ld == NULL)
	{
	ldap_free_urldesc(ldURL);
	return;
	}
  if ((err = ldap_simple_bind_s(ld, NULL, NULL)) != LDAP_SUCCESS)
	{
	ldap_unbind(ld);
	ldap_free_urldesc(ldURL);
	return;
	}

  attrs[0] = "labeledURI";
  attrs[1] = NULL;

  err = ldap_search_s(ld, ldURL->lud_dn, LDAP_SCOPE_BASE, ldURL->lud_filter, attrs, 0, &result);
  if (err == LDAP_SUCCESS)
	{
	for (entry=ldap_first_entry(ld, result); entry!=NULL; entry=ldap_next_entry(ld, entry))
		{
		if ((vals = ldap_get_values(ld, entry, attrs[0])) != NULL)
		{
			for (i=0; vals[i] != NULL; i++)
			{
				/* vals[i] has a URL... add into RDF graph */

/*
				if (((title = strstr(vals[i], " ")) != NULL)
					&& (*(title+1) != '\0'))
				{
					*(++title) = '\0';
				}
				else
				{
					title = NULL;
				}
*/
				if ((node = RDF_Create(vals[i], true)) != NULL)
				{
					setResourceType(node, LDAP_RT);
					if (ldapContainerp(node) == true)
					{
						setContainerp(node, 1);
					}

					ldapDBAdd(rdf, node, gCoreVocab->RDF_parent,
							u, RDF_RESOURCE_TYPE);

					if (title != NULL)
					{
						ldapDBAdd(rdf, node, gCoreVocab->RDF_name,
							title, RDF_STRING_TYPE);
					}
				}
			}
			ldap_value_free(vals);
			}
		}
	}
  if (result != NULL)
	{
	ldap_msgfree(result);
	}
  ldap_unbind(ld);
  ldap_free_urldesc(ldURL);
}



PRBool
ldapContainerp (RDF_Resource u)
{
	return(ldap_is_ldap_url(resourceID(u)));		/* XXX ??? */
}


#endif /* MOZ_LDAP */
#endif 

