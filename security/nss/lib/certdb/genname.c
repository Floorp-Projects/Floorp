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

#include "plarena.h"
#include "seccomon.h"
#include "secitem.h"
#include "secoidt.h"
#include "mcom_db.h"
#include "secasn1.h"
#include "secder.h"
#include "certt.h"
#include "cert.h"
#include "xconst.h"
#include "secerr.h"
#include "secoid.h"
#include "prprf.h"
#include "genname.h"



static const SEC_ASN1Template CERTNameConstraintTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CERTNameConstraint) },
    { SEC_ASN1_ANY, offsetof(CERTNameConstraint, DERName) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 0, 
          offsetof(CERTNameConstraint, min), SEC_IntegerTemplate }, 
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 1, 
          offsetof(CERTNameConstraint, max), SEC_IntegerTemplate },
    { 0, }
};

const SEC_ASN1Template CERT_NameConstraintSubtreeSubTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, SEC_AnyTemplate }
};


const SEC_ASN1Template CERT_NameConstraintSubtreePermitedTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | 0, 0, CERT_NameConstraintSubtreeSubTemplate }
};

const SEC_ASN1Template CERT_NameConstraintSubtreeExcludedTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | 1, 0, CERT_NameConstraintSubtreeSubTemplate }
};


static const SEC_ASN1Template CERTNameConstraintsTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CERTNameConstraints) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 0, 
          offsetof(CERTNameConstraints, DERPermited), CERT_NameConstraintSubtreeSubTemplate},
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 1, 
          offsetof(CERTNameConstraints, DERExcluded), CERT_NameConstraintSubtreeSubTemplate},
    { 0, }
};


static const SEC_ASN1Template CERTOthNameTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(OtherName) },
    { SEC_ASN1_OBJECT_ID, 
	  offsetof(OtherName, oid) },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | 0,
          offsetof(OtherName, name), SEC_AnyTemplate },
    { 0, } 
};

static const SEC_ASN1Template CERTOtherNameTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | 0 ,
      offsetof(CERTGeneralName, name.OthName), CERTOthNameTemplate, 
      sizeof(CERTGeneralName) }
};

static const SEC_ASN1Template CERTOtherName2Template[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_CONTEXT_SPECIFIC | 0 ,
      0, NULL, sizeof(CERTGeneralName) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(CERTGeneralName, name.OthName) + offsetof(OtherName, oid) },
    { SEC_ASN1_ANY,
	  offsetof(CERTGeneralName, name.OthName) + offsetof(OtherName, name) },
    { 0, } 
};

static const SEC_ASN1Template CERT_RFC822NameTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | 1 ,
          offsetof(CERTGeneralName, name.other), SEC_IA5StringTemplate,
          sizeof (CERTGeneralName)}
};

static const SEC_ASN1Template CERT_DNSNameTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | 2 ,
          offsetof(CERTGeneralName, name.other), SEC_IA5StringTemplate,
          sizeof (CERTGeneralName)}
};

static const SEC_ASN1Template CERT_X400AddressTemplate[] = {
    { SEC_ASN1_ANY | SEC_ASN1_CONTEXT_SPECIFIC | 3,
          offsetof(CERTGeneralName, name.other), SEC_AnyTemplate,
          sizeof (CERTGeneralName)}
};

static const SEC_ASN1Template CERT_DirectoryNameTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | 4,
          offsetof(CERTGeneralName, derDirectoryName), SEC_AnyTemplate,
          sizeof (CERTGeneralName)}
};


static const SEC_ASN1Template CERT_EDIPartyNameTemplate[] = {
    { SEC_ASN1_ANY | SEC_ASN1_CONTEXT_SPECIFIC | 5,
          offsetof(CERTGeneralName, name.other), SEC_AnyTemplate,
          sizeof (CERTGeneralName)}
};

static const SEC_ASN1Template CERT_URITemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | 6 ,
          offsetof(CERTGeneralName, name.other), SEC_IA5StringTemplate,
          sizeof (CERTGeneralName)}
};

static const SEC_ASN1Template CERT_IPAddressTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | 7 ,
          offsetof(CERTGeneralName, name.other), SEC_OctetStringTemplate,
          sizeof (CERTGeneralName)}
};

static const SEC_ASN1Template CERT_RegisteredIDTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | 8 ,
          offsetof(CERTGeneralName, name.other), SEC_ObjectIDTemplate,
          sizeof (CERTGeneralName)}
};


const SEC_ASN1Template CERT_GeneralNamesTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, SEC_AnyTemplate }
};



void
CERT_DestroyGeneralNameList(CERTGeneralNameList *list)
{
    PZLock *lock;

    if (list != NULL) {
	lock = list->lock;
	PZ_Lock(lock);
	if (--list->refCount <= 0 && list->arena != NULL) {
	    PORT_FreeArena(list->arena, PR_FALSE);
	    PZ_Unlock(lock);
	    PZ_DestroyLock(lock);
	} else {
	    PZ_Unlock(lock);
	}
    }
    return;
}

CERTGeneralNameList *
CERT_CreateGeneralNameList(CERTGeneralName *name) {
    PRArenaPool *arena;
    CERTGeneralNameList *list = NULL;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	goto done;
    }
    list = (CERTGeneralNameList *)
	PORT_ArenaZAlloc(arena, sizeof(CERTGeneralNameList));
    if (name != NULL) {
	list->name = (CERTGeneralName *)
	    PORT_ArenaZAlloc(arena, sizeof(CERTGeneralName));
	list->name->l.next = list->name->l.prev = &list->name->l;
	CERT_CopyGeneralName(arena, list->name, name);
    }
    list->lock = PZ_NewLock(nssILockList);
    list->arena = arena;
    list->refCount = 1;
done:
    return list;
}

CERTGeneralName *
cert_get_next_general_name(CERTGeneralName *current)
{
    PRCList *next;
    
    next = current->l.next;
    return (CERTGeneralName *) (((char *) next) - offsetof(CERTGeneralName, l));
}

CERTGeneralName *
cert_get_prev_general_name(CERTGeneralName *current)
{
    PRCList *prev;
    prev = current->l.prev;
    return (CERTGeneralName *) (((char *) prev) - offsetof(CERTGeneralName, l));
}

