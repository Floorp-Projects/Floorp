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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
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

#include "nsCRLManager.h"
#include "nsCRLInfo.h"

#include "nsCOMPtr.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsComponentManagerUtils.h"
#include "nsReadableUtils.h"
#include "nsNSSComponent.h"
#include "nsIWindowWatcher.h"
#include "nsCOMPtr.h"
#include "nsIPrompt.h"
#include "nsICertificateDialogs.h"
#include "nsIMutableArray.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsNSSShutDown.h"

#include "nsNSSCertHeader.h"

#include "nspr.h"
extern "C" {
#include "pk11func.h"
#include "certdb.h"
#include "cert.h"
#include "secerr.h"
#include "nssb64.h"
#include "secasn1.h"
#include "secder.h"
}
#include "ssl.h"
#include "ocsp.h"
#include "plbase64.h"

static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

NS_IMPL_ISUPPORTS1(nsCRLManager, nsICRLManager)

nsCRLManager::nsCRLManager()
{
}

nsCRLManager::~nsCRLManager()
{
}

NS_IMETHODIMP 
nsCRLManager::ImportCrl (PRUint8 *aData, PRUint32 aLength, nsIURI * aURI, PRUint32 aType, bool doSilentDownload, const PRUnichar* crlKey)
{
  nsNSSShutDownPreventionLock locker;
  nsresult rv;
  PRArenaPool *arena = NULL;
  CERTCertificate *caCert;
  SECItem derName = { siBuffer, NULL, 0 };
  SECItem derCrl;
  CERTSignedData sd;
  SECStatus sec_rv;
  CERTSignedCrl *crl;
  nsCAutoString url;
  nsCOMPtr<nsICRLInfo> crlData;
  bool importSuccessful;
  PRInt32 errorCode;
  nsString errorMessage;
  
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv)) return rv;
	         
  aURI->GetSpec(url);
  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!arena) {
    goto loser;
  }
  memset(&sd, 0, sizeof(sd));

  derCrl.data = (unsigned char*)aData;
  derCrl.len = aLength;
  sec_rv = CERT_KeyFromDERCrl(arena, &derCrl, &derName);
  if (sec_rv != SECSuccess) {
    goto loser;
  }

  caCert = CERT_FindCertByName(CERT_GetDefaultCertDB(), &derName);
  if (!caCert) {
    if (aType == SEC_KRL_TYPE){
      goto loser;
    }
  } else {
    sec_rv = SEC_ASN1DecodeItem(arena,
                            &sd, SEC_ASN1_GET(CERT_SignedDataTemplate), 
                            &derCrl);
    if (sec_rv != SECSuccess) {
      goto loser;
    }
    sec_rv = CERT_VerifySignedData(&sd, caCert, PR_Now(),
                               nsnull);
    if (sec_rv != SECSuccess) {
      goto loser;
    }
  }
  
  crl = SEC_NewCrl(CERT_GetDefaultCertDB(), const_cast<char*>(url.get()), &derCrl,
                   aType);
  
  if (!crl) {
    goto loser;
  }

  crlData = new nsCRLInfo(crl);
  SSL_ClearSessionCache();
  SEC_DestroyCrl(crl);
  
  importSuccessful = PR_TRUE;
  goto done;

loser:
  importSuccessful = PR_FALSE;
  errorCode = PR_GetError();
  switch (errorCode) {
    case SEC_ERROR_CRL_EXPIRED:
      nssComponent->GetPIPNSSBundleString("CrlImportFailureExpired", errorMessage);
      break;

	case SEC_ERROR_CRL_BAD_SIGNATURE:
      nssComponent->GetPIPNSSBundleString("CrlImportFailureBadSignature", errorMessage);
      break;

	case SEC_ERROR_CRL_INVALID:
      nssComponent->GetPIPNSSBundleString("CrlImportFailureInvalid", errorMessage);
      break;

	case SEC_ERROR_OLD_CRL:
      nssComponent->GetPIPNSSBundleString("CrlImportFailureOld", errorMessage);
      break;

	case SEC_ERROR_CRL_NOT_YET_VALID:
      nssComponent->GetPIPNSSBundleString("CrlImportFailureNotYetValid", errorMessage);
      break;

    default:
      nssComponent->GetPIPNSSBundleString("CrlImportFailureReasonUnknown", errorMessage);
      errorMessage.AppendInt(errorCode,16);
      break;
  }

