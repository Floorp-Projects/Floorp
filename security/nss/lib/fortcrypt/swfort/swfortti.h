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
/*
 * All the data structures for Software fortezza are internal only.
 * The external API for Software fortezza is MACI (which is only used by
 * the PKCS #11 module.
 */

#ifndef _SWFORTTI_H_
#define _SWFORTTI_H_

#include "maci.h"
#include "seccomon.h"
#include "mcom_db.h"  /* really should be included by certt.h */
#include "certt.h"
#include "keyt.h"
#include "swfortt.h"

/* the following parameters are tunable. The bigger the key registers are,
 * the less likely the PKCS #11 module will thrash. */
#define KEY_REGISTERS	100
#define MAX_RA_SLOTS	20

/* SKIPJACK algorithm constants */
#define SKIPJACK_KEY_SIZE 10
#define SKIPJACK_BLOCK_SIZE 8
#define SKIPJACK_LEAF_SIZE 16

/* private typedefs */
typedef unsigned char FORTSkipjackKey[SKIPJACK_KEY_SIZE];
typedef unsigned char *FORTSkipjackKeyPtr;
typedef unsigned char fortRaPrivate[20];
typedef unsigned char *fortRaPrivatePtr;

/* save a public/private key pair */
struct FORTRaRegistersStr {
    CI_RA	public;
    fortRaPrivate private;
};

/* FORTEZZA Key Register */
struct FORTKeySlotStr {
    FORTSkipjackKey data;
    PRBool   present;
};

/* structure to hole private key information */
struct fortKeyInformationStr {
    SECItem	keyFlags;
    SECItem	privateKeyWrappedWithKs;
    SECItem     derPublicKey;
    SECItem	p;
    SECItem	g;
    SECItem	q;
};

/* struture to hole Ks wrapped data */
struct fortProtectedDataStr {
    SECItem	length;
    SECItem	dataIV;
    SECItem	dataEncryptedWithKs;
};

/* This structure represents a fortezza personality */
struct fortSlotEntryStr {
    SECItem	trusted;
    SECItem	certificateIndex;
    int 	certIndex;
    fortProtectedData certificateLabel;
    fortProtectedData certificateData;
    fortKeyInformation *exchangeKeyInformation;
    fortKeyInformation *signatureKeyInformation;
};

/* this structure represents a K value wrapped by a protected pin */
struct fortProtectedPhraseStr {
    SECItem	kValueIV;
    SECItem	wrappedKValue;
    SECItem	memPhraseIV;
    SECItem	hashedEncryptedMemPhrase;
};


/* This structure represents all the relevant data stored in a der encoded
 * fortezza slot file. */
struct FORTSWFileStr {
    PRArenaPool *arena;
    SECItem	version;
    SECItem	derIssuer;
    SECItem	serialID;
    fortProtectedPhrase initMemPhrase;
#define fortezzaPhrase initMemPhrase 
    fortProtectedPhrase ssoMemPhrase;
    fortProtectedPhrase userMemPhrase;
    fortProtectedPhrase ssoPinPhrase;
    fortProtectedPhrase userPinPhrase;
    SECItem	wrappedRandomSeed;
    fortSlotEntry **slotEntries;
};

/* This data structed represents a signed data structure */
struct FORTSignedSWFileStr {
    FORTSWFile  file;
    CERTSignedData signatureWrap;
    FORTSkipjackKeyPtr Kinit;
    FORTSkipjackKeyPtr Ks;
};


/* collect all the data that makes up a token */
struct FORTSWTokenStr {
    PRBool	login;		       /* has this token been logged in? */
    int		lock;		       /* the current lock state */
    int		certIndex;	       /* index of the current personality */
    int		key;		       /* currently selected key */
    int		nextRa;		       /* where the next Ra/ra pair will go */
    FORTSWFile *config_file;           /* parsed Fortezza Config file */
    unsigned char IV[SKIPJACK_BLOCK_SIZE];
    FORTKeySlot keyReg[KEY_REGISTERS]; /* sw fortezza key slots */
    FORTRaRegisters RaValues[MAX_RA_SLOTS];  /* Ra/ra values */
};

#endif
