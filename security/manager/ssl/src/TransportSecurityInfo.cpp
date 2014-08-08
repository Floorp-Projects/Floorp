/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TransportSecurityInfo.h"

#include "pkix/pkixtypes.h"
#include "nsNSSComponent.h"
#include "nsIWebProgressListener.h"
#include "nsNSSCertificate.h"
#include "nsIX509CertValidity.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsICertOverrideService.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsNSSCertHelper.h"
#include "nsIProgrammingLanguage.h"
#include "nsIArray.h"
#include "nsComponentManagerUtils.h"
#include "nsReadableUtils.h"
#include "nsServiceManagerUtils.h"
#include "PSMRunnable.h"

#include "secerr.h"

//#define DEBUG_SSL_VERBOSE //Enable this define to get minimal 
                            //reports when doing SSL read/write
                            
//#define DUMP_BUFFER  //Enable this define along with
                       //DEBUG_SSL_VERBOSE to dump SSL
                       //read/write buffer to a log.
                       //Uses PR_LOG except on Mac where
                       //we always write out to our own
                       //file.

namespace mozilla { namespace psm {

TransportSecurityInfo::TransportSecurityInfo()
  : mMutex("TransportSecurityInfo::mMutex"),
    mSecurityState(nsIWebProgressListener::STATE_IS_INSECURE),
    mSubRequestsBrokenSecurity(0),
    mSubRequestsNoSecurity(0),
    mErrorCode(0),
    mErrorMessageType(PlainErrorMessage),
    mPort(0)
{
}

TransportSecurityInfo::~TransportSecurityInfo()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return;