CERTNameConstraint *
cert_get_next_name_constraint(CERTNameConstraint *current)
{
    PRCList *next;
    
    next = current->l.next;
    return (CERTNameConstraint *) (((char *) next) - offsetof(CERTNameConstraint, l));
}

CERTNameConstraint *
cert_get_prev_name_constraint(CERTNameConstraint *current)
{
    PRCList *prev;
    prev = current->l.prev;
    return (CERTNameConstraint *) (((char *) prev) - offsetof(CERTNameConstraint, l));
}

SECItem *
cert_EncodeGeneralName(CERTGeneralName *genName, SECItem *dest, PRArenaPool *arena)
{


    PORT_Assert(arena);
    if (arena == NULL) {
	goto loser;
    }
    if (dest == NULL) {
	dest = (SECItem *) PORT_ArenaZAlloc(arena, sizeof(SECItem));
    }
    switch (genName->type) {
      case certURI:
          dest = SEC_ASN1EncodeItem(arena, dest, genName, 
				    CERT_URITemplate);
	  break;
      case certRFC822Name:
          dest = SEC_ASN1EncodeItem(arena, dest, genName, 
				    CERT_RFC822NameTemplate);
	  break;
      case certDNSName:
          dest = SEC_ASN1EncodeItem(arena, dest, genName, 
				    CERT_DNSNameTemplate);
	  break;
      case certIPAddress:
	  dest = SEC_ASN1EncodeItem(arena, dest, genName,
				    CERT_IPAddressTemplate);
	  break;
      case certOtherName:
	  dest = SEC_ASN1EncodeItem(arena, dest, genName,
				    CERTOtherNameTemplate);
	  break;
      case certRegisterID:
	  dest = SEC_ASN1EncodeItem(arena, dest, genName,
				    CERT_RegisteredIDTemplate);
	  break;
      case certEDIPartyName:
	  /* for this type, we expect the value is already encoded */
	  dest = SEC_ASN1EncodeItem (arena, dest, genName, 
				     CERT_EDIPartyNameTemplate);
	  break;
      case certX400Address:
	  /* for this type, we expect the value is already encoded */
	  dest = SEC_ASN1EncodeItem (arena, dest, genName, 
				     CERT_X400AddressTemplate);
	  break;
      case certDirectoryName:
	  if (genName->derDirectoryName.data == NULL) {
	      /* The field hasn't been encoded yet. */
	      SEC_ASN1EncodeItem (arena, &(genName->derDirectoryName),
				  &(genName->name.directoryName), 
				  CERT_NameTemplate);
	  }
	  if (genName->derDirectoryName.data == NULL) {
	      goto loser;
	  }
	  dest = SEC_ASN1EncodeItem(arena, dest, genName, 
				    CERT_DirectoryNameTemplate);
	  break;
    }
    if (!dest) {
	goto loser;
    }
    return dest;
loser:
    return NULL;
}



SECItem **
cert_EncodeGeneralNames(PRArenaPool *arena, CERTGeneralName *names)
{
    CERTGeneralName  *current_name;
    SECItem          **items = NULL;
    int              count = 0;
    int              i;
    PRCList          *head;

    PORT_Assert(arena);
    current_name = names;
    if (names != NULL) {
	count = 1;
    }
    head = &(names->l);
    while (current_name->l.next != head) {
	current_name = cert_get_next_general_name(current_name);
	++count;
    }
    current_name = cert_get_next_general_name(current_name);
    items = (SECItem **) PORT_ArenaAlloc(arena, sizeof(SECItem *) * (count + 1));
    
    if (items == NULL) {
	goto loser;
    }
    for (i = 0; i < count; i++) {
	items[i] = cert_EncodeGeneralName(current_name, (SECItem *) NULL, arena);
	if (items[i] == NULL) {
	    goto loser;
	}
	current_name = cert_get_next_general_name(current_name);
    }
    items[i] = NULL;
    return items;
loser:
    return NULL;
}

CERTGeneralName *
cert_DecodeGeneralName(PRArenaPool      *arena,
		       SECItem          *encodedName,
		       CERTGeneralName  *genName)
{
    CERTGeneralNameType              genNameType;
    SECStatus                        rv = SECSuccess;

    PORT_Assert(arena);
    if (genName == NULL) {
	genName = (CERTGeneralName *) PORT_ArenaZAlloc(arena, sizeof(CERTGeneralName));
    }
    genNameType = (CERTGeneralNameType)((*(encodedName->data) & 0x0f) + 1);
    switch (genNameType) {
      case certURI:
	  rv = SEC_ASN1DecodeItem(arena, genName, CERT_URITemplate, encodedName);
	  break;
      case certRFC822Name:
	  rv = SEC_ASN1DecodeItem(arena, genName, CERT_RFC822NameTemplate, encodedName);
	  break;
      case certDNSName:
	  rv = SEC_ASN1DecodeItem(arena, genName, CERT_DNSNameTemplate, encodedName);
	  break;
      case certIPAddress:
	  rv = SEC_ASN1DecodeItem(arena, genName, CERT_IPAddressTemplate, encodedName);
	  break;
      case certOtherName:
	  rv = SEC_ASN1DecodeItem(arena, genName, CERTOtherNameTemplate, encodedName);
	  break;
      case certRegisterID:
	  rv = SEC_ASN1DecodeItem(arena, genName, CERT_RegisteredIDTemplate, encodedName);
	  break;
      case certEDIPartyName:
	  rv = SEC_ASN1DecodeItem(arena, genName, CERT_EDIPartyNameTemplate, encodedName);
	  break;
      case certX400Address:
	  rv = SEC_ASN1DecodeItem(arena, genName, CERT_X400AddressTemplate, encodedName);
	  break;
      case certDirectoryName:
		rv = SEC_ASN1DecodeItem
		     (arena, genName, CERT_DirectoryNameTemplate, encodedName);
		if (rv != SECSuccess) {
		    goto loser;
		}
		rv = SEC_ASN1DecodeItem
		      (arena, &(genName->name.directoryName), CERT_NameTemplate,
		      &(genName->derDirectoryName));
	  break;
    }

    if (rv != SECSuccess) {
	goto loser;
    }
    genName->type = genNameType;
    genName->l.next = (PRCList *) ((char *) genName) + offsetof(CERTGeneralName, l);
    genName->l.prev = genName->l.next;
    return genName;
loser:
    return NULL;
}