done:
          
  if(!doSilentDownload){
    if (!importSuccessful){
      nsString message;
      nsString temp;
      nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
      nsCOMPtr<nsIPrompt> prompter;
      if (wwatch){
        wwatch->GetNewPrompter(0, getter_AddRefs(prompter));
        nssComponent->GetPIPNSSBundleString("CrlImportFailure1x", message);
        message.Append(NS_LITERAL_STRING("\n").get());
        message.Append(errorMessage);
        nssComponent->GetPIPNSSBundleString("CrlImportFailure2", temp);
        message.Append(NS_LITERAL_STRING("\n").get());
        message.Append(temp);
     
        if(prompter) {
          nsPSMUITracker tracker;
          if (!tracker.isUIForbidden()) {
            prompter->Alert(0, message.get());
          }
        }
      }
    } else {
      nsCOMPtr<nsICertificateDialogs> certDialogs;
      // Not being able to display the success dialog should not
      // be a fatal error, so don't return a failure code.
      {
        nsPSMUITracker tracker;
        if (tracker.isUIForbidden()) {
          rv = NS_ERROR_NOT_AVAILABLE;
        }
        else {
          rv = ::getNSSDialogs(getter_AddRefs(certDialogs),
            NS_GET_IID(nsICertificateDialogs), NS_CERTIFICATEDIALOGS_CONTRACTID);
        }
      }
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIInterfaceRequestor> cxt = new PipUIContext();
        certDialogs->CrlImportStatusDialog(cxt, crlData);
      }
    }
  } else {
    if(crlKey == nsnull){
      return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIPrefService> prefSvc = do_GetService(NS_PREFSERVICE_CONTRACTID,&rv);
    nsCOMPtr<nsIPrefBranch> pref = do_GetService(NS_PREFSERVICE_CONTRACTID,&rv);
    if (NS_FAILED(rv)){
      return rv;
    }
    
    nsCAutoString updateErrCntPrefStr(CRL_AUTOUPDATE_ERRCNT_PREF);
    updateErrCntPrefStr.AppendWithConversion(crlKey);
    if(importSuccessful){
      PRUnichar *updateTime;
      nsCAutoString updateTimeStr;
      nsCString updateURL;
      PRInt32 timingTypePref;
      double dayCnt;
      char *dayCntStr;
      nsCAutoString updateTypePrefStr(CRL_AUTOUPDATE_TIMIINGTYPE_PREF);
      nsCAutoString updateTimePrefStr(CRL_AUTOUPDATE_TIME_PREF);
      nsCAutoString updateUrlPrefStr(CRL_AUTOUPDATE_URL_PREF);
      nsCAutoString updateDayCntPrefStr(CRL_AUTOUPDATE_DAYCNT_PREF);
      nsCAutoString updateFreqCntPrefStr(CRL_AUTOUPDATE_FREQCNT_PREF);
      updateTypePrefStr.AppendWithConversion(crlKey);
      updateTimePrefStr.AppendWithConversion(crlKey);
      updateUrlPrefStr.AppendWithConversion(crlKey);
      updateDayCntPrefStr.AppendWithConversion(crlKey);
      updateFreqCntPrefStr.AppendWithConversion(crlKey);

      pref->GetIntPref(updateTypePrefStr.get(),&timingTypePref);
      
      //Compute and update the next download instant
      if(timingTypePref == TYPE_AUTOUPDATE_TIME_BASED){
        pref->GetCharPref(updateDayCntPrefStr.get(),&dayCntStr);
      }else{
        pref->GetCharPref(updateFreqCntPrefStr.get(),&dayCntStr);
      }
      dayCnt = atof(dayCntStr);
      nsMemory::Free(dayCntStr);

      bool toBeRescheduled = false;
      if(NS_SUCCEEDED(ComputeNextAutoUpdateTime(crlData, timingTypePref, dayCnt, &updateTime))){
        updateTimeStr.AssignWithConversion(updateTime);
        nsMemory::Free(updateTime);
        pref->SetCharPref(updateTimePrefStr.get(),updateTimeStr.get());
        //Now, check if this update time is already in the past. This would
        //imply we have downloaded the same crl, or there is something wrong
        //with the next update date. We will not reschedule this crl in this
        //session anymore - or else, we land into a loop. It would anyway be
        //imported once the browser is restarted.
        PRTime nextTime;
        PR_ParseTimeString(updateTimeStr.get(),PR_TRUE, &nextTime);
        if(LL_CMP(nextTime, > , PR_Now())){
          toBeRescheduled = PR_TRUE;
        }
      }
      
      //Update the url to download from, next time
      crlData->GetLastFetchURL(updateURL);
      pref->SetCharPref(updateUrlPrefStr.get(),updateURL.get());
      
      pref->SetIntPref(updateErrCntPrefStr.get(),0);
      
      if (toBeRescheduled) {
        nsAutoString hashKey(crlKey);
        nssComponent->RemoveCrlFromList(hashKey);
        nssComponent->DefineNextTimer();
      }

    } else{
      PRInt32 errCnt;
      nsCAutoString errMsg;
      nsCAutoString updateErrDetailPrefStr(CRL_AUTOUPDATE_ERRDETAIL_PREF);
      updateErrDetailPrefStr.AppendWithConversion(crlKey);
      errMsg.AssignWithConversion(errorMessage.get());
      rv = pref->GetIntPref(updateErrCntPrefStr.get(),&errCnt);
      if(NS_FAILED(rv))
        errCnt = 0;

      pref->SetIntPref(updateErrCntPrefStr.get(),errCnt+1);
      pref->SetCharPref(updateErrDetailPrefStr.get(),errMsg.get());
    }
    prefSvc->SavePrefFile(nsnull);
  }

  return rv;
}