  shutdown(calledFromObject);
}

void
TransportSecurityInfo::virtualDestroyNSSReference()
{
}

NS_IMPL_ISUPPORTS(TransportSecurityInfo,
                  nsITransportSecurityInfo,
                  nsIInterfaceRequestor,
                  nsISSLStatusProvider,
                  nsIAssociatedContentSecurity,
                  nsISerializable,
                  nsIClassInfo)

nsresult
TransportSecurityInfo::SetHostName(const char* host)
{
  mHostName.Adopt(host ? NS_strdup(host) : 0);
  return NS_OK;
}

nsresult
TransportSecurityInfo::GetHostName(char **host)
{
  *host = (mHostName) ? NS_strdup(mHostName) : nullptr;
  return NS_OK;
}

nsresult
TransportSecurityInfo::SetPort(int32_t aPort)
{
  mPort = aPort;
  return NS_OK;
}

nsresult
TransportSecurityInfo::GetPort(int32_t *aPort)
{
  *aPort = mPort;
  return NS_OK;
}

PRErrorCode
TransportSecurityInfo::GetErrorCode() const
{
  MutexAutoLock lock(mMutex);

  return mErrorCode;
}

void
TransportSecurityInfo::SetCanceled(PRErrorCode errorCode,
                                   SSLErrorMessageType errorMessageType)
{
  MutexAutoLock lock(mMutex);

  mErrorCode = errorCode;
  mErrorMessageType = errorMessageType;
  mErrorMessageCached.Truncate();
}

NS_IMETHODIMP
TransportSecurityInfo::GetSecurityState(uint32_t* state)
{
  *state = mSecurityState;
  return NS_OK;
}

nsresult
TransportSecurityInfo::SetSecurityState(uint32_t aState)
{
  mSecurityState = aState;
  return NS_OK;
}

/* attribute unsigned long countSubRequestsBrokenSecurity; */
NS_IMETHODIMP
TransportSecurityInfo::GetCountSubRequestsBrokenSecurity(
  int32_t *aSubRequestsBrokenSecurity)
{
  *aSubRequestsBrokenSecurity = mSubRequestsBrokenSecurity;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::SetCountSubRequestsBrokenSecurity(
  int32_t aSubRequestsBrokenSecurity)
{
  mSubRequestsBrokenSecurity = aSubRequestsBrokenSecurity;
  return NS_OK;
}

/* attribute unsigned long countSubRequestsNoSecurity; */
NS_IMETHODIMP
TransportSecurityInfo::GetCountSubRequestsNoSecurity(
  int32_t *aSubRequestsNoSecurity)
{
  *aSubRequestsNoSecurity = mSubRequestsNoSecurity;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::SetCountSubRequestsNoSecurity(
  int32_t aSubRequestsNoSecurity)
{
  mSubRequestsNoSecurity = aSubRequestsNoSecurity;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::Flush()
{
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetErrorMessage(char16_t** aText)
{
  NS_ENSURE_ARG_POINTER(aText);
  *aText = nullptr;

  if (!NS_IsMainThread()) {
    NS_ERROR("nsNSSSocketInfo::GetErrorMessage called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  MutexAutoLock lock(mMutex);

  if (mErrorMessageCached.IsEmpty()) {
    nsresult rv = formatErrorMessage(lock, 
                                     mErrorCode, mErrorMessageType,
                                     true, true, mErrorMessageCached);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aText = ToNewUnicode(mErrorMessageCached);
  return *aText ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

void
TransportSecurityInfo::GetErrorLogMessage(PRErrorCode errorCode,
                                          SSLErrorMessageType errorMessageType,
                                          nsString &result)
{
  if (!NS_IsMainThread()) {
    NS_ERROR("nsNSSSocketInfo::GetErrorLogMessage called off the main thread");
    return;
  }

  MutexAutoLock lock(mMutex);
  (void) formatErrorMessage(lock, errorCode, errorMessageType,
                            false, false, result);
}

static nsresult
formatPlainErrorMessage(nsXPIDLCString const & host, int32_t port,
                        PRErrorCode err, 
                        bool suppressPort443,
                        nsString &returnedMessage);

static nsresult
formatOverridableCertErrorMessage(nsISSLStatus & sslStatus,
                                  PRErrorCode errorCodeToReport, 
                                  const nsXPIDLCString & host, int32_t port,
                                  bool suppressPort443,
                                  bool wantsHtml,
                                  nsString & returnedMessage);

// XXX: uses nsNSSComponent string bundles off the main thread when called by
//      nsNSSSocketInfo::Write().
nsresult
TransportSecurityInfo::formatErrorMessage(MutexAutoLock const & proofOfLock, 
                                          PRErrorCode errorCode,
                                          SSLErrorMessageType errorMessageType,
                                          bool wantsHtml, bool suppressPort443, 
                                          nsString &result)
{
  if (errorCode == 0) {
    result.Truncate();
    return NS_OK;
  }

  nsresult rv;
  NS_ConvertASCIItoUTF16 hostNameU(mHostName);
  NS_ASSERTION(errorMessageType != OverridableCertErrorMessage || 
                (mSSLStatus && mSSLStatus->mServerCert &&
                 mSSLStatus->mHaveCertErrorBits),
                "GetErrorLogMessage called for cert error without cert");
  if (errorMessageType == OverridableCertErrorMessage && 
      mSSLStatus && mSSLStatus->mServerCert) {
    rv = formatOverridableCertErrorMessage(*mSSLStatus, errorCode,
                                           mHostName, mPort,
                                           suppressPort443,
                                           wantsHtml,
                                           result);
  } else {
    rv = formatPlainErrorMessage(mHostName, mPort, 
                                 errorCode,
                                 suppressPort443,
                                 result);
  }

  if (NS_FAILED(rv)) {
    result.Truncate();
  }

  return rv;
}

/* void getInterface (in nsIIDRef uuid, [iid_is (uuid), retval] out nsQIResult result); */
NS_IMETHODIMP
TransportSecurityInfo::GetInterface(const nsIID & uuid, void * *result)
{
  if (!NS_IsMainThread()) {
    NS_ERROR("nsNSSSocketInfo::GetInterface called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  nsresult rv;
  if (!mCallbacks) {
    nsCOMPtr<nsIInterfaceRequestor> ir = new PipUIContext();
    rv = ir->GetInterface(uuid, result);
  } else {
    rv = mCallbacks->GetInterface(uuid, result);
  }
  return rv;
}

// This is a new magic value. However, it re-uses the first 4 bytes
// of the previous value. This is so when older versions attempt to
// read a newer serialized TransportSecurityInfo, they will actually
// fail and return NS_ERROR_FAILURE instead of silently failing.
#define TRANSPORTSECURITYINFOMAGIC { 0xa9863a23, 0x28ea, 0x45d2, \
  { 0xa2, 0x5a, 0x35, 0x7c, 0xae, 0xfa, 0x7f, 0x82 } }
static NS_DEFINE_CID(kTransportSecurityInfoMagic, TRANSPORTSECURITYINFOMAGIC);

NS_IMETHODIMP
TransportSecurityInfo::Write(nsIObjectOutputStream* stream)
{
  nsresult rv = stream->WriteID(kTransportSecurityInfoMagic);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MutexAutoLock lock(mMutex);

  rv = stream->Write32(mSecurityState);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = stream->Write32(mSubRequestsBrokenSecurity);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = stream->Write32(mSubRequestsNoSecurity);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // XXX: uses nsNSSComponent string bundles off the main thread
  rv = formatErrorMessage(lock, mErrorCode, mErrorMessageType, true, true,
                          mErrorMessageCached);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = stream->WriteWStringZ(mErrorMessageCached.get());
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsISerializable> serializable(mSSLStatus);
  rv = stream->WriteCompoundObject(serializable, NS_GET_IID(nsISSLStatus),
                                   true);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::Read(nsIObjectInputStream* stream)
{
  nsID id;
  nsresult rv = stream->ReadID(&id);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!id.Equals(kTransportSecurityInfoMagic)) {
    return NS_ERROR_UNEXPECTED;
  }

  MutexAutoLock lock(mMutex);

  rv = stream->Read32(&mSecurityState);
  if (NS_FAILED(rv)) {
    return rv;
  }
  uint32_t subRequestsBrokenSecurity;
  rv = stream->Read32(&subRequestsBrokenSecurity);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (subRequestsBrokenSecurity >
      static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
    return NS_ERROR_UNEXPECTED;
  }
  mSubRequestsBrokenSecurity = subRequestsBrokenSecurity;
  uint32_t subRequestsNoSecurity;
  rv = stream->Read32(&subRequestsNoSecurity);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (subRequestsNoSecurity >
      static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
    return NS_ERROR_UNEXPECTED;
  }
  mSubRequestsNoSecurity = subRequestsNoSecurity;
  rv = stream->ReadString(mErrorMessageCached);
  if (NS_FAILED(rv)) {
    return rv;
  }
  mErrorCode = 0;
  nsCOMPtr<nsISupports> supports;
  rv = stream->ReadObject(true, getter_AddRefs(supports));
  if (NS_FAILED(rv)) {
    return rv;
  }
  mSSLStatus = reinterpret_cast<nsSSLStatus*>(supports.get());
  if (!mSSLStatus) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetInterfaces(uint32_t *count, nsIID * **array)
{
  *count = 0;
  *array = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetHelperForLanguage(uint32_t language,
                                            nsISupports **_retval)
{
  *_retval = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetContractID(char * *aContractID)
{
  *aContractID = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetClassDescription(char * *aClassDescription)
{
  *aClassDescription = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetClassID(nsCID * *aClassID)
{
  *aClassID = (nsCID*) nsMemory::Alloc(sizeof(nsCID));
  if (!*aClassID)
    return NS_ERROR_OUT_OF_MEMORY;
  return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP
TransportSecurityInfo::GetImplementationLanguage(
  uint32_t *aImplementationLanguage)
{
  *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetFlags(uint32_t *aFlags)
{
  *aFlags = 0;
  return NS_OK;
}

static NS_DEFINE_CID(kNSSSocketInfoCID, TRANSPORTSECURITYINFO_CID);

NS_IMETHODIMP
TransportSecurityInfo::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
  *aClassIDNoAlloc = kNSSSocketInfoCID;
  return NS_OK;
}

nsresult
TransportSecurityInfo::GetSSLStatus(nsISSLStatus** _result)
{
  NS_ENSURE_ARG_POINTER(_result);

  *_result = mSSLStatus;
  NS_IF_ADDREF(*_result);

  return NS_OK;
}

nsresult
TransportSecurityInfo::SetSSLStatus(nsSSLStatus *aSSLStatus)
{
  mSSLStatus = aSSLStatus;

  return NS_OK;
}

/* Formats an error message for non-certificate-related SSL errors
 * and non-overridable certificate errors (both are of type
 * PlainErrormMessage). Use formatOverridableCertErrorMessage
 * for overridable cert errors.
 */
static nsresult
formatPlainErrorMessage(const nsXPIDLCString &host, int32_t port,
                        PRErrorCode err, 
                        bool suppressPort443,
                        nsString &returnedMessage)
{
  static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

  const char16_t *params[1];
  nsresult rv;

  nsCOMPtr<nsINSSComponent> component = do_GetService(kNSSComponentCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (host.Length())
  {
    nsString hostWithPort;

    // For now, hide port when it's 443 and we're reporting the error.
    // In the future a better mechanism should be used
    // to make a decision about showing the port number, possibly by requiring
    // the context object to implement a specific interface.
    // The motivation is that Mozilla browser would like to hide the port number
    // in error pages in the common case.

    hostWithPort.AssignASCII(host);
    if (!suppressPort443 || port != 443) {
      hostWithPort.Append(':');
      hostWithPort.AppendInt(port);
    }
    params[0] = hostWithPort.get();

    nsString formattedString;
    rv = component->PIPBundleFormatStringFromName("SSLConnectionErrorPrefix", 
                                                  params, 1, 
                                                  formattedString);
    if (NS_SUCCEEDED(rv))
    {
      returnedMessage.Append(formattedString);
      returnedMessage.AppendLiteral("\n\n");
    }
  }

  nsString explanation;
  rv = nsNSSErrors::getErrorMessageFromCode(err, component, explanation);
  if (NS_SUCCEEDED(rv))
    returnedMessage.Append(explanation);

  return NS_OK;
}

static void
AppendErrorTextUntrusted(PRErrorCode errTrust,
                         const nsString &host,
                         nsIX509Cert* ix509,
                         nsINSSComponent *component,
                         nsString &returnedMessage)
{
  const char *errorID = nullptr;
  bool isSelfSigned;
  if (NS_SUCCEEDED(ix509->GetIsSelfSigned(&isSelfSigned)) && isSelfSigned) {
    errorID = "certErrorTrust_SelfSigned";
  }

  if (!errorID) {
    switch (errTrust) {
      case SEC_ERROR_UNKNOWN_ISSUER:
      {
        nsCOMPtr<nsIArray> chain;
        ix509->GetChain(getter_AddRefs(chain));
        uint32_t length = 0;
        if (chain && NS_FAILED(chain->GetLength(&length)))
          length = 0;
        if (length == 1)
          errorID = "certErrorTrust_MissingChain";
        else
          errorID = "certErrorTrust_UnknownIssuer";
        break;
      }
      case SEC_ERROR_CA_CERT_INVALID:
        errorID = "certErrorTrust_CaInvalid";
        break;
      case SEC_ERROR_UNTRUSTED_ISSUER:
        errorID = "certErrorTrust_Issuer";
        break;
      case SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED:
        errorID = "certErrorTrust_SignatureAlgorithmDisabled";
        break;
      case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
        errorID = "certErrorTrust_ExpiredIssuer";
        break;
      case SEC_ERROR_UNTRUSTED_CERT:
      default:
        errorID = "certErrorTrust_Untrusted";
        break;
    }
  }

  nsString formattedString;
  nsresult rv = component->GetPIPNSSBundleString(errorID, 
                                                 formattedString);
  if (NS_SUCCEEDED(rv))
  {
    returnedMessage.Append(formattedString);
    returnedMessage.Append('\n');
  }
}

// returns TRUE if SAN was used to produce names
// return FALSE if nothing was produced
// names => a single name or a list of names
// multipleNames => whether multiple names were delivered
static bool
GetSubjectAltNames(CERTCertificate *nssCert,
                   nsINSSComponent *component,
                   nsString &allNames,
                   uint32_t &nameCount)
{
  allNames.Truncate();
  nameCount = 0;

  SECItem altNameExtension = {siBuffer, nullptr, 0 };
  CERTGeneralName *sanNameList = nullptr;

  SECStatus rv = CERT_FindCertExtension(nssCert, SEC_OID_X509_SUBJECT_ALT_NAME,
                                        &altNameExtension);
  if (rv != SECSuccess) {
    return false;
  }

  ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return false;
  }

  sanNameList = CERT_DecodeAltNameExtension(arena.get(), &altNameExtension);
  if (!sanNameList) {
    return false;
  }

  SECITEM_FreeItem(&altNameExtension, false);

  CERTGeneralName *current = sanNameList;
  do {
    nsAutoString name;
    switch (current->type) {
      case certDNSName:
        {
          nsDependentCSubstring nameFromCert(reinterpret_cast<char*>
                                              (current->name.other.data),
                                              current->name.other.len);
          // dNSName fields are defined as type IA5String and thus should
          // be limited to ASCII characters.
          if (IsASCII(nameFromCert)) {
            name.Assign(NS_ConvertASCIItoUTF16(nameFromCert));
            if (!allNames.IsEmpty()) {
              allNames.AppendLiteral(", ");
            }
            ++nameCount;
            allNames.Append(name);
          }
        }
        break;

      case certIPAddress:
        {
          char buf[INET6_ADDRSTRLEN];
          PRNetAddr addr;
          if (current->name.other.len == 4) {
            addr.inet.family = PR_AF_INET;
            memcpy(&addr.inet.ip, current->name.other.data, current->name.other.len);
            PR_NetAddrToString(&addr, buf, sizeof(buf));
            name.AssignASCII(buf);
          } else if (current->name.other.len == 16) {
            addr.ipv6.family = PR_AF_INET6;
            memcpy(&addr.ipv6.ip, current->name.other.data, current->name.other.len);
            PR_NetAddrToString(&addr, buf, sizeof(buf));
            name.AssignASCII(buf);
          } else {
            /* invalid IP address */
          }
          if (!name.IsEmpty()) {
            if (!allNames.IsEmpty()) {
              allNames.AppendLiteral(", ");
            }
            ++nameCount;
            allNames.Append(name);
          }
          break;
        }

      default: // all other types of names are ignored
        break;
    }
    current = CERT_GetNextGeneralName(current);
  } while (current != sanNameList); // double linked

  return true;
}

static void
AppendErrorTextMismatch(const nsString &host,
                        nsIX509Cert* ix509,
                        nsINSSComponent *component,
                        bool wantsHtml,
                        nsString &returnedMessage)
{
  const char16_t *params[1];
  nsresult rv;

  ScopedCERTCertificate nssCert(ix509->GetCert());

  if (!nssCert) {
    // We are unable to extract the valid names, say "not valid for name".
    params[0] = host.get();
    nsString formattedString;
    rv = component->PIPBundleFormatStringFromName("certErrorMismatch", 
                                                  params, 1, 
                                                  formattedString);
    if (NS_SUCCEEDED(rv)) {
      returnedMessage.Append(formattedString);
      returnedMessage.Append('\n');
    }
    return;
  }

  nsString allNames;
  uint32_t nameCount = 0;
  bool useSAN = false;

  if (nssCert)
    useSAN = GetSubjectAltNames(nssCert.get(), component, allNames, nameCount);

  if (!useSAN) {
    char *certName = CERT_GetCommonName(&nssCert->subject);
    if (certName) {
      nsDependentCSubstring commonName(certName, strlen(certName));
      if (IsUTF8(commonName)) {
        // Bug 1024781
        // We should actually check that the common name is a valid dns name or
        // ip address and not any string value before adding it to the display
        // list.
        ++nameCount;
        allNames.Assign(NS_ConvertUTF8toUTF16(commonName));
      }
      PORT_Free(certName);
    }
  }

  if (nameCount > 1) {
    nsString message;
    rv = component->GetPIPNSSBundleString("certErrorMismatchMultiple", 
                                          message);
    if (NS_SUCCEEDED(rv)) {
      returnedMessage.Append(message);
      returnedMessage.AppendLiteral("\n  ");
      returnedMessage.Append(allNames);
      returnedMessage.AppendLiteral("  \n");
    }
  }
  else if (nameCount == 1) {
    const char16_t *params[1];
    params[0] = allNames.get();
    
    const char *stringID;
    if (wantsHtml)
      stringID = "certErrorMismatchSingle2";
    else
      stringID = "certErrorMismatchSinglePlain";

    nsString formattedString;
    rv = component->PIPBundleFormatStringFromName(stringID, 
                                                  params, 1, 
                                                  formattedString);
    if (NS_SUCCEEDED(rv)) {
      returnedMessage.Append(formattedString);
      returnedMessage.Append('\n');
    }
  }
  else { // nameCount == 0
    nsString message;
    nsresult rv = component->GetPIPNSSBundleString("certErrorMismatchNoNames",
                                                   message);
    if (NS_SUCCEEDED(rv)) {
      returnedMessage.Append(message);
      returnedMessage.Append('\n');
    }
  }
}

static void
GetDateBoundary(nsIX509Cert* ix509,
                nsString &formattedDate,
                nsString &nowDate,
                bool &trueExpired_falseNotYetValid)
{
  trueExpired_falseNotYetValid = true;
  formattedDate.Truncate();

  PRTime notAfter, notBefore, timeToUse;
  nsCOMPtr<nsIX509CertValidity> validity;
  nsresult rv;

  rv = ix509->GetValidity(getter_AddRefs(validity));
  if (NS_FAILED(rv))
    return;

  rv = validity->GetNotAfter(&notAfter);
  if (NS_FAILED(rv))
    return;

  rv = validity->GetNotBefore(&notBefore);
  if (NS_FAILED(rv))
    return;

  PRTime now = PR_Now();
  if (now > notAfter) {
    timeToUse = notAfter;
  } else {
    timeToUse = notBefore;
    trueExpired_falseNotYetValid = false;
  }

  nsCOMPtr<nsIDateTimeFormat> dateTimeFormat(do_CreateInstance(NS_DATETIMEFORMAT_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return;

  dateTimeFormat->FormatPRTime(nullptr, kDateFormatShort, 
                               kTimeFormatNoSeconds, timeToUse, 
                               formattedDate);
  dateTimeFormat->FormatPRTime(nullptr, kDateFormatShort,
                               kTimeFormatNoSeconds, now,
                               nowDate);
}

static void
AppendErrorTextTime(nsIX509Cert* ix509,
                    nsINSSComponent *component,
                    nsString &returnedMessage)
{
  nsAutoString formattedDate, nowDate;
  bool trueExpired_falseNotYetValid;
  GetDateBoundary(ix509, formattedDate, nowDate, trueExpired_falseNotYetValid);

  const char16_t *params[2];
  params[0] = formattedDate.get(); // might be empty, if helper function had a problem 
  params[1] = nowDate.get();

  const char *key = trueExpired_falseNotYetValid ? 
                    "certErrorExpiredNow" : "certErrorNotYetValidNow";
  nsresult rv;
  nsString formattedString;
  rv = component->PIPBundleFormatStringFromName(
           key,
           params, 
           ArrayLength(params),
           formattedString);
  if (NS_SUCCEEDED(rv))
  {
    returnedMessage.Append(formattedString);
    returnedMessage.Append('\n');
  }
}

static void
AppendErrorTextCode(PRErrorCode errorCodeToReport,
                    nsINSSComponent *component,
                    nsString &returnedMessage)
{
  const char *codeName = nsNSSErrors::getDefaultErrorStringName(errorCodeToReport);
  if (codeName)
  {
    nsCString error_id(codeName);
    ToLowerCase(error_id);
    NS_ConvertASCIItoUTF16 idU(error_id);

    const char16_t *params[1];
    params[0] = idU.get();

    nsString formattedString;
    nsresult rv;
    rv = component->PIPBundleFormatStringFromName("certErrorCodePrefix", 
                                                  params, 1, 
                                                  formattedString);
    if (NS_SUCCEEDED(rv)) {
      returnedMessage.Append('\n');
      returnedMessage.Append(formattedString);
      returnedMessage.Append('\n');
    }
    else {
      returnedMessage.AppendLiteral(" (");
      returnedMessage.Append(idU);
      returnedMessage.Append(')');
    }
  }
}

/* Formats an error message for overridable certificate errors (of type
 * OverridableCertErrorMessage). Use formatPlainErrorMessage to format
 * non-overridable cert errors and non-cert-related errors.
 */
static nsresult
formatOverridableCertErrorMessage(nsISSLStatus & sslStatus,
                                  PRErrorCode errorCodeToReport, 
                                  const nsXPIDLCString & host, int32_t port,
                                  bool suppressPort443,
                                  bool wantsHtml,
                                  nsString & returnedMessage)
{
  static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

  const char16_t *params[1];
  nsresult rv;
  nsAutoString hostWithPort;
  nsAutoString hostWithoutPort;

  // For now, hide port when it's 443 and we're reporting the error.
  // In the future a better mechanism should be used
  // to make a decision about showing the port number, possibly by requiring
  // the context object to implement a specific interface.
  // The motivation is that Mozilla browser would like to hide the port number
  // in error pages in the common case.
  
  hostWithoutPort.AppendASCII(host);
  if (suppressPort443 && port == 443) {
    params[0] = hostWithoutPort.get();
  } else {
    hostWithPort.AppendASCII(host);
    hostWithPort.Append(':');
    hostWithPort.AppendInt(port);
    params[0] = hostWithPort.get();
  }

  nsCOMPtr<nsINSSComponent> component = do_GetService(kNSSComponentCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  returnedMessage.Truncate();
  rv = component->PIPBundleFormatStringFromName("certErrorIntro", params, 1,
                                                returnedMessage);
  NS_ENSURE_SUCCESS(rv, rv);

  returnedMessage.AppendLiteral("\n\n");

  RefPtr<nsIX509Cert> ix509;
  rv = sslStatus.GetServerCert(byRef(ix509));
  NS_ENSURE_SUCCESS(rv, rv);

  bool isUntrusted;
  rv = sslStatus.GetIsUntrusted(&isUntrusted);
  NS_ENSURE_SUCCESS(rv, rv);
  if (isUntrusted) {
    AppendErrorTextUntrusted(errorCodeToReport, hostWithoutPort, ix509, 
                             component, returnedMessage);
  }

  bool isDomainMismatch;
  rv = sslStatus.GetIsDomainMismatch(&isDomainMismatch);
  NS_ENSURE_SUCCESS(rv, rv);
  if (isDomainMismatch) {
    AppendErrorTextMismatch(hostWithoutPort, ix509, component, wantsHtml, returnedMessage);
  }

  bool isNotValidAtThisTime;
  rv = sslStatus.GetIsNotValidAtThisTime(&isNotValidAtThisTime);
  NS_ENSURE_SUCCESS(rv, rv);
  if (isNotValidAtThisTime) {
    AppendErrorTextTime(ix509, component, returnedMessage);
  }

  AppendErrorTextCode(errorCodeToReport, component, returnedMessage);

  return NS_OK;
}

// RememberCertErrorsTable

/*static*/ RememberCertErrorsTable*
RememberCertErrorsTable::sInstance = nullptr;

RememberCertErrorsTable::RememberCertErrorsTable()
  : mErrorHosts()
  , mMutex("RememberCertErrorsTable::mMutex")
{
}

static nsresult
GetHostPortKey(TransportSecurityInfo* infoObject, nsAutoCString &result)
{
  nsresult rv;

  result.Truncate();

  nsXPIDLCString hostName;
  rv = infoObject->GetHostName(getter_Copies(hostName));
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t port;
  rv = infoObject->GetPort(&port);
  NS_ENSURE_SUCCESS(rv, rv);

  result.Assign(hostName);
  result.Append(':');
  result.AppendInt(port);

  return NS_OK;
}

void
RememberCertErrorsTable::RememberCertHasError(TransportSecurityInfo* infoObject,
                                              nsSSLStatus* status,
                                              SECStatus certVerificationResult)
{
  nsresult rv;

  nsAutoCString hostPortKey;
  rv = GetHostPortKey(infoObject, hostPortKey);
  if (NS_FAILED(rv))
    return;

  if (certVerificationResult != SECSuccess) {
    NS_ASSERTION(status,
        "Must have nsSSLStatus object when remembering flags");

    if (!status)
      return;

    CertStateBits bits;
    bits.mIsDomainMismatch = status->mIsDomainMismatch;
    bits.mIsNotValidAtThisTime = status->mIsNotValidAtThisTime;
    bits.mIsUntrusted = status->mIsUntrusted;

    MutexAutoLock lock(mMutex);
    mErrorHosts.Put(hostPortKey, bits);
  }
  else {
    MutexAutoLock lock(mMutex);
    mErrorHosts.Remove(hostPortKey);
  }
}

void
RememberCertErrorsTable::LookupCertErrorBits(TransportSecurityInfo* infoObject,
                                             nsSSLStatus* status)
{
  // Get remembered error bits from our cache, because of SSL session caching
  // the NSS library potentially hasn't notified us for this socket.
  if (status->mHaveCertErrorBits)
    // Rather do not modify bits if already set earlier
    return;

  nsresult rv;

  nsAutoCString hostPortKey;
  rv = GetHostPortKey(infoObject, hostPortKey);
  if (NS_FAILED(rv))
    return;

  CertStateBits bits;
  {
    MutexAutoLock lock(mMutex);
    if (!mErrorHosts.Get(hostPortKey, &bits))
      // No record was found, this host had no cert errors
      return;
  }

  // This host had cert errors, update the bits correctly
  status->mHaveCertErrorBits = true;
  status->mIsDomainMismatch = bits.mIsDomainMismatch;
  status->mIsNotValidAtThisTime = bits.mIsNotValidAtThisTime;
  status->mIsUntrusted = bits.mIsUntrusted;
}

void
TransportSecurityInfo::SetStatusErrorBits(nsIX509Cert & cert,
                                          uint32_t collected_errors)
{
  MutexAutoLock lock(mMutex);

  if (!mSSLStatus)
    mSSLStatus = new nsSSLStatus();

  mSSLStatus->mServerCert = &cert;

  mSSLStatus->mHaveCertErrorBits = true;
  mSSLStatus->mIsDomainMismatch = 
    collected_errors & nsICertOverrideService::ERROR_MISMATCH;
  mSSLStatus->mIsNotValidAtThisTime = 
    collected_errors & nsICertOverrideService::ERROR_TIME;
  mSSLStatus->mIsUntrusted = 
    collected_errors & nsICertOverrideService::ERROR_UNTRUSTED;

  RememberCertErrorsTable::GetInstance().RememberCertHasError(this,
                                                              mSSLStatus,
                                                              SECFailure);
}

} } // namespace mozilla::psm