CERTGeneralName *
cert_DecodeGeneralNames (PRArenaPool  *arena,
			 SECItem      **encodedGenName)
{
    PRCList                           *head = NULL;
    PRCList                           *tail = NULL;
    CERTGeneralName                   *currentName = NULL;

    PORT_Assert(arena);
    if (!encodedGenName) {
	goto loser;
    }
    while (*encodedGenName != NULL) {
	currentName = cert_DecodeGeneralName(arena, *encodedGenName, NULL);
	if (currentName == NULL) {
	    goto loser;
	}
	if (head == NULL) {
	    head = &(currentName->l);
	    tail = head;
	}
	currentName->l.next = head;
	currentName->l.prev = tail;
	tail = &(currentName->l);
	(cert_get_prev_general_name(currentName))->l.next = tail;
	encodedGenName++;
    }
    (cert_get_next_general_name(currentName))->l.prev = tail;
    return cert_get_next_general_name(currentName);
loser:
    return NULL;
}

void
CERT_DestroyGeneralName(CERTGeneralName *name)
{
    cert_DestroyGeneralNames(name);
}

SECStatus
cert_DestroyGeneralNames(CERTGeneralName *name)
{
    CERTGeneralName    *first;
    CERTGeneralName    *next = NULL;


    first = name;
    do {
	next = cert_get_next_general_name(name);
	PORT_Free(name);
	name = next;
    } while (name != first);
    return SECSuccess;
}

SECItem *
cert_EncodeNameConstraint(CERTNameConstraint  *constraint, 
			 SECItem             *dest,
			 PRArenaPool         *arena)
{
    PORT_Assert(arena);
    if (dest == NULL) {
	dest = (SECItem *) PORT_ArenaZAlloc(arena, sizeof(SECItem));
	if (dest == NULL) {
	    return NULL;
	}
    }
    cert_EncodeGeneralName(&(constraint->name), &(constraint->DERName), 
			   arena);
    
    dest = SEC_ASN1EncodeItem (arena, dest, constraint,
			       CERTNameConstraintTemplate);
    return dest;
} 

SECStatus 
cert_EncodeNameConstraintSubTree(CERTNameConstraint  *constraints,
			         PRArenaPool         *arena,
				 SECItem             ***dest,
				 PRBool              permited)
{
    CERTNameConstraint  *current_constraint = constraints;
    SECItem             **items = NULL;
    int                 count = 0;
    int                 i;
    PRCList             *head;

    PORT_Assert(arena);
    if (constraints != NULL) {
	count = 1;
    }
    head = (PRCList *) (((char *) constraints) + offsetof(CERTNameConstraint, l));
    while (current_constraint->l.next != head) {
	current_constraint = cert_get_next_name_constraint(current_constraint);
	++count;
    }
    current_constraint = cert_get_next_name_constraint(current_constraint);
    items = (SECItem **) PORT_ArenaZAlloc(arena, sizeof(SECItem *) * (count + 1));
    
    if (items == NULL) {
	goto loser;
    }
    for (i = 0; i < count; i++) {
	items[i] = cert_EncodeNameConstraint(current_constraint, 
					     (SECItem *) NULL, arena);
	if (items[i] == NULL) {
	    goto loser;
	}
	current_constraint = cert_get_next_name_constraint(current_constraint);
    }
    *dest = items;
    if (*dest == NULL) {
	goto loser;
    }
    return SECSuccess;
loser:
    return SECFailure;
}

SECStatus 
cert_EncodeNameConstraints(CERTNameConstraints  *constraints,
			   PRArenaPool          *arena,
			   SECItem              *dest)
{
    SECStatus    rv = SECSuccess;

    PORT_Assert(arena);
    if (constraints->permited != NULL) {
	rv = cert_EncodeNameConstraintSubTree(constraints->permited, arena,
					      &constraints->DERPermited, PR_TRUE);
	if (rv == SECFailure) {
	    goto loser;
	}
    }
    if (constraints->excluded != NULL) {
	rv = cert_EncodeNameConstraintSubTree(constraints->excluded, arena,
					      &constraints->DERExcluded, PR_FALSE);
	if (rv == SECFailure) {
	    goto loser;
	}
    }
    dest = SEC_ASN1EncodeItem(arena, dest, constraints, 
			      CERTNameConstraintsTemplate);
    if (dest == NULL) {
	goto loser;
    }
    return SECSuccess;
loser:
    return SECFailure;
}


CERTNameConstraint *
cert_DecodeNameConstraint(PRArenaPool       *arena,
			  SECItem           *encodedConstraint)
{
    CERTNameConstraint     *constraint;
    SECStatus              rv = SECSuccess;
    CERTGeneralName        *temp;



    PORT_Assert(arena);
    constraint = (CERTNameConstraint *) PORT_ArenaZAlloc(arena, sizeof(CERTNameConstraint));
    rv = SEC_ASN1DecodeItem(arena, constraint, CERTNameConstraintTemplate, encodedConstraint);
    if (rv != SECSuccess) {
	goto loser;
    }
    temp = cert_DecodeGeneralName(arena, &(constraint->DERName), &(constraint->name));
    if (temp != &(constraint->name)) {
	goto loser;
    }

    /* ### sjlee: since the name constraint contains only one 
     *            CERTGeneralName, the list within CERTGeneralName shouldn't 
     *            point anywhere else.  Otherwise, bad things will happen.
     */
    constraint->name.l.prev = constraint->name.l.next = &(constraint->name.l);
    return constraint;
loser:
    return NULL;
}