NS_IMETHODIMP 
nsCRLManager::UpdateCRLFromURL( const PRUnichar *url, const PRUnichar* key, bool *res)
{
  nsresult rv;
  nsAutoString downloadUrl(url);
  nsAutoString dbKey(key);
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if(NS_FAILED(rv)){
    *res = PR_FALSE;
    return rv;
  }

  rv = nssComponent->DownloadCRLDirectly(downloadUrl, dbKey);
  if(NS_FAILED(rv)){
    *res = PR_FALSE;
  } else {
    *res = PR_TRUE;
  }
  return NS_OK;

}

NS_IMETHODIMP 
nsCRLManager::RescheduleCRLAutoUpdate(void)
{
  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if(NS_FAILED(rv)){
    return rv;
  }
  rv = nssComponent->DefineNextTimer();
  return rv;
}

/**
 * getCRLs
 *
 * Export a set of certs and keys from the database to a PKCS#12 file.
 */
NS_IMETHODIMP 
nsCRLManager::GetCrls(nsIArray ** aCrls)
{
  nsNSSShutDownPreventionLock locker;
  SECStatus sec_rv;
  CERTCrlHeadNode *head = nsnull;
  CERTCrlNode *node = nsnull;
  nsresult rv;
  nsCOMPtr<nsIMutableArray> crlsArray =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Get the list of certs //
  sec_rv = SEC_LookupCrls(CERT_GetDefaultCertDB(), &head, -1);
  if (sec_rv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  if (head) {
    for (node=head->first; node != nsnull; node = node->next) {

      nsCOMPtr<nsICRLInfo> entry = new nsCRLInfo((node->crl));
      crlsArray->AppendElement(entry, PR_FALSE);
    }
    PORT_FreeArena(head->arena, PR_FALSE);
  }

  *aCrls = crlsArray;
  NS_IF_ADDREF(*aCrls);
  return NS_OK;
}

/**
 * deleteCrl
 *
 * Delete a Crl entry from the cert db.
 */
NS_IMETHODIMP 
nsCRLManager::DeleteCrl(PRUint32 aCrlIndex)
{
  nsNSSShutDownPreventionLock locker;
  CERTSignedCrl *realCrl = nsnull;
  CERTCrlHeadNode *head = nsnull;
  CERTCrlNode *node = nsnull;
  SECStatus sec_rv;
  PRUint32 i;

  // Get the list of certs //
  sec_rv = SEC_LookupCrls(CERT_GetDefaultCertDB(), &head, -1);
  if (sec_rv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  if (head) {
    for (i = 0, node=head->first; node != nsnull; i++, node = node->next) {
      if (i != aCrlIndex) {
        continue;
      }
      realCrl = SEC_FindCrlByName(CERT_GetDefaultCertDB(), &(node->crl->crl.derName), node->type);
      SEC_DeletePermCRL(realCrl);
      SEC_DestroyCrl(realCrl);
      SSL_ClearSessionCache();
    }
    PORT_FreeArena(head->arena, PR_FALSE);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCRLManager::ComputeNextAutoUpdateTime(nsICRLInfo *info, 
  PRUint32 autoUpdateType, double dayCnt, PRUnichar **nextAutoUpdate)
{
  if (!info)
    return NS_ERROR_FAILURE;

  PRTime microsecInDayCnt;
  PRTime now = PR_Now();
  PRTime tempTime;
  PRInt64 diff = 0;
  PRInt64 secsInDay = 86400UL;
  PRInt64 temp;
  PRInt64 cycleCnt = 0;
  PRInt64 secsInDayCnt;
  PRFloat64 tmpData;
  
  LL_L2F(tmpData,secsInDay);
  LL_MUL(tmpData,dayCnt,tmpData);
  LL_F2L(secsInDayCnt,tmpData);
  LL_MUL(microsecInDayCnt, secsInDayCnt, PR_USEC_PER_SEC);
    
  PRTime lastUpdate;
  PRTime nextUpdate;
  
  nsresult rv;

  rv = info->GetLastUpdate(&lastUpdate);
  if (NS_FAILED(rv))
    return rv;

  rv = info->GetNextUpdate(&nextUpdate);
  if (NS_FAILED(rv))
    return rv;

  switch (autoUpdateType) {
  case TYPE_AUTOUPDATE_FREQ_BASED:
    LL_SUB(diff, now, lastUpdate);             //diff is the no of micro sec between now and last update
    LL_DIV(cycleCnt, diff, microsecInDayCnt);   //temp is the number of full cycles from lst update
    LL_MOD(temp, diff, microsecInDayCnt);
    if(!(LL_IS_ZERO(temp))) {
      LL_ADD(cycleCnt,cycleCnt,1);            //no of complete cycles till next autoupdate instant
    }
    LL_MUL(temp,cycleCnt,microsecInDayCnt);    //micro secs from last update
    LL_ADD(tempTime, lastUpdate, temp);
    break;  
  case TYPE_AUTOUPDATE_TIME_BASED:
    LL_SUB(tempTime, nextUpdate, microsecInDayCnt);
    break;
  default:
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  //Now, a basic constraing is that the next auto update date can never be after
  //next update, if one is defined
  if(LL_CMP(nextUpdate , > , 0 )) {
    if(LL_CMP(tempTime , > , nextUpdate)) {
      tempTime = nextUpdate;
    }
  }

  nsAutoString nextAutoUpdateDate;
  PRExplodedTime explodedTime;
  nsCOMPtr<nsIDateTimeFormat> dateFormatter = do_CreateInstance(NS_DATETIMEFORMAT_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;
  PR_ExplodeTime(tempTime, PR_GMTParameters, &explodedTime);
  dateFormatter->FormatPRExplodedTime(nsnull, kDateFormatShort, kTimeFormatSeconds,
                                      &explodedTime, nextAutoUpdateDate);
  *nextAutoUpdate = ToNewUnicode(nextAutoUpdateDate);

  return NS_OK;
}

