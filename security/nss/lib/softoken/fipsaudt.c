/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Network Security Services (NSS).
 *
 * The Initial Developer of the Original Code is
 * Red Hat, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
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
 * This file implements audit logging required by FIPS 140-2 Security
 * Level 2.
 */

#include "prprf.h"
#include "softoken.h"

void sftk_AuditCreateObject(CK_SESSION_HANDLE hSession,
    CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount,
    CK_OBJECT_HANDLE_PTR phObject, CK_RV rv)
{
    char msg[256];
    char shObject[32];
    NSSAuditSeverity severity = (rv == CKR_OK) ?
	NSS_AUDIT_INFO : NSS_AUDIT_ERROR;
    if (rv == CKR_OK) {
	PR_snprintf(shObject, sizeof shObject, " *phObject=0x%08lX",
	    (PRUint32)*phObject);
    } else {
	shObject[0] = '\0';
    }
    PR_snprintf(msg, sizeof msg,
	"C_CreateObject(hSession=0x%08lX, pTemplate=%p, ulCount=%lu, "
	"phObject=%p)=0x%08lX%s",
	(PRUint32)hSession, pTemplate, (PRUint32)ulCount,
	phObject, (PRUint32)rv, shObject);
    sftk_LogAuditMessage(severity, msg);
}

void sftk_AuditCopyObject(CK_SESSION_HANDLE hSession,
    CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount,
    CK_OBJECT_HANDLE_PTR phNewObject, CK_RV rv)
{
    char msg[256];
    char shNewObject[32];
    NSSAuditSeverity severity = (rv == CKR_OK) ?
	NSS_AUDIT_INFO : NSS_AUDIT_ERROR;
    if (rv == CKR_OK) {
	PR_snprintf(shNewObject, sizeof shNewObject,
	    " *phNewObject=0x%08lX", (PRUint32)*phNewObject);
    } else {
	shNewObject[0] = '\0';
    }
    PR_snprintf(msg, sizeof msg,
	"C_CopyObject(hSession=0x%08lX, hObject=0x%08lX, "
	"pTemplate=%p, ulCount=%lu, phNewObject=%p)=0x%08lX%s",
	(PRUint32)hSession, (PRUint32)hObject,
	pTemplate, (PRUint32)ulCount, phNewObject, (PRUint32)rv, shNewObject);
    sftk_LogAuditMessage(severity, msg);
}

/* WARNING: hObject has been destroyed and can only be printed. */
void sftk_AuditDestroyObject(CK_SESSION_HANDLE hSession,
    CK_OBJECT_HANDLE hObject, CK_RV rv)
{
    char msg[256];
    NSSAuditSeverity severity = (rv == CKR_OK) ?
	NSS_AUDIT_INFO : NSS_AUDIT_ERROR;
    PR_snprintf(msg, sizeof msg,
	"C_DestroyObject(hSession=0x%08lX, hObject=0x%08lX)=0x%08lX",
	(PRUint32)hSession, (PRUint32)hObject, (PRUint32)rv);
    sftk_LogAuditMessage(severity, msg);
}

void sftk_AuditGetObjectSize(CK_SESSION_HANDLE hSession,
    CK_OBJECT_HANDLE hObject, CK_ULONG_PTR pulSize, CK_RV rv)
{
    char msg[256];
    NSSAuditSeverity severity = (rv == CKR_OK) ?
	NSS_AUDIT_INFO : NSS_AUDIT_ERROR;
    PR_snprintf(msg, sizeof msg,
	"C_GetObjectSize(hSession=0x%08lX, hObject=0x%08lX, "
	"pulSize=%p)=0x%08lX",
	(PRUint32)hSession, (PRUint32)hObject,
	pulSize, (PRUint32)rv);
    sftk_LogAuditMessage(severity, msg);
}

void sftk_AuditGetAttributeValue(CK_SESSION_HANDLE hSession,
    CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR pTemplate,
    CK_ULONG ulCount, CK_RV rv)
{
    char msg[256];
    NSSAuditSeverity severity = (rv == CKR_OK) ?
	NSS_AUDIT_INFO : NSS_AUDIT_ERROR;
    PR_snprintf(msg, sizeof msg,
	"C_GetAttributeValue(hSession=0x%08lX, hObject=0x%08lX, "
	"pTemplate=%p, ulCount=%lu)=0x%08lX",
	(PRUint32)hSession, (PRUint32)hObject,
	pTemplate, (PRUint32)ulCount, (PRUint32)rv);
    sftk_LogAuditMessage(severity, msg);
}