CERTNameConstraint *
cert_DecodeNameConstraintSubTree(PRArenaPool   *arena,
				 SECItem       **subTree,
				 PRBool        permited)
{
    CERTNameConstraint   *current = NULL;
    CERTNameConstraint   *first = NULL;
    CERTNameConstraint   *last = NULL;
    CERTNameConstraint   *next = NULL;
    int                  i = 0;

    while (subTree[i] != NULL) {
	current = cert_DecodeNameConstraint(arena, subTree[i]);
	if (current == NULL) {
	    goto loser;
	}
	if (last == NULL) {
	    first = last = current;
	}
	current->l.prev = &(last->l);
	current->l.next = last->l.next;
	last->l.next = &(current->l);
	i++;
    }
    first->l.prev = &(current->l);
    return first;
loser:
    if (first) {
	current = first;
	do {
	    next = cert_get_next_name_constraint(current);
	    PORT_Free(current);
	    current = next;
	}while (current != first);
    }
    return NULL;
}

CERTNameConstraints *
cert_DecodeNameConstraints(PRArenaPool   *arena,
			   SECItem       *encodedConstraints)
{
    CERTNameConstraints   *constraints;
    SECStatus             rv;

    PORT_Assert(arena);
    PORT_Assert(encodedConstraints);
    constraints = (CERTNameConstraints *) PORT_ArenaZAlloc(arena, 
							   sizeof(CERTNameConstraints));
    if (constraints == NULL) {
	goto loser;
    }
    rv = SEC_ASN1DecodeItem(arena, constraints, CERTNameConstraintsTemplate, 
			    encodedConstraints);
    if (rv != SECSuccess) {
	goto loser;
    }
    if (constraints->DERPermited != NULL && constraints->DERPermited[0] != NULL) {
	constraints->permited = cert_DecodeNameConstraintSubTree(arena,
								 constraints->DERPermited,
								 PR_TRUE);
	if (constraints->permited == NULL) {
	    goto loser;
	}
    }
    if (constraints->DERExcluded != NULL && constraints->DERExcluded[0] != NULL) {
	constraints->excluded = cert_DecodeNameConstraintSubTree(arena,
								 constraints->DERExcluded,
								 PR_FALSE);
	if (constraints->excluded == NULL) {
	    goto loser;
	}
    }
    return constraints;
loser:
    return NULL;
}


SECStatus
CERT_CopyGeneralName(PRArenaPool      *arena, 
		     CERTGeneralName  *dest, 
		     CERTGeneralName  *src)
{
    SECStatus rv;
    CERTGeneralName *destHead = dest;
    CERTGeneralName *srcHead = src;
    CERTGeneralName *temp;

    PORT_Assert(dest != NULL);
    dest->type = src->type;
    do {
	switch (src->type) {
	  case certDirectoryName: {
	      rv = SECITEM_CopyItem(arena, &dest->derDirectoryName, &src->derDirectoryName);
	      if (rv != SECSuccess) {
		  return rv;
	      }
	      rv = CERT_CopyName(arena, &dest->name.directoryName, &src->name.directoryName);
	      break;
	  }
	  case certOtherName: {
	      rv = SECITEM_CopyItem(arena, &dest->name.OthName.name, &src->name.OthName.name);
	      if (rv != SECSuccess) {
		  return rv;
	      }
	      rv = SECITEM_CopyItem(arena, &dest->name.OthName.oid, &src->name.OthName.oid);
	      break;
	  }
	  default: {
	      rv = SECITEM_CopyItem(arena, &dest->name.other, &src->name.other);
	  }
	}
	src = cert_get_next_general_name(src);
	/* if there is only one general name, we shouldn't do this */
	if (src != srcHead) {
	    if (dest->l.next == &destHead->l) {
		if (arena) {
		    temp = (CERTGeneralName *) 
		      PORT_ArenaZAlloc(arena, sizeof(CERTGeneralName));
		} else {
		    temp = (CERTGeneralName *)
		      PORT_ZAlloc(sizeof(CERTGeneralName));
		}
		temp->l.next = &destHead->l;
		temp->l.prev = &dest->l;
		destHead->l.prev = &temp->l;
		dest->l.next = &temp->l;
		dest = temp;
	    } else {
		dest = cert_get_next_general_name(dest);
	    }
	}
    } while (src != srcHead && rv == SECSuccess);
    return rv;
}


CERTGeneralNameList *
CERT_DupGeneralNameList(CERTGeneralNameList *list)
{
    if (list != NULL) {
	PZ_Lock(list->lock);
	list->refCount++;
	PZ_Unlock(list->lock);
    }
    return list;
}

CERTNameConstraint *
CERT_CopyNameConstraint(PRArenaPool         *arena, 
			CERTNameConstraint  *dest, 
			CERTNameConstraint  *src)
{
    SECStatus  rv;
    
    if (dest == NULL) {
	dest = (CERTNameConstraint *) PORT_ArenaZAlloc(arena, sizeof(CERTNameConstraint));
	/* mark that it is not linked */
	dest->name.l.prev = dest->name.l.next = &(dest->name.l);
    }
    rv = CERT_CopyGeneralName(arena, &dest->name, &src->name);
    if (rv != SECSuccess) {
	goto loser;
    }
    rv = SECITEM_CopyItem(arena, &dest->DERName, &src->DERName);
    if (rv != SECSuccess) {
	goto loser;
    }
    rv = SECITEM_CopyItem(arena, &dest->min, &src->min);
    if (rv != SECSuccess) {
	goto loser;
    }
    rv = SECITEM_CopyItem(arena, &dest->max, &src->max);
    if (rv != SECSuccess) {
	goto loser;
    }
    dest->l.prev = dest->l.next = &dest->l;
    return dest;
loser:
    return NULL;
}


CERTGeneralName *
cert_CombineNamesLists(CERTGeneralName *list1, CERTGeneralName *list2)
{
    PRCList *begin1;
    PRCList *begin2;
    PRCList *end1;
    PRCList *end2;

    if (list1 == NULL){
	return list2;
    } else if (list2 == NULL) {
	return list1;
    } else {
	begin1 = &list1->l;
	begin2 = &list2->l;
	end1 = list1->l.prev;
	end2 = list2->l.prev;
	end1->next = begin2;
	end2->next = begin1;
	begin1->prev = end2;
	begin2->prev = end1;
	return list1;
    }
}


