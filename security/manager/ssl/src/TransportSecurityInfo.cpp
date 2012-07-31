/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TransportSecurityInfo.h"
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
#include "nsNSSCleaner.h"
#include "nsIProgrammingLanguage.h"
#include "nsIArray.h"
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

namespace {

NSSCleanupAutoPtrClass(CERTCertificate, CERT_DestroyCertificate)

static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

} // unnamed namespace

namespace mozilla { namespace psm {

TransportSecurityInfo::TransportSecurityInfo()
  : mMutex("TransportSecurityInfo::mMutex"),
    mSecurityState(nsIWebProgressListener::STATE_IS_INSECURE),
    mSubRequestsHighSecurity(0),
    mSubRequestsLowSecurity(0),
    mSubRequestsBrokenSecurity(0),
    mSubRequestsNoSecurity(0),
    mErrorCode(0),
    mErrorMessageType(PlainErrorMessage),
    mPort(0),
    mIsCertIssuerBlacklisted(false)
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

NS_IMPL_THREADSAFE_ISUPPORTS6(TransportSecurityInfo,
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
TransportSecurityInfo::SetPort(PRInt32 aPort)
{
  mPort = aPort;
  return NS_OK;
}

nsresult
TransportSecurityInfo::GetPort(PRInt32 *aPort)
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
TransportSecurityInfo::GetSecurityState(PRUint32* state)
{
  *state = mSecurityState;
  return NS_OK;
}

nsresult
TransportSecurityInfo::SetSecurityState(PRUint32 aState)
{
  mSecurityState = aState;
  return NS_OK;
}

/* attribute unsigned long countSubRequestsHighSecurity; */
NS_IMETHODIMP
TransportSecurityInfo::GetCountSubRequestsHighSecurity(
  PRInt32 *aSubRequestsHighSecurity)
{
  *aSubRequestsHighSecurity = mSubRequestsHighSecurity;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::SetCountSubRequestsHighSecurity(
  PRInt32 aSubRequestsHighSecurity)
{
  mSubRequestsHighSecurity = aSubRequestsHighSecurity;
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute unsigned long countSubRequestsLowSecurity; */
NS_IMETHODIMP
TransportSecurityInfo::GetCountSubRequestsLowSecurity(
  PRInt32 *aSubRequestsLowSecurity)
{
  *aSubRequestsLowSecurity = mSubRequestsLowSecurity;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::SetCountSubRequestsLowSecurity(
  PRInt32 aSubRequestsLowSecurity)
{
  mSubRequestsLowSecurity = aSubRequestsLowSecurity;
  return NS_OK;
}

/* attribute unsigned long countSubRequestsBrokenSecurity; */
NS_IMETHODIMP
TransportSecurityInfo::GetCountSubRequestsBrokenSecurity(
  PRInt32 *aSubRequestsBrokenSecurity)
{
  *aSubRequestsBrokenSecurity = mSubRequestsBrokenSecurity;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::SetCountSubRequestsBrokenSecurity(
  PRInt32 aSubRequestsBrokenSecurity)
{
  mSubRequestsBrokenSecurity = aSubRequestsBrokenSecurity;
  return NS_OK;
}

/* attribute unsigned long countSubRequestsNoSecurity; */
NS_IMETHODIMP
TransportSecurityInfo::GetCountSubRequestsNoSecurity(
  PRInt32 *aSubRequestsNoSecurity)
{
  *aSubRequestsNoSecurity = mSubRequestsNoSecurity;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::SetCountSubRequestsNoSecurity(
  PRInt32 aSubRequestsNoSecurity)
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
TransportSecurityInfo::GetShortSecurityDescription(PRUnichar** aText)
{
  if (mShortDesc.IsEmpty())
    *aText = nullptr;
  else {
    *aText = ToNewUnicode(mShortDesc);
    NS_ENSURE_TRUE(*aText, NS_ERROR_OUT_OF_MEMORY);
  }
  return NS_OK;
}

nsresult
TransportSecurityInfo::SetShortSecurityDescription(const PRUnichar* aText)
{
  mShortDesc.Assign(aText);
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetErrorMessage(PRUnichar** aText)
{
  NS_ENSURE_ARG_POINTER(aText);
  *aText = nullptr;

  if (!NS_IsMainThread()) {
    NS_ERROR("nsNSSSocketInfo::GetErrorMessage called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  MutexAutoLock lock(mMutex);

  nsresult rv = formatErrorMessage(lock);
  NS_ENSURE_SUCCESS(rv, rv);

  *aText = ToNewUnicode(mErrorMessageCached);
  return *aText != nullptr ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

static nsresult
formatPlainErrorMessage(nsXPIDLCString const & host, PRInt32 port,
                        PRErrorCode err, nsString &returnedMessage);

static nsresult
formatOverridableCertErrorMessage(nsISSLStatus & sslStatus,
                                  PRErrorCode errorCodeToReport, 
                                  const nsXPIDLCString & host, PRInt32 port,
                                  nsString & returnedMessage);

// XXX: uses nsNSSComponent string bundles off the main thread when called by
//      nsNSSSocketInfo::Write(). When we remove the error message from the
//      serialization of nsNSSSocketInfo (bug 697781) we can inline
//      formatErrorMessage into GetErrorMessage().
nsresult
TransportSecurityInfo::formatErrorMessage(MutexAutoLock const & proofOfLock)
{
  if (mErrorCode == 0 || !mErrorMessageCached.IsEmpty()) {
    return NS_OK;
  }

  nsresult rv;
  NS_ConvertASCIItoUTF16 hostNameU(mHostName);
  NS_ASSERTION(mErrorMessageType != OverridableCertErrorMessage || 
                (mSSLStatus && mSSLStatus->mServerCert &&
                 mSSLStatus->mHaveCertErrorBits),
                "GetErrorMessage called for cert error without cert");
  if (mErrorMessageType == OverridableCertErrorMessage && 
      mSSLStatus && mSSLStatus->mServerCert) {
    rv = formatOverridableCertErrorMessage(*mSSLStatus, mErrorCode,
                                           mHostName, mPort,
                                           mErrorMessageCached);
  } else {
    rv = formatPlainErrorMessage(mHostName, mPort, mErrorCode,
                                 mErrorMessageCached);
  }

  if (NS_FAILED(rv)) {
    mErrorMessageCached.Truncate();
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
    if (!ir)
      return NS_ERROR_OUT_OF_MEMORY;

    rv = ir->GetInterface(uuid, result);
  } else {
    rv = mCallbacks->GetInterface(uuid, result);
  }
  return rv;
}

static NS_DEFINE_CID(kNSSCertificateCID, NS_X509CERT_CID);
#define TRANSPORTSECURITYINFOMAGIC { 0xa9863a23, 0x26b8, 0x4a9c, \
  { 0x83, 0xf1, 0xe9, 0xda, 0xdb, 0x36, 0xb8, 0x30 } }
static NS_DEFINE_CID(kTransportSecurityInfoMagic, TRANSPORTSECURITYINFOMAGIC);

NS_IMETHODIMP
TransportSecurityInfo::Write(nsIObjectOutputStream* stream)
{
  stream->WriteID(kTransportSecurityInfoMagic);

  MutexAutoLock lock(mMutex);

  nsRefPtr<nsSSLStatus> status = mSSLStatus;
  nsCOMPtr<nsISerializable> certSerializable;

  // Write a redundant copy of the certificate for backward compatibility
  // with previous versions, which also unnecessarily wrote it.
  //
  // As we are reading the object our self, not using ReadObject, we have
  // to store it here 'manually' as well, mimicking our object stream
  // implementation.

  if (status) {
    nsCOMPtr<nsIX509Cert> cert = status->mServerCert;
    certSerializable = do_QueryInterface(cert);

    if (!certSerializable) {
      NS_ERROR("certificate is missing or isn't serializable");
      return NS_ERROR_UNEXPECTED;
    }
  } else {
    NS_WARNING("Serializing nsNSSSocketInfo without mSSLStatus");
  }

  // Store the flag if there is the certificate present
  stream->WriteBoolean(certSerializable);
  if (certSerializable) {
    stream->WriteID(kNSSCertificateCID);
    stream->WriteID(NS_GET_IID(nsISupports));
    certSerializable->Write(stream);
  }

  // Store the version number of the binary stream data format.
  // The 0xFFFF0000 mask is included to the version number
  // to distinguish version number from mSecurityState
  // field stored in times before versioning has been introduced.
  // This mask value has been chosen as mSecurityState could
  // never be assigned such value.
  PRUint32 version = 3;
  stream->Write32(version | 0xFFFF0000);
  stream->Write32(mSecurityState);
  stream->WriteWStringZ(mShortDesc.get());

  // XXX: uses nsNSSComponent string bundles off the main thread
  nsresult rv = formatErrorMessage(lock); 
  NS_ENSURE_SUCCESS(rv, rv);
  stream->WriteWStringZ(mErrorMessageCached.get());

  stream->WriteCompoundObject(NS_ISUPPORTS_CAST(nsISSLStatus*, status),
                              NS_GET_IID(nsISupports), true);

  stream->Write32((PRUint32)mSubRequestsHighSecurity);
  stream->Write32((PRUint32)mSubRequestsLowSecurity);
  stream->Write32((PRUint32)mSubRequestsBrokenSecurity);
  stream->Write32((PRUint32)mSubRequestsNoSecurity);
  return NS_OK;
}

static bool CheckUUIDEquals(PRUint32 m0,
                            nsIObjectInputStream* stream,
                            const nsCID& id)
{
  nsID tempID;
  tempID.m0 = m0;
  stream->Read16(&tempID.m1);
  stream->Read16(&tempID.m2);
  for (int i = 0; i < 8; ++i)
    stream->Read8(&tempID.m3[i]);
  return tempID.Equals(id);
}

NS_IMETHODIMP
TransportSecurityInfo::Read(nsIObjectInputStream* stream)
{
  nsresult rv;

  PRUint32 version;
  bool certificatePresent;

  // Check what we have here...
  PRUint32 UUID_0;
  stream->Read32(&UUID_0);
  if (UUID_0 == kTransportSecurityInfoMagic.m0) {
    // It seems this stream begins with our magic ID, check it really is there
    if (!CheckUUIDEquals(UUID_0, stream, kTransportSecurityInfoMagic))
      return NS_ERROR_FAILURE;

    // OK, this seems to be our stream, now continue to check there is
    // the certificate
    stream->ReadBoolean(&certificatePresent);
    stream->Read32(&UUID_0);
  }
  else {
    // There is no magic, assume there is a certificate present as in versions
    // prior to those with the magic didn't store that flag; we check the 
    // certificate is present by cheking the CID then
    certificatePresent = true;
  }

  if (certificatePresent && UUID_0 == kNSSCertificateCID.m0) {
    // It seems there is the certificate CID present, check it now; we only
    // have this single certificate implementation at this time.
    if (!CheckUUIDEquals(UUID_0, stream, kNSSCertificateCID))
      return NS_ERROR_FAILURE;

    // OK, we have read the CID of the certificate, check the interface ID
    nsID tempID;
    stream->ReadID(&tempID);
    if (!tempID.Equals(NS_GET_IID(nsISupports)))
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsISerializable> serializable =
        do_CreateInstance(kNSSCertificateCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // This is the redundant copy of the certificate; just ignore it
    serializable->Read(stream);

    // We are done with reading the certificate, now read the version
    // as we did before.
    stream->Read32(&version);
  }
  else {
    // There seems not to be the certificate present in the stream.
    version = UUID_0;
  }

  MutexAutoLock lock(mMutex);

  // If the version field we have just read is not masked with 0xFFFF0000
  // then it is stored mSecurityState field and this is version 1 of
  // the binary data stream format.
  if ((version & 0xFFFF0000) == 0xFFFF0000) {
    version &= ~0xFFFF0000;
    stream->Read32(&mSecurityState);
  }
  else {
    mSecurityState = version;
    version = 1;
  }
  stream->ReadString(mShortDesc);
  stream->ReadString(mErrorMessageCached);
  mErrorCode = 0;

  nsCOMPtr<nsISupports> obj;
  stream->ReadObject(true, getter_AddRefs(obj));
  
  mSSLStatus = reinterpret_cast<nsSSLStatus*>(obj.get());

  if (!mSSLStatus) {
    NS_WARNING("deserializing nsNSSSocketInfo without mSSLStatus");
  }

  if (version >= 2) {
    stream->Read32((PRUint32*)&mSubRequestsHighSecurity);
    stream->Read32((PRUint32*)&mSubRequestsLowSecurity);
    stream->Read32((PRUint32*)&mSubRequestsBrokenSecurity);
    stream->Read32((PRUint32*)&mSubRequestsNoSecurity);
  }
  else {
    mSubRequestsHighSecurity = 0;
    mSubRequestsLowSecurity = 0;
    mSubRequestsBrokenSecurity = 0;
    mSubRequestsNoSecurity = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetInterfaces(PRUint32 *count, nsIID * **array)
{
  *count = 0;
  *array = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetHelperForLanguage(PRUint32 language,
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
  PRUint32 *aImplementationLanguage)
{
  *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetFlags(PRUint32 *aFlags)
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
formatPlainErrorMessage(const nsXPIDLCString &host, PRInt32 port,
                        PRErrorCode err, nsString &returnedMessage)
{
  const PRUnichar *params[1];
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
    if (port != 443) {
      hostWithPort.AppendLiteral(":");
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
      returnedMessage.Append(NS_LITERAL_STRING("\n\n"));
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
  nsCOMPtr<nsIX509Cert3> cert3 = do_QueryInterface(ix509);
  if (cert3) {
    bool isSelfSigned;
    if (NS_SUCCEEDED(cert3->GetIsSelfSigned(&isSelfSigned))
        && isSelfSigned) {
      errorID = "certErrorTrust_SelfSigned";
    }
  }

  if (!errorID) {
    switch (errTrust) {
      case SEC_ERROR_UNKNOWN_ISSUER:
      {
        nsCOMPtr<nsIArray> chain;
        ix509->GetChain(getter_AddRefs(chain));
        PRUint32 length = 0;
        if (chain && NS_FAILED(chain->GetLength(&length)))
          length = 0;
        if (length == 1)
          errorID = "certErrorTrust_MissingChain";
        else
          errorID = "certErrorTrust_UnknownIssuer";
        break;
      }
      case SEC_ERROR_INADEQUATE_KEY_USAGE:
        // Should get an individual string in the future
        // For now, use the same as CaInvalid
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
    returnedMessage.Append(NS_LITERAL_STRING("\n"));
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
                   PRUint32 &nameCount)
{
  allNames.Truncate();
  nameCount = 0;

  PRArenaPool *san_arena = nullptr;
  SECItem altNameExtension = {siBuffer, NULL, 0 };
  CERTGeneralName *sanNameList = nullptr;

  SECStatus rv = CERT_FindCertExtension(nssCert, SEC_OID_X509_SUBJECT_ALT_NAME,
                                        &altNameExtension);
  if (rv != SECSuccess)
    return false;

  san_arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!san_arena)
    return false;

  sanNameList = CERT_DecodeAltNameExtension(san_arena, &altNameExtension);
  if (!sanNameList)
    return false;

  SECITEM_FreeItem(&altNameExtension, false);

  CERTGeneralName *current = sanNameList;
  do {
    nsAutoString name;
    switch (current->type) {
      case certDNSName:
        name.AssignASCII((char*)current->name.other.data, current->name.other.len);
        if (!allNames.IsEmpty()) {
          allNames.Append(NS_LITERAL_STRING(" , "));
        }
        ++nameCount;
        allNames.Append(name);
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
              allNames.Append(NS_LITERAL_STRING(" , "));
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

  PORT_FreeArena(san_arena, false);
  return true;
}

static void
AppendErrorTextMismatch(const nsString &host,
                        nsIX509Cert* ix509,
                        nsINSSComponent *component,
                        nsString &returnedMessage)
{
  const PRUnichar *params[1];
  nsresult rv;

  CERTCertificate *nssCert = NULL;
  CERTCertificateCleaner nssCertCleaner(nssCert);

  nsCOMPtr<nsIX509Cert2> cert2 = do_QueryInterface(ix509, &rv);
  if (cert2)
    nssCert = cert2->GetCert();

  if (!nssCert) {
    // We are unable to extract the valid names, say "not valid for name".
    params[0] = host.get();
    nsString formattedString;
    rv = component->PIPBundleFormatStringFromName("certErrorMismatch", 
                                                  params, 1, 
                                                  formattedString);
    if (NS_SUCCEEDED(rv)) {
      returnedMessage.Append(formattedString);
      returnedMessage.Append(NS_LITERAL_STRING("\n"));
    }
    return;
  }

  nsString allNames;
  PRUint32 nameCount = 0;
  bool useSAN = false;

  if (nssCert)
    useSAN = GetSubjectAltNames(nssCert, component, allNames, nameCount);

  if (!useSAN) {
    char *certName = nullptr;
    // currently CERT_FindNSStringExtension is not being exported by NSS.
    // If it gets exported, enable the following line.
    //   certName = CERT_FindNSStringExtension(nssCert, SEC_OID_NS_CERT_EXT_SSL_SERVER_NAME);
    // However, it has been discussed to treat the extension as obsolete and ignore it.
    if (!certName)
      certName = CERT_GetCommonName(&nssCert->subject);
    if (certName) {
      ++nameCount;
      allNames.AssignASCII(certName);
      PORT_Free(certName);
    }
  }

  if (nameCount > 1) {
    nsString message;
    rv = component->GetPIPNSSBundleString("certErrorMismatchMultiple", 
                                          message);
    if (NS_SUCCEEDED(rv)) {
      returnedMessage.Append(message);
      returnedMessage.Append(NS_LITERAL_STRING("\n  "));
      returnedMessage.Append(allNames);
      returnedMessage.Append(NS_LITERAL_STRING("  \n"));
    }
  }
  else if (nameCount == 1) {
    const PRUnichar *params[1];
    params[0] = allNames.get();

    nsString formattedString;
    rv = component->PIPBundleFormatStringFromName("certErrorMismatchSingle2", 
                                                  params, 1, 
                                                  formattedString);
    if (NS_SUCCEEDED(rv)) {
      returnedMessage.Append(formattedString);
      returnedMessage.Append(NS_LITERAL_STRING("\n"));
    }
  }
  else { // nameCount == 0
    nsString message;
    nsresult rv = component->GetPIPNSSBundleString("certErrorMismatchNoNames",
                                                   message);
    if (NS_SUCCEEDED(rv)) {
      returnedMessage.Append(message);
      returnedMessage.Append(NS_LITERAL_STRING("\n"));
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
  if (LL_CMP(now, >, notAfter)) {
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

  const PRUnichar *params[2];
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
    returnedMessage.Append(NS_LITERAL_STRING("\n"));
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

    const PRUnichar *params[1];
    params[0] = idU.get();

    nsString formattedString;
    nsresult rv;
    rv = component->PIPBundleFormatStringFromName("certErrorCodePrefix", 
                                                  params, 1, 
                                                  formattedString);
    if (NS_SUCCEEDED(rv)) {
      returnedMessage.Append(NS_LITERAL_STRING("\n"));
      returnedMessage.Append(formattedString);
      returnedMessage.Append(NS_LITERAL_STRING("\n"));
    }
    else {
      returnedMessage.Append(NS_LITERAL_STRING(" ("));
      returnedMessage.Append(idU);
      returnedMessage.Append(NS_LITERAL_STRING(")"));
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
                                  const nsXPIDLCString & host, PRInt32 port,
                                  nsString & returnedMessage)
{
  const PRUnichar *params[1];
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
  if (port == 443) {
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

  returnedMessage.Append(NS_LITERAL_STRING("\n\n"));

  nsRefPtr<nsIX509Cert> ix509;
  rv = sslStatus.GetServerCert(getter_AddRefs(ix509));
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
    AppendErrorTextMismatch(hostWithoutPort, ix509, component, returnedMessage);
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
  : mMutex("RememberCertErrorsTable::mMutex")
{
  mErrorHosts.Init(16);
}

static nsresult
GetHostPortKey(TransportSecurityInfo* infoObject, nsCAutoString &result)
{
  nsresult rv;

  result.Truncate();

  nsXPIDLCString hostName;
  rv = infoObject->GetHostName(getter_Copies(hostName));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 port;
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

  nsCAutoString hostPortKey;
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

  nsCAutoString hostPortKey;
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
                                          PRUint32 collected_errors)
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
