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

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: gnseq.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:16:18 $ $Name:  $";
#endif /* DEBUG */

/*
 * gnseq.c
 *
 * This file contains the implementation of the PKIX part-1 object
 * GeneralNames (note the ending 's').
 */

#ifndef NSSBASE_H
#include "nssbase.h"
#endif /* NSSBASE_H */

#ifndef ASN1_H
#include "asn1.h"
#endif /* ASN1_H */

#ifndef PKI1_H
#include "pki1.h"
#endif /* PKI1_H */

/*
 * GeneralNames (or, as we call it, GeneralNameSeq)
 *
 * From draft-ietf-pkix-ipki-part1-10:
 *
 *  GeneralNames ::= SEQUENCE SIZE (1..MAX) OF GeneralName
 *
 * A GeneralNames is simply an (ordered) sequence of General Names.
 * The seqSize variable is  "helper" kept for simplicity.
 */

struct nssGeneralNameSeqStr {
  PRUint32 seqSize;
  NSSGeneralName **generalNames;
};