CERTNameConstraint *
cert_CombineConstraintsLists(CERTNameConstraint *list1, CERTNameConstraint *list2)
{
    PRCList *begin1;
    PRCList *begin2;
    PRCList *end1;
    PRCList *end2;

    if (list1 == NULL){
	return list2;
    } else if (list2 == NULL) {
	return list1;
    } else {
	begin1 = &list1->l;
	begin2 = &list2->l;
	end1 = list1->l.prev;
	end2 = list2->l.prev;
	end1->next = begin2;
	end2->next = begin1;
	begin1->prev = end2;
	begin2->prev = end1;
	return list1;
    }
}


CERTNameConstraint *
CERT_AddNameConstraint(CERTNameConstraint *list, 
		       CERTNameConstraint *constraint)
{
    PORT_Assert(constraint != NULL);
    constraint->l.next = constraint->l.prev = &constraint->l;
    list = cert_CombineConstraintsLists(list, constraint);
    return list;
}


SECStatus
CERT_GetNameConstriantByType (CERTNameConstraint *constraints,
			      CERTGeneralNameType type, 
			      CERTNameConstraint **returnList,
			      PRArenaPool *arena)
{
    CERTNameConstraint *current;
    CERTNameConstraint *temp;
    
    *returnList = NULL;
    if (!constraints)
	return SECSuccess;
    current = constraints;

    do {
	if (current->name.type == type || 
	    (type == certDirectoryName && current->name.type == certRFC822Name)) {
	    temp = NULL;
	    temp = CERT_CopyNameConstraint(arena, temp, current);
	    if (temp == NULL) {
		goto loser;
	    }
	    *returnList = CERT_AddNameConstraint(*returnList, temp);
	}
	current = cert_get_next_name_constraint(current);
    } while (current != constraints);
    return SECSuccess;
loser:
    return SECFailure;
}


void *
CERT_GetGeneralNameByType (CERTGeneralName *genNames,
			   CERTGeneralNameType type, PRBool derFormat)
{
    CERTGeneralName *current;
    
    if (!genNames)
	return (NULL);
    current = genNames;

    do {
	if (current->type == type) {
	    switch (type) {
	      case certDNSName:
	      case certEDIPartyName:
	      case certIPAddress:
	      case certRegisterID:
	      case certRFC822Name:
	      case certX400Address:
	      case certURI: {
		    return &(current->name.other);
	      }
	      case certOtherName: {
		  return &(current->name.OthName);
		  break;
	      }
	      case certDirectoryName: {
		  if (derFormat) {
		      return &(current->derDirectoryName);
		  } else{
		      return &(current->name.directoryName);
		  }
		  break;
	      }
	    }
	}
	current = cert_get_next_general_name(current);
    } while (current != genNames);
    return (NULL);
}

int
CERT_GetNamesLength(CERTGeneralName *names)
{
    int              length = 0;
    CERTGeneralName  *first;

    first = names;
    if (names != NULL) {
	do {
	    length++;
	    names = cert_get_next_general_name(names);
	} while (names != first);
    }
    return length;
}

CERTGeneralName *
CERT_GetCertificateNames(CERTCertificate *cert, PRArenaPool *arena)
{
    CERTGeneralName  *DN;
    CERTGeneralName  *altName;
    SECItem          altNameExtension;
    SECStatus        rv;


    DN = (CERTGeneralName *) PORT_ArenaZAlloc(arena, sizeof(CERTGeneralName));
    if (DN == NULL) {
	goto loser;
    }
    rv = CERT_CopyName(arena, &DN->name.directoryName, &cert->subject);
    DN->type = certDirectoryName;
    DN->l.next = DN->l.prev = &DN->l;
    if (rv != SECSuccess) {
	goto loser;
    }
    rv = SECITEM_CopyItem(arena, &DN->derDirectoryName, &cert->derSubject);
    if (rv != SECSuccess) {
	goto loser;
    }
    rv = CERT_FindCertExtension(cert, SEC_OID_X509_SUBJECT_ALT_NAME, 
				&altNameExtension);
    if (rv != SECSuccess) {
	return DN;
    }
    altName = CERT_DecodeAltNameExtension(arena, &altNameExtension);
    if (altName == NULL) {
	goto loser;
    }
    DN = cert_CombineNamesLists(DN, altName);
    return DN;
loser:
    return NULL;
}

static SECStatus
compareNameToConstraint(char *name, char *constraint, PRBool substring)
{
    SECStatus  rv;

    if (*constraint == '\0' && *name == '\0') {
	return SECSuccess;
    }
    if (*constraint == '*') {
	return compareNameToConstraint(name, constraint + 1, PR_TRUE);
    }
    if (substring) {
	if (*constraint == '\0') {
	    return SECSuccess;
	}
	while (*name != *constraint) {
	    if (*name == '\0') {
		return SECFailure;
	    }
	    name++;
	}
	rv = compareNameToConstraint(name + 1, constraint + 1, PR_FALSE);
	if (rv == SECSuccess) {
	    return rv;
	}
	name++;
    } else {
	if (*name == *constraint) {
	    name++;
	    constraint++;
	} else {
	    return SECFailure;
	}
    }
    return compareNameToConstraint(name, constraint, substring);
}