void sftk_AuditSetAttributeValue(CK_SESSION_HANDLE hSession,
    CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR pTemplate,
    CK_ULONG ulCount, CK_RV rv)
{
    char msg[256];
    NSSAuditSeverity severity = (rv == CKR_OK) ?
	NSS_AUDIT_INFO : NSS_AUDIT_ERROR;
    PR_snprintf(msg, sizeof msg,
	"C_SetAttributeValue(hSession=0x%08lX, hObject=0x%08lX, "
	"pTemplate=%p, ulCount=%lu)=0x%08lX",
	(PRUint32)hSession, (PRUint32)hObject,
	pTemplate, (PRUint32)ulCount, (PRUint32)rv);
    sftk_LogAuditMessage(severity, msg);
}

void sftk_AuditCryptInit(const char *opName, CK_SESSION_HANDLE hSession,
    CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey, CK_RV rv)
{
    char msg[256];
    NSSAuditSeverity severity = (rv == CKR_OK) ?
	NSS_AUDIT_INFO : NSS_AUDIT_ERROR;
    PR_snprintf(msg, sizeof msg,
	"C_%sInit(hSession=0x%08lX, pMechanism->mechanism=0x%08lX, "
	"hKey=0x%08lX)=0x%08lX",
	opName, (PRUint32)hSession, (PRUint32)pMechanism->mechanism,
	(PRUint32)hKey, (PRUint32)rv);
    sftk_LogAuditMessage(severity, msg);
}

void sftk_AuditGenerateKey(CK_SESSION_HANDLE hSession,
    CK_MECHANISM_PTR pMechanism, CK_ATTRIBUTE_PTR pTemplate,
    CK_ULONG ulCount, CK_OBJECT_HANDLE_PTR phKey, CK_RV rv)
{
    char msg[256];
    char shKey[32];
    NSSAuditSeverity severity = (rv == CKR_OK) ?
	NSS_AUDIT_INFO : NSS_AUDIT_ERROR;
    if (rv == CKR_OK) {
	PR_snprintf(shKey, sizeof shKey,
	    " *phKey=0x%08lX", (PRUint32)*phKey);
    } else {
        shKey[0] = '\0';
    }
    PR_snprintf(msg, sizeof msg,
	"C_GenerateKey(hSession=0x%08lX, pMechanism->mechanism=0x%08lX, "
	"pTemplate=%p, ulCount=%lu, phKey=%p)=0x%08lX%s",
	(PRUint32)hSession, (PRUint32)pMechanism->mechanism,
	pTemplate, (PRUint32)ulCount, phKey, (PRUint32)rv, shKey);
    sftk_LogAuditMessage(severity, msg);
}

void sftk_AuditGenerateKeyPair(CK_SESSION_HANDLE hSession,
    CK_MECHANISM_PTR pMechanism, CK_ATTRIBUTE_PTR pPublicKeyTemplate,
    CK_ULONG ulPublicKeyAttributeCount, CK_ATTRIBUTE_PTR pPrivateKeyTemplate,
    CK_ULONG ulPrivateKeyAttributeCount, CK_OBJECT_HANDLE_PTR phPublicKey,
    CK_OBJECT_HANDLE_PTR phPrivateKey, CK_RV rv)
{
    char msg[512];
    char shPublicKey[32];
    char shPrivateKey[32];
    NSSAuditSeverity severity = (rv == CKR_OK) ?
	NSS_AUDIT_INFO : NSS_AUDIT_ERROR;
    if (rv == CKR_OK) {
	PR_snprintf(shPublicKey, sizeof shPublicKey,
	    " *phPublicKey=0x%08lX", (PRUint32)*phPublicKey);
	PR_snprintf(shPrivateKey, sizeof shPrivateKey,
	    " *phPrivateKey=0x%08lX", (PRUint32)*phPrivateKey);
    } else {
	shPublicKey[0] = shPrivateKey[0] = '\0';
    }
    PR_snprintf(msg, sizeof msg,
	"C_GenerateKeyPair(hSession=0x%08lX, pMechanism->mechanism=0x%08lX, "
	"pPublicKeyTemplate=%p, ulPublicKeyAttributeCount=%lu, "
	"pPrivateKeyTemplate=%p, ulPrivateKeyAttributeCount=%lu, "
	"phPublicKey=%p, phPrivateKey=%p)=0x%08lX%s%s",
	(PRUint32)hSession, (PRUint32)pMechanism->mechanism,
	pPublicKeyTemplate, (PRUint32)ulPublicKeyAttributeCount,
	pPrivateKeyTemplate, (PRUint32)ulPrivateKeyAttributeCount,
	phPublicKey, phPrivateKey, (PRUint32)rv, shPublicKey, shPrivateKey);
    sftk_LogAuditMessage(severity, msg);
}

void sftk_AuditWrapKey(CK_SESSION_HANDLE hSession,
    CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hWrappingKey,
    CK_OBJECT_HANDLE hKey, CK_BYTE_PTR pWrappedKey,
    CK_ULONG_PTR pulWrappedKeyLen, CK_RV rv)
{
    char msg[256];
    NSSAuditSeverity severity = (rv == CKR_OK) ?
	NSS_AUDIT_INFO : NSS_AUDIT_ERROR;
    PR_snprintf(msg, sizeof msg,
	"C_WrapKey(hSession=0x%08lX, hWrappingKey=0x%08lX, hKey=0x%08lX, "
	"pWrappedKey=%p, pulWrappedKeyLen=%p)=0x%08lX",
	(PRUint32)hSession, (PRUint32)hWrappingKey, (PRUint32)hKey,
	pWrappedKey, pulWrappedKeyLen, (PRUint32)rv);
    sftk_LogAuditMessage(severity, msg);
}