SECStatus
cert_CompareNameWithConstraints(CERTGeneralName     *name, 
				CERTNameConstraint  *constraints,
				PRBool              excluded)
{
    SECStatus           rv = SECSuccess;
    char                *nameString = NULL;
    char                *constraintString = NULL;
    int                 start;
    int                 end;
    int                 tag;
    CERTRDN             **nameRDNS, *nameRDN;
    CERTRDN             **constraintRDNS, *constraintRDN;
    CERTAVA             **nameAVAS, *nameAVA;
    CERTAVA             **constraintAVAS, *constraintAVA;
    CERTNameConstraint  *current;
    SECItem             *avaValue;
    CERTName            constraintName;
    CERTName            certName;
    SECComparison       status = SECEqual;
    PRArenaPool         *certNameArena;
    PRArenaPool         *constraintNameArena;

    certName.arena = NULL;
    certName.rdns = NULL;
    constraintName.arena = NULL;
    constraintName.rdns = NULL;
    if (constraints != NULL) {
	current = constraints;
	if (name->type == certDirectoryName) {
	    certNameArena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
	    CERT_CopyName(certNameArena, &certName, &name->name.directoryName);
	    nameRDNS = certName.rdns;
	    for (;;) {
		nameRDN = *nameRDNS++;
		nameAVAS = nameRDN->avas;
		for(;;) {
		    nameAVA = *nameAVAS++;
		    tag = CERT_GetAVATag(nameAVA);
		    if ( tag == SEC_OID_PKCS9_EMAIL_ADDRESS ||
			 tag == SEC_OID_RFC1274_MAIL) {
			avaValue = CERT_DecodeAVAValue(&nameAVA->value);
			nameString = (char*)PORT_ZAlloc(avaValue->len + 1);
			nameString = PORT_Strncpy(nameString, (char *) avaValue->data, avaValue->len);
			start = 0;
			while(nameString[start] != '@' && nameString[start + 1] != '\0') {
			    start++;
			} 
			start++;
			do{
			    if (current->name.type == certRFC822Name) {
				constraintString = (char*)PORT_ZAlloc(current->name.name.other.len + 1);
				constraintString = PORT_Strncpy(constraintString, 
								(char *) current->name.name.other.data,
								current->name.name.other.len);
				rv = compareNameToConstraint(nameString + start, constraintString, 
							     PR_FALSE);

				if (constraintString != NULL) {
				    PORT_Free(constraintString);
				    constraintString = NULL;
				}
				if (nameString != NULL) {
				    PORT_Free(nameString);
				    nameString = NULL;
				}
				if (rv == SECSuccess && excluded == PR_TRUE) {
				    goto found;
				}
				if (rv == SECSuccess && excluded == PR_FALSE) {
				    break;
				}
			    }
			    current = cert_get_next_name_constraint(current);
			} while (current != constraints);
		    }
		    if (rv != SECSuccess && excluded == PR_FALSE) {
			goto loser;
		    }
		    if (*nameAVAS == NULL) {
			break;
		    }
		}
		if (*nameRDNS == NULL) {
		    break;
		}
	    }
	}
	current = constraints;
	do {
	    switch (name->type) {
	      case certDNSName:
		nameString = (char*)PORT_ZAlloc(name->name.other.len + 1);
		nameString = PORT_Strncpy(nameString, (char *) name->name.other.data, 
					  name->name.other.len);
		constraintString = (char*)PORT_ZAlloc(current->name.name.other.len + 1);
		constraintString = PORT_Strncpy(constraintString, 
						(char *) current->name.name.other.data,
						current->name.name.other.len);
		rv = compareNameToConstraint(nameString, constraintString, PR_FALSE);
		if (nameString != NULL) {
		    PORT_Free(nameString);
		}
		if (constraintString != NULL) {
		    PORT_Free(constraintString);
		}
		break;
	      case certRFC822Name:
		nameString = (char*)PORT_ZAlloc(name->name.other.len + 1);
		nameString = PORT_Strncpy(nameString, (char *) name->name.other.data, 
					  name->name.other.len);
		start = 0;
		while(nameString[start] != '@' && nameString[start + 1] != '\0') {
		    start++;
		} 
		start++;
		constraintString = (char*)PORT_ZAlloc(current->name.name.other.len + 1);
		constraintString = PORT_Strncpy(constraintString, 
						(char *) current->name.name.other.data,
						current->name.name.other.len);
		rv = compareNameToConstraint(nameString + start, constraintString, PR_FALSE);
		if (nameString != NULL) {
		    PORT_Free(nameString);
		}
		if (constraintString != NULL) {
		    PORT_Free(constraintString);
		}
		break;
	      case certURI:
	        nameString = (char*)PORT_ZAlloc(name->name.other.len + 1);
		nameString = PORT_Strncpy(nameString, (char *) name->name.other.data, 
					  name->name.other.len);
		start = 0;
		while(PORT_Strncmp(nameString + start, "://", 3) != 0 && 
		      nameString[start + 3] != '\0') {
		    start++;
		}
		start +=3;
		end = 0;
		while(nameString[start + end] != '/' && 
		      nameString[start + end] != '\0') {
		    end++;
		}
		nameString[start + end] = '\0';
		constraintString = (char*)PORT_ZAlloc(current->name.name.other.len + 1);
		constraintString = PORT_Strncpy(constraintString, 
						(char *) current->name.name.other.data,
						current->name.name.other.len);
		rv = compareNameToConstraint(nameString + start, constraintString, PR_FALSE);
		if (nameString != NULL) {
		    PORT_Free(nameString);
		}
		if (constraintString != NULL) {
		    PORT_Free(constraintString);
		}
		break;
	      case certDirectoryName:  
		if (current->name.type == certDirectoryName) {
		    constraintNameArena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE); 
		    CERT_CopyName(constraintNameArena, &constraintName, &current->name.name.directoryName);
		    constraintRDNS = constraintName.rdns;  
		    for (;;) {
			constraintRDN = *constraintRDNS++;
			constraintAVAS = constraintRDN->avas;
			for(;;) {
			    constraintAVA = *constraintAVAS++;
			    certNameArena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
			    CERT_CopyName(certNameArena, &certName, &name->name.directoryName);
			    nameRDNS = certName.rdns;
			    for (;;) {
				nameRDN = *nameRDNS++;
				nameAVAS = nameRDN->avas++;
				for(;;) {
				    nameAVA = *nameAVAS++;
				    status = CERT_CompareAVA(constraintAVA, nameAVA);
				    if (status == SECEqual || *nameAVAS == NULL) {
					break;
				    }
				}
				if (status == SECEqual || *nameRDNS == NULL) {
				    break;
				}
			    }
			    if (status != SECEqual || *constraintAVAS == NULL) {
				break;
			    }
			}
			if (status != SECEqual || *constraintRDNS == NULL) {
			    break;
			}
		    }
		    if (status == SECEqual) {
			if (excluded == PR_FALSE) {
			    goto found;
			} else {
			    goto loser;
			}
		    }
		    break;
		} else if (current->name.type == certRFC822Name) {
		    current = cert_get_next_name_constraint(current);
		    continue;
		}
	      default:
		/* other types are not supported */
		if (excluded) {
		    goto found;
		} else {
		    goto loser;
		}
	    }
	    if (rv == SECSuccess && status == SECEqual) {
		goto found;
	    }
	    current = cert_get_next_name_constraint(current);
	} while (current !=constraints);
    } else {
	goto found;
    }
loser:
    if (certName.arena) {
	CERT_DestroyName(&certName);
    }
    if (constraintName.arena) {
	CERT_DestroyName(&constraintName);
    }
    return SECFailure;
found:
    if (certName.arena) {
	CERT_DestroyName(&certName);
    }
    if (constraintName.arena) {
	CERT_DestroyName(&constraintName);
    }
    return SECSuccess;
}


CERTCertificate *
CERT_CompareNameSpace(CERTCertificate  *cert,
		      CERTGeneralName  *namesList,
 		      SECItem          *namesListIndex,
 		      PRArenaPool      *arena,
 		      CERTCertDBHandle *handle)
{
    SECStatus            rv;
    SECItem              constraintsExtension;
    CERTNameConstraints  *constraints;
    CERTGeneralName      *currentName;
    int                  count = 0;
    CERTNameConstraint  *matchingConstraints;
    CERTCertificate      *badCert = NULL;
    
    rv = CERT_FindCertExtension(cert, SEC_OID_X509_NAME_CONSTRAINTS, &constraintsExtension);
    if (rv != SECSuccess) {
	return NULL;
    }
    constraints = cert_DecodeNameConstraints(arena, &constraintsExtension);
    currentName = namesList;
    if (constraints == NULL) {
	goto loser;
    }
    do {
 	if (constraints->excluded != NULL) {
 	    rv = CERT_GetNameConstriantByType(constraints->excluded, currentName->type, 
 					      &matchingConstraints, arena);
 	    if (rv != SECSuccess) {
 		goto loser;
 	    }
 	    if (matchingConstraints != NULL) {
 		rv = cert_CompareNameWithConstraints(currentName, matchingConstraints,
 						     PR_TRUE);
 		if (rv != SECFailure) {
 		    goto loser;
 		}
 	    }
 	}
 	if (constraints->permited != NULL) {
 	    rv = CERT_GetNameConstriantByType(constraints->permited, currentName->type, 
 					      &matchingConstraints, arena);
            if (rv != SECSuccess) {
		goto loser;
	    }
 	    if (matchingConstraints != NULL) {
 		rv = cert_CompareNameWithConstraints(currentName, matchingConstraints,
 						     PR_FALSE);
 		if (rv != SECSuccess) {
 		    goto loser;
 		}
 	    } else {
 		goto loser;
 	    }
 	}
 	currentName = cert_get_next_general_name(currentName);
 	count ++;
    } while (currentName != namesList);
    return NULL;
loser:
    badCert = CERT_FindCertByName (handle, &namesListIndex[count]);
    return badCert;
}



/* Search the cert for an X509_SUBJECT_ALT_NAME extension.
** ASN1 Decode it into a list of alternate names.
** Search the list of alternate names for one with the NETSCAPE_NICKNAME OID.
** ASN1 Decode that name.  Turn the result into a zString.  
** Look for duplicate nickname already in the certdb. 
** If one is found, create a nickname string that is not a duplicate.
*/
char *
CERT_GetNickName(CERTCertificate   *cert,
 		 CERTCertDBHandle  *handle,
		 PRArenaPool      *nicknameArena)
{ 
    CERTGeneralName  *current;
    CERTGeneralName  *names;
    char             *nickname   = NULL;
    char             *returnName = NULL;
    char             *basename   = NULL;
    PRArenaPool      *arena      = NULL;
    CERTCertificate  *tmpcert;
    SECStatus        rv;
    int              count;
    int              found = 0;
    SECItem          altNameExtension;
    SECItem          nick;

    if (handle == NULL) {
	handle = CERT_GetDefaultCertDB();
    }
    altNameExtension.data = NULL;
    rv = CERT_FindCertExtension(cert, SEC_OID_X509_SUBJECT_ALT_NAME, 
				&altNameExtension);
    if (rv != SECSuccess) { 
	goto loser; 
    }
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	goto loser;
    }
    names = CERT_DecodeAltNameExtension(arena, &altNameExtension);
    if (names == NULL) {
	goto loser;
    } 
    current = names;
    do {
	if (current->type == certOtherName && 
	    SECOID_FindOIDTag(&current->name.OthName.oid) == 
	      SEC_OID_NETSCAPE_NICKNAME) {
	    found = 1;
	    break;
	}
	current = cert_get_next_general_name(current);
    } while (current != names);
    if (!found)
    	goto loser;

    rv = SEC_ASN1DecodeItem(arena, &nick, SEC_IA5StringTemplate, 
			    &current->name.OthName.name);
    if (rv != SECSuccess) {
	goto loser;
    }

    /* make a null terminated string out of nick, with room enough at
    ** the end to add on a number of up to 21 digits in length, (a signed
    ** 64-bit number in decimal) plus a space and a "#". 
    */
    nickname = (char*)PORT_ZAlloc(nick.len + 24);
    if (!nickname) 
	goto loser;
    PORT_Strncpy(nickname, (char *)nick.data, nick.len);

    /* Don't let this cert's nickname duplicate one already in the DB.
    ** If it does, create a variant of the nickname that doesn't.
    */
    count = 0;
    while ((tmpcert = CERT_FindCertByNickname(handle, nickname)) != NULL) {
	CERT_DestroyCertificate(tmpcert);
	if (!basename) {
	    basename = PORT_Strdup(nickname);
	    if (!basename)
		goto loser;
	}
	count++;
	sprintf(nickname, "%s #%d", basename, count);
    }

    /* success */
    if (nicknameArena) {
	returnName =  PORT_ArenaStrdup(nicknameArena, nickname);
    } else {
	returnName = nickname;
	nickname = NULL;
    }