void sftk_AuditUnwrapKey(CK_SESSION_HANDLE hSession,
    CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hUnwrappingKey,
    CK_BYTE_PTR pWrappedKey, CK_ULONG ulWrappedKeyLen,
    CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulAttributeCount,
    CK_OBJECT_HANDLE_PTR phKey, CK_RV rv)
{
    char msg[256];
    char shKey[32];
    NSSAuditSeverity severity = (rv == CKR_OK) ?
	NSS_AUDIT_INFO : NSS_AUDIT_ERROR;
    if (rv == CKR_OK) {
	PR_snprintf(shKey, sizeof shKey,
	    " *phKey=0x%08lX", (PRUint32)*phKey);
    } else {
	shKey[0] = '\0';
    }
    PR_snprintf(msg, sizeof msg,
	"C_UnwrapKey(hSession=0x%08lX, pMechanism->mechanism=0x%08lX, "
	"hUnwrappingKey=0x%08lX, pWrappedKey=%p, ulWrappedKeyLen=%lu, "
	"pTemplate=%p, ulAttributeCount=%lu, phKey=%p)=0x%08lX%s",
	(PRUint32)hSession, (PRUint32)pMechanism->mechanism,
	(PRUint32)hUnwrappingKey, pWrappedKey, (PRUint32)ulWrappedKeyLen,
	pTemplate, (PRUint32)ulAttributeCount, phKey, (PRUint32)rv, shKey);
    sftk_LogAuditMessage(severity, msg);
}

void sftk_AuditDeriveKey(CK_SESSION_HANDLE hSession,
    CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hBaseKey,
    CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulAttributeCount,
    CK_OBJECT_HANDLE_PTR phKey, CK_RV rv)
{
    char msg[512];
    char shKey[32];
    char sTlsKeys[128];
    NSSAuditSeverity severity = (rv == CKR_OK) ?
	NSS_AUDIT_INFO : NSS_AUDIT_ERROR;
    /* phKey is NULL for CKM_TLS_KEY_AND_MAC_DERIVE */
    if ((rv == CKR_OK) && phKey) {
	PR_snprintf(shKey, sizeof shKey,
	    " *phKey=0x%08lX", (PRUint32)*phKey);
    } else {
	shKey[0] = '\0';
    }
    if ((rv == CKR_OK) &&
	(pMechanism->mechanism == CKM_TLS_KEY_AND_MAC_DERIVE)) {
	CK_SSL3_KEY_MAT_PARAMS *param =
	    (CK_SSL3_KEY_MAT_PARAMS *)pMechanism->pParameter;
	CK_SSL3_KEY_MAT_OUT *keymat = param->pReturnedKeyMaterial;
	PR_snprintf(sTlsKeys, sizeof sTlsKeys,
	    " hClientMacSecret=0x%08lX hServerMacSecret=0x%08lX"
	    " hClientKey=0x%08lX hServerKey=0x%08lX",
	    (PRUint32)keymat->hClientMacSecret,
	    (PRUint32)keymat->hServerMacSecret,
	    (PRUint32)keymat->hClientKey,
	    (PRUint32)keymat->hServerKey);
    } else {
	sTlsKeys[0] = '\0';
    }
    PR_snprintf(msg, sizeof msg,
	"C_DeriveKey(hSession=0x%08lX, pMechanism->mechanism=0x%08lX, "
	"hBaseKey=0x%08lX, pTemplate=%p, ulAttributeCount=%lu, "
	"phKey=%p)=0x%08lX%s%s",
	(PRUint32)hSession, (PRUint32)pMechanism->mechanism,
	(PRUint32)hBaseKey, pTemplate,(PRUint32)ulAttributeCount,
	phKey, (PRUint32)rv, shKey, sTlsKeys);
    sftk_LogAuditMessage(severity, msg);
}

void sftk_AuditDigestKey(CK_SESSION_HANDLE hSession,
    CK_OBJECT_HANDLE hKey, CK_RV rv)
{
    char msg[256];
    NSSAuditSeverity severity = (rv == CKR_OK) ?
	NSS_AUDIT_INFO : NSS_AUDIT_ERROR;
    PR_snprintf(msg, sizeof msg,
	"C_DigestKey(hSession=0x%08lX, hKey=0x%08lX)=0x%08lX",
	(PRUint32)hSession, (PRUint32)hKey, (PRUint32)rv);
    sftk_LogAuditMessage(severity, msg);
}