loser:
    if (arena != NULL) 
	PORT_FreeArena(arena, PR_FALSE);
    if (nickname)
	PORT_Free(nickname);
    if (basename)
	PORT_Free(basename);
    if (altNameExtension.data)
    	PORT_Free(altNameExtension.data);
    return returnName;
}


SECStatus
CERT_CompareGeneralName(CERTGeneralName *a, CERTGeneralName *b)
{
    CERTGeneralName *currentA;
    CERTGeneralName *currentB;
    PRBool found;

    currentA = a;
    currentB = b;
    if (a != NULL) {
	do { 
	    if (currentB == NULL) {
		return SECFailure;
	    }
	    currentB = cert_get_next_general_name(currentB);
	    currentA = cert_get_next_general_name(currentA);
	} while (currentA != a);
    }
    if (currentB != b) {
	return SECFailure;
    }
    currentA = a;
    do {
	currentB = b;
	found = PR_FALSE;
	do {
	    if (currentB->type == currentA->type) {
		switch (currentB->type) {
		  case certDNSName:
		  case certEDIPartyName:
		  case certIPAddress:
		  case certRegisterID:
		  case certRFC822Name:
		  case certX400Address:
		  case certURI:
		    if (SECITEM_CompareItem(&currentA->name.other,
					    &currentB->name.other) 
			== SECEqual) {
			found = PR_TRUE;
		    }
		    break;
		  case certOtherName:
		    if (SECITEM_CompareItem(&currentA->name.OthName.oid,
					    &currentB->name.OthName.oid) 
			== SECEqual &&
			SECITEM_CompareItem(&currentA->name.OthName.name,
					    &currentB->name.OthName.name)
			== SECEqual) {
			found = PR_TRUE;
		    }
		    break;
		  case certDirectoryName:
		    if (CERT_CompareName(&currentA->name.directoryName,
					 &currentB->name.directoryName)
			== SECEqual) {
			found = PR_TRUE;
		    }
		}
		    
	    }
	    currentB = cert_get_next_general_name(currentB);
	} while (currentB != b && found != PR_TRUE);
	if (found != PR_TRUE) {
	    return SECFailure;
	}
	currentA = cert_get_next_general_name(currentA);
    } while (currentA != a);
    return SECSuccess;
}

SECStatus
CERT_CompareGeneralNameLists(CERTGeneralNameList *a, CERTGeneralNameList *b)
{
    SECStatus rv;

    if (a == b) {
	return SECSuccess;
    }
    if (a != NULL && b != NULL) {
	PZ_Lock(a->lock);
	PZ_Lock(b->lock);
	rv = CERT_CompareGeneralName(a->name, b->name);
	PZ_Unlock(a->lock);
	PZ_Unlock(b->lock);
    } else {
	rv = SECFailure;
    }
    return rv;
}

void *
CERT_GetGeneralNameFromListByType(CERTGeneralNameList *list,
				  CERTGeneralNameType type,
				  PRArenaPool *arena)
{
    CERTName *name = NULL; 
    SECItem *item = NULL;
    OtherName *other = NULL;
    OtherName *tmpOther = NULL;
    void *data;

    PZ_Lock(list->lock);
    data = CERT_GetGeneralNameByType(list->name, type, PR_FALSE);
    if (data != NULL) {
	switch (type) {
	  case certDNSName:
	  case certEDIPartyName:
	  case certIPAddress:
	  case certRegisterID:
	  case certRFC822Name:
	  case certX400Address:
	  case certURI:
	    if (arena != NULL) {
		item = (SECItem *)PORT_ArenaAlloc(arena, sizeof(SECItem));
		if (item != NULL) {
		    SECITEM_CopyItem(arena, item, (SECItem *) data);
		}
	    } else { 
		item = SECITEM_DupItem((SECItem *) data);
	    }
	    PZ_Unlock(list->lock);
	    return item;
	  case certOtherName:
	    other = (OtherName *) data;
	    if (arena != NULL) {
		tmpOther = (OtherName *)PORT_ArenaAlloc(arena, 
							sizeof(OtherName));
	    } else {
		tmpOther = (OtherName *) PORT_Alloc(sizeof(OtherName));
	    }
	    if (tmpOther != NULL) {
		SECITEM_CopyItem(arena, &tmpOther->oid, &other->oid);
		SECITEM_CopyItem(arena, &tmpOther->name, &other->name);
	    }
	    PZ_Unlock(list->lock);
	    return tmpOther;
	  case certDirectoryName:
	    if (arena == NULL) {
		PZ_Unlock(list->lock);
		return NULL;
	    } else {
		name = (CERTName *) PORT_ArenaZAlloc(list->arena, 
						    sizeof(CERTName));
		if (name != NULL) {
		    CERT_CopyName(arena, name, (CERTName *) data);
		}
		PZ_Unlock(list->lock);
		return name;
	    }
	}
    }
    PZ_Unlock(list->lock);
    return NULL;
}

void
CERT_AddGeneralNameToList(CERTGeneralNameList *list, 
			  CERTGeneralNameType type,
			  void *data, SECItem *oid)
{
    CERTGeneralName *name;

    if (list != NULL && data != NULL) {
	PZ_Lock(list->lock);
	name = (CERTGeneralName *) 
	    PORT_ArenaZAlloc(list->arena, sizeof(CERTGeneralName));
	name->type = type;
	switch (type) {
	  case certDNSName:
	  case certEDIPartyName:
	  case certIPAddress:
	  case certRegisterID:
	  case certRFC822Name:
	  case certX400Address:
	  case certURI:
	    SECITEM_CopyItem(list->arena, &name->name.other, (SECItem *)data);
	    break;
	  case certOtherName:
	    SECITEM_CopyItem(list->arena, &name->name.OthName.name,
			     (SECItem *) data);
	    SECITEM_CopyItem(list->arena, &name->name.OthName.oid,
			     oid);
	    break;
	  case certDirectoryName:
	    CERT_CopyName(list->arena, &name->name.directoryName,
			  (CERTName *) data);
	    break;
	}
	name->l.prev = name->l.next = &name->l;
	list->name = cert_CombineNamesLists(list->name, name);
	list->len++;
	PZ_Unlock(list->lock);
    }
    return;
}
