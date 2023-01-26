import * as asn1js from "asn1js";
import * as pvtsutils from "pvtsutils";
import * as pvutils from "pvutils";
import { AuthorityKeyIdentifier } from "./AuthorityKeyIdentifier";
import { BasicOCSPResponse } from "./BasicOCSPResponse";
import { Certificate } from "./Certificate";
import { CertificateRevocationList } from "./CertificateRevocationList";
import * as common from "./common";
import * as Helpers from "./Helpers";
import { GeneralName } from "./GeneralName";
import { id_AnyPolicy, id_AuthorityInfoAccess, id_AuthorityKeyIdentifier, id_BasicConstraints, id_CertificatePolicies, id_CRLDistributionPoints, id_FreshestCRL, id_InhibitAnyPolicy, id_KeyUsage, id_NameConstraints, id_PolicyConstraints, id_PolicyMappings, id_SubjectAltName, id_SubjectKeyIdentifier } from "./ObjectIdentifiers";
import { RelativeDistinguishedNames } from "./RelativeDistinguishedNames";
import { GeneralSubtree } from "./GeneralSubtree";
import { EMPTY_STRING } from "./constants";

const TRUSTED_CERTS = "trustedCerts";
const CERTS = "certs";
const CRLS = "crls";
const OCSPS = "ocsps";
const CHECK_DATE = "checkDate";
const FIND_ORIGIN = "findOrigin";
const FIND_ISSUER = "findIssuer";


export enum ChainValidationCode {
  unknown = -1,
  success = 0,
  noRevocation = 11,
  noPath = 60,
  noValidPath = 97,
}

export class ChainValidationError extends Error {

  public static readonly NAME = "ChainValidationError";

  public code: ChainValidationCode;

  constructor(code: ChainValidationCode, message: string) {
    super(message);

    this.name = ChainValidationError.NAME;
    this.code = code;
    this.message = message;
  }

}

export interface CertificateChainValidationEngineVerifyResult {
  result: boolean;
  resultCode: number;
  resultMessage: string;
  error?: Error | ChainValidationError;
  authConstrPolicies?: string[];
  userConstrPolicies?: string[];
  explicitPolicyIndicator?: boolean;
  policyMappings?: string[];
  certificatePath?: Certificate[];
}

export type FindOriginCallback = (certificate: Certificate, validationEngine: CertificateChainValidationEngine) => string;
export type FindIssuerCallback = (certificate: Certificate, validationEngine: CertificateChainValidationEngine, crypto?: common.ICryptoEngine) => Promise<Certificate[]>;

export interface CertificateChainValidationEngineParameters {
  trustedCerts?: Certificate[];
  certs?: Certificate[];
  crls?: CertificateRevocationList[];
  ocsps?: BasicOCSPResponse[];
  checkDate?: Date;
  findOrigin?: FindOriginCallback;
  findIssuer?: FindIssuerCallback;
}
interface CrlAndCertificate {
  crl: CertificateRevocationList;
  certificate: Certificate;
}

interface FindCrlResult {
  status: number;
  statusMessage: string;
  result?: CrlAndCertificate[];
}

export interface CertificateChainValidationEngineVerifyParams {
  initialPolicySet?: string[];
  initialExplicitPolicy?: boolean;
  initialPolicyMappingInhibit?: boolean;
  initialInhibitPolicy?: boolean;
  initialPermittedSubtreesSet?: GeneralSubtree[];
  initialExcludedSubtreesSet?: GeneralSubtree[];
  initialRequiredNameForms?: GeneralSubtree[];
  passedWhenNotRevValues?: boolean;
}

/**
 * Returns `true` if the certificate is in the trusted list, otherwise `false`
 * @param cert A certificate that is expected to be in the trusted list
 * @param trustedList List of trusted certificates
 * @returns
 */
function isTrusted(cert: Certificate, trustedList: Certificate[]): boolean {
  for (let i = 0; i < trustedList.length; i++) {
    if (pvtsutils.BufferSourceConverter.isEqual(cert.tbsView, trustedList[i].tbsView)) {
      return true;
    }
  }

  return false;
}

/**
 * Represents a chain-building engine for {@link Certificate} certificates.
 *
 * @example
 * ```js The following example demonstrates how to verify certificate chain
 * const rootCa = pkijs.Certificate.fromBER(certRaw1);
 * const intermediateCa = pkijs.Certificate.fromBER(certRaw2);
 * const leafCert = pkijs.Certificate.fromBER(certRaw3);
 * const crl1 = pkijs.CertificateRevocationList.fromBER(crlRaw1);
 * const ocsp1 = pkijs.BasicOCSPResponse.fromBER(ocspRaw1);
 *
 * const chainEngine = new pkijs.CertificateChainValidationEngine({
 *   certs: [rootCa, intermediateCa, leafCert],
 *   crls: [crl1],
 *   ocsps: [ocsp1],
 *   checkDate: new Date("2015-07-13"), // optional
 *   trustedCerts: [rootCa],
 * });
 *
 * const chain = await chainEngine.verify();
 * ```
 */
export class CertificateChainValidationEngine {

  /**
   * Array of pre-defined trusted (by user) certificates
   */
  public trustedCerts: Certificate[];
  /**
   * Array with certificate chain. Could be only one end-user certificate in there!
   */
  public certs: Certificate[];
  /**
   * Array of all CRLs for all certificates from certificate chain
   */
  public crls: CertificateRevocationList[];
  /**
   * Array of all OCSP responses
   */
  public ocsps: BasicOCSPResponse[];
  /**
   * The date at which the check would be
   */
  public checkDate: Date;
  /**
   * The date at which the check would be
   */
  public findOrigin: FindOriginCallback;
  /**
   * The date at which the check would be
   */
  public findIssuer: FindIssuerCallback;

  /**
   * Constructor for CertificateChainValidationEngine class
   * @param parameters
   */
  constructor(parameters: CertificateChainValidationEngineParameters = {}) {
    //#region Internal properties of the object
    this.trustedCerts = pvutils.getParametersValue(parameters, TRUSTED_CERTS, this.defaultValues(TRUSTED_CERTS));
    this.certs = pvutils.getParametersValue(parameters, CERTS, this.defaultValues(CERTS));
    this.crls = pvutils.getParametersValue(parameters, CRLS, this.defaultValues(CRLS));
    this.ocsps = pvutils.getParametersValue(parameters, OCSPS, this.defaultValues(OCSPS));
    this.checkDate = pvutils.getParametersValue(parameters, CHECK_DATE, this.defaultValues(CHECK_DATE));
    this.findOrigin = pvutils.getParametersValue(parameters, FIND_ORIGIN, this.defaultValues(FIND_ORIGIN));
    this.findIssuer = pvutils.getParametersValue(parameters, FIND_ISSUER, this.defaultValues(FIND_ISSUER));
    //#endregion
  }

  public static defaultFindOrigin(certificate: Certificate, validationEngine: CertificateChainValidationEngine): string {
    //#region Firstly encode TBS for certificate
    if (certificate.tbsView.byteLength === 0) {
      certificate.tbsView = new Uint8Array(certificate.encodeTBS().toBER());
    }
    //#endregion

    //#region Search in Intermediate Certificates
    for (const localCert of validationEngine.certs) {
      //#region Firstly encode TBS for certificate
      if (localCert.tbsView.byteLength === 0) {
        localCert.tbsView = new Uint8Array(localCert.encodeTBS().toBER());
      }
      //#endregion

      if (pvtsutils.BufferSourceConverter.isEqual(certificate.tbsView, localCert.tbsView))
        return "Intermediate Certificates";
    }
    //#endregion

    //#region Search in Trusted Certificates
    for (const trustedCert of validationEngine.trustedCerts) {
      //#region Firstly encode TBS for certificate
      if (trustedCert.tbsView.byteLength === 0)
        trustedCert.tbsView = new Uint8Array(trustedCert.encodeTBS().toBER());
      //#endregion

      if (pvtsutils.BufferSourceConverter.isEqual(certificate.tbsView, trustedCert.tbsView))
        return "Trusted Certificates";
    }
    //#endregion

    return "Unknown";
  }

  public async defaultFindIssuer(certificate: Certificate, validationEngine: CertificateChainValidationEngine, crypto = common.getCrypto(true)): Promise<Certificate[]> {
    //#region Initial variables
    const result: Certificate[] = [];

    let keyIdentifier: asn1js.OctetString | null = null;
    let authorityCertIssuer: GeneralName[] | null = null;
    let authorityCertSerialNumber: asn1js.Integer | null = null;
    //#endregion

    //#region Speed-up searching in case of self-signed certificates
    if (certificate.subject.isEqual(certificate.issuer)) {
      try {
        const verificationResult = await certificate.verify(undefined, crypto);
        if (verificationResult) {
          return [certificate];
        }
      }
      catch (ex) {
        // nothing
      }
    }
    //#endregion

    //#region Find values to speed-up search
    if (certificate.extensions) {
      for (const extension of certificate.extensions) {
        if (extension.extnID === id_AuthorityKeyIdentifier && extension.parsedValue instanceof AuthorityKeyIdentifier) {
          if (extension.parsedValue.keyIdentifier) {
            keyIdentifier = extension.parsedValue.keyIdentifier;
          } else {
            if (extension.parsedValue.authorityCertIssuer) {
              authorityCertIssuer = extension.parsedValue.authorityCertIssuer;
            }
            if (extension.parsedValue.authorityCertSerialNumber) {
              authorityCertSerialNumber = extension.parsedValue.authorityCertSerialNumber;
            }
          }

          break;
        }
      }
    }
    //#endregion

    // Aux function
    function checkCertificate(possibleIssuer: Certificate): void {
      //#region Firstly search for appropriate extensions
      if (keyIdentifier !== null) {
        if (possibleIssuer.extensions) {
          let extensionFound = false;

          for (const extension of possibleIssuer.extensions) {
            if (extension.extnID === id_SubjectKeyIdentifier && extension.parsedValue) {
              extensionFound = true;

              if (pvtsutils.BufferSourceConverter.isEqual(extension.parsedValue.valueBlock.valueHex, keyIdentifier.valueBlock.valueHexView)) {
                result.push(possibleIssuer);
              }

              break;
            }
          }

          if (extensionFound) {
            return;
          }
        }
      }
      //#endregion

      //#region Now search for authorityCertSerialNumber
      let authorityCertSerialNumberEqual = false;

      if (authorityCertSerialNumber !== null)
        authorityCertSerialNumberEqual = possibleIssuer.serialNumber.isEqual(authorityCertSerialNumber);
      //#endregion

      //#region And at least search for Issuer data
      if (authorityCertIssuer !== null) {
        if (possibleIssuer.subject.isEqual(authorityCertIssuer)) {
          if (authorityCertSerialNumberEqual)
            result.push(possibleIssuer);
        }
      }
      else {
        if (certificate.issuer.isEqual(possibleIssuer.subject))
          result.push(possibleIssuer);
      }
      //#endregion
    }

    // Search in Trusted Certificates
    for (const trustedCert of validationEngine.trustedCerts) {
      checkCertificate(trustedCert);
    }

    // Search in Intermediate Certificates
    for (const intermediateCert of validationEngine.certs) {
      checkCertificate(intermediateCert);
    }

    // Now perform certificate verification checking
    for (let i = 0; i < result.length; i++) {
      try {
        const verificationResult = await certificate.verify(result[i], crypto);
        if (verificationResult === false)
          result.splice(i, 1);
      }
      catch (ex) {
        result.splice(i, 1); // Something wrong, remove the certificate
      }
    }

    return result;
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public defaultValues(memberName: typeof TRUSTED_CERTS): Certificate[];
  public defaultValues(memberName: typeof CERTS): Certificate[];
  public defaultValues(memberName: typeof CRLS): CertificateRevocationList[];
  public defaultValues(memberName: typeof OCSPS): BasicOCSPResponse[];
  public defaultValues(memberName: typeof CHECK_DATE): Date;
  public defaultValues(memberName: typeof FIND_ORIGIN): FindOriginCallback;
  public defaultValues(memberName: typeof FIND_ISSUER): FindIssuerCallback;
  public defaultValues(memberName: string): any {
    switch (memberName) {
      case TRUSTED_CERTS:
        return [];
      case CERTS:
        return [];
      case CRLS:
        return [];
      case OCSPS:
        return [];
      case CHECK_DATE:
        return new Date();
      case FIND_ORIGIN:
        return CertificateChainValidationEngine.defaultFindOrigin;
      case FIND_ISSUER:
        return this.defaultFindIssuer;
      default:
        throw new Error(`Invalid member name for CertificateChainValidationEngine class: ${memberName}`);
    }
  }

  public async sort(passedWhenNotRevValues = false, crypto = common.getCrypto(true)): Promise<Certificate[]> {
    // Initial variables
    const localCerts: Certificate[] = [];

    //#region Building certificate path
    const buildPath = async (certificate: Certificate, crypto: common.ICryptoEngine): Promise<Certificate[][]> => {
      const result: Certificate[][] = [];

      // Aux function checking array for unique elements
      function checkUnique(array: Certificate[]): boolean {
        let unique = true;

        for (let i = 0; i < array.length; i++) {
          for (let j = 0; j < array.length; j++) {
            if (j === i)
              continue;

            if (array[i] === array[j]) {
              unique = false;
              break;
            }
          }

          if (!unique)
            break;
        }

        return unique;
      }

      if (isTrusted(certificate, this.trustedCerts)) {
        return [[certificate]];
      }

      const findIssuerResult = await this.findIssuer(certificate, this, crypto);
      if (findIssuerResult.length === 0) {
        throw new Error("No valid certificate paths found");
      }

      for (let i = 0; i < findIssuerResult.length; i++) {
        if (pvtsutils.BufferSourceConverter.isEqual(findIssuerResult[i].tbsView, certificate.tbsView)) {
          result.push([findIssuerResult[i]]);
          continue;
        }

        const buildPathResult = await buildPath(findIssuerResult[i], crypto);

        for (let j = 0; j < buildPathResult.length; j++) {
          const copy = buildPathResult[j].slice();
          copy.splice(0, 0, findIssuerResult[i]);

          if (checkUnique(copy))
            result.push(copy);
          else
            result.push(buildPathResult[j]);
        }
      }

      return result;
    };
    //#endregion

    //#region Find CRL for specific certificate
    const findCRL = async (certificate: Certificate): Promise<FindCrlResult> => {
      //#region Initial variables
      const issuerCertificates: Certificate[] = [];
      const crls: CertificateRevocationList[] = [];
      const crlsAndCertificates: CrlAndCertificate[] = [];
      //#endregion

      //#region Find all possible CRL issuers
      issuerCertificates.push(...localCerts.filter(element => certificate.issuer.isEqual(element.subject)));
      if (issuerCertificates.length === 0) {
        return {
          status: 1,
          statusMessage: "No certificate's issuers"
        };
      }
      //#endregion

      //#region Find all CRLs for certificate's issuer
      crls.push(...this.crls.filter(o => o.issuer.isEqual(certificate.issuer)));
      if (crls.length === 0) {
        return {
          status: 2,
          statusMessage: "No CRLs for specific certificate issuer"
        };
      }
      //#endregion

      //#region Find specific certificate of issuer for each CRL
      for (let i = 0; i < crls.length; i++) {
        const crl = crls[i];
        //#region Check "nextUpdate" for the CRL
        // The "nextUpdate" is older than CHECK_DATE.
        // Thus we should do have another, updated CRL.
        // Thus the CRL assumed to be invalid.
        if (crl.nextUpdate && crl.nextUpdate.value < this.checkDate) {
          continue;
        }
        //#endregion

        for (let j = 0; j < issuerCertificates.length; j++) {
          try {
            const result = await crls[i].verify({ issuerCertificate: issuerCertificates[j] }, crypto);
            if (result) {
              crlsAndCertificates.push({
                crl: crls[i],
                certificate: issuerCertificates[j]
              });

              break;
            }
          }
          catch (ex) {
            // nothing
          }
        }
      }
      //#endregion

      if (crlsAndCertificates.length) {
        return {
          status: 0,
          statusMessage: EMPTY_STRING,
          result: crlsAndCertificates
        };
      }

      return {
        status: 3,
        statusMessage: "No valid CRLs found"
      };
    };
    //#endregion

    //#region Find OCSP for specific certificate
    const findOCSP = async (certificate: Certificate, issuerCertificate: Certificate): Promise<number> => {
      //#region Get hash algorithm from certificate
      const hashAlgorithm = crypto.getAlgorithmByOID<any>(certificate.signatureAlgorithm.algorithmId);
      if (!hashAlgorithm.name) {
        return 1;
      }
      if (!hashAlgorithm.hash) {
        return 1;
      }
      //#endregion

      //#region Search for OCSP response for the certificate
      for (let i = 0; i < this.ocsps.length; i++) {
        const ocsp = this.ocsps[i];
        const result = await ocsp.getCertificateStatus(certificate, issuerCertificate, crypto);
        if (result.isForCertificate) {
          if (result.status === 0)
            return 0;

          return 1;
        }
      }
      //#endregion

      return 2;
    };
    //#endregion

    //#region Check for certificate to be CA
    async function checkForCA(certificate: Certificate, needToCheckCRL = false) {
      //#region Initial variables
      let isCA = false;
      let mustBeCA = false;
      let keyUsagePresent = false;
      let cRLSign = false;
      //#endregion

      if (certificate.extensions) {
        for (let j = 0; j < certificate.extensions.length; j++) {
          const extension = certificate.extensions[j];
          if (extension.critical && !extension.parsedValue) {
            return {
              result: false,
              resultCode: 6,
              resultMessage: `Unable to parse critical certificate extension: ${extension.extnID}`
            };
          }

          if (extension.extnID === id_KeyUsage) // KeyUsage
          {
            keyUsagePresent = true;

            const view = new Uint8Array(extension.parsedValue.valueBlock.valueHex);

            if ((view[0] & 0x04) === 0x04) // Set flag "keyCertSign"
              mustBeCA = true;

            if ((view[0] & 0x02) === 0x02) // Set flag "cRLSign"
              cRLSign = true;
          }

          if (extension.extnID === id_BasicConstraints) // BasicConstraints
          {
            if ("cA" in extension.parsedValue) {
              if (extension.parsedValue.cA === true)
                isCA = true;
            }
          }
        }

        if ((mustBeCA === true) && (isCA === false)) {
          return {
            result: false,
            resultCode: 3,
            resultMessage: "Unable to build certificate chain - using \"keyCertSign\" flag set without BasicConstraints"
          };
        }

        if ((keyUsagePresent === true) && (isCA === true) && (mustBeCA === false)) {
          return {
            result: false,
            resultCode: 4,
            resultMessage: "Unable to build certificate chain - \"keyCertSign\" flag was not set"
          };
        }

        if ((isCA === true) && (keyUsagePresent === true) && ((needToCheckCRL) && (cRLSign === false))) {
          return {
            result: false,
            resultCode: 5,
            resultMessage: "Unable to build certificate chain - intermediate certificate must have \"cRLSign\" key usage flag"
          };
        }
      }

      if (isCA === false) {
        return {
          result: false,
          resultCode: 7,
          resultMessage: "Unable to build certificate chain - more than one possible end-user certificate"
        };
      }

      return {
        result: true,
        resultCode: 0,
        resultMessage: EMPTY_STRING
      };
    }
    //#endregion

    //#region Basic check for certificate path
    const basicCheck = async (path: Certificate[], checkDate: Date): Promise<{ result: boolean; resultCode?: number; resultMessage?: string; }> => {
      //#region Check that all dates are valid
      for (let i = 0; i < path.length; i++) {
        if ((path[i].notBefore.value > checkDate) ||
          (path[i].notAfter.value < checkDate)) {
          return {
            result: false,
            resultCode: 8,
            resultMessage: "The certificate is either not yet valid or expired"
          };
        }
      }
      //#endregion

      //#region Check certificate name chain

      // We should have at least two certificates: end entity and trusted root
      if (path.length < 2) {
        return {
          result: false,
          resultCode: 9,
          resultMessage: "Too short certificate path"
        };
      }

      for (let i = (path.length - 2); i >= 0; i--) {
        //#region Check that we do not have a "self-signed" certificate
        if (path[i].issuer.isEqual(path[i].subject) === false) {
          if (path[i].issuer.isEqual(path[i + 1].subject) === false) {
            return {
              result: false,
              resultCode: 10,
              resultMessage: "Incorrect name chaining"
            };
          }
        }
        //#endregion
      }
      //#endregion

      //#region Check each certificate (except "trusted root") to be non-revoked
      if ((this.crls.length !== 0) || (this.ocsps.length !== 0)) // If CRLs and OCSPs are empty then we consider all certificates to be valid
      {
        for (let i = 0; i < (path.length - 1); i++) {
          //#region Initial variables
          let ocspResult = 2;
          let crlResult: FindCrlResult = {
            status: 0,
            statusMessage: EMPTY_STRING
          };
          //#endregion

          //#region Check OCSPs first
          if (this.ocsps.length !== 0) {
            ocspResult = await findOCSP(path[i], path[i + 1]);

            switch (ocspResult) {
              case 0:
                continue;
              case 1:
                return {
                  result: false,
                  resultCode: 12,
                  resultMessage: "One of certificates was revoked via OCSP response"
                };
              case 2: // continue to check the certificate with CRL
                break;
              default:
            }
          }
          //#endregion

          //#region Check CRLs
          if (this.crls.length !== 0) {
            crlResult = await findCRL(path[i]);

            if (crlResult.status === 0 && crlResult.result) {
              for (let j = 0; j < crlResult.result.length; j++) {
                //#region Check that the CRL issuer certificate have not been revoked
                const isCertificateRevoked = crlResult.result[j].crl.isCertificateRevoked(path[i]);
                if (isCertificateRevoked) {
                  return {
                    result: false,
                    resultCode: 12,
                    resultMessage: "One of certificates had been revoked"
                  };
                }
                //#endregion

                //#region Check that the CRL issuer certificate is a CA certificate
                const isCertificateCA = await checkForCA(crlResult.result[j].certificate, true);
                if (isCertificateCA.result === false) {
                  return {
                    result: false,
                    resultCode: 13,
                    resultMessage: "CRL issuer certificate is not a CA certificate or does not have crlSign flag"
                  };
                }
                //#endregion
              }
            } else {
              if (passedWhenNotRevValues === false) {
                throw new ChainValidationError(ChainValidationCode.noRevocation, `No revocation values found for one of certificates: ${crlResult.statusMessage}`);
              }
            }
          } else {
            if (ocspResult === 2) {
              return {
                result: false,
                resultCode: 11,
                resultMessage: "No revocation values found for one of certificates"
              };
            }
          }
          //#endregion

          //#region Check we do have links to revocation values inside issuer's certificate
          if ((ocspResult === 2) && (crlResult.status === 2) && passedWhenNotRevValues) {
            const issuerCertificate = path[i + 1];
            let extensionFound = false;

            if (issuerCertificate.extensions) {
              for (const extension of issuerCertificate.extensions) {
                switch (extension.extnID) {
                  case id_CRLDistributionPoints:
                  case id_FreshestCRL:
                  case id_AuthorityInfoAccess:
                    extensionFound = true;
                    break;
                  default:
                }
              }
            }

            if (extensionFound) {
              throw new ChainValidationError(ChainValidationCode.noRevocation, `No revocation values found for one of certificates: ${crlResult.statusMessage}`);
            }
          }
          //#endregion
        }
      }
      //#endregion

      //#region Check each certificate (except "end entity") in the path to be a CA certificate
      for (const [i, cert] of path.entries()) {
        if (!i) {
          // Skip entity certificate
          continue;
        }

        const result = await checkForCA(cert);
        if (!result.result) {
          return {
            result: false,
            resultCode: 14,
            resultMessage: "One of intermediate certificates is not a CA certificate"
          };
        }
      }
      //#endregion

      return {
        result: true
      };
    };
    //#endregion

    //#region Do main work
    //#region Initialize "localCerts" by value of "this.certs" + "this.trustedCerts" arrays
    localCerts.push(...this.trustedCerts);
    localCerts.push(...this.certs);
    //#endregion

    //#region Check all certificates for been unique
    for (let i = 0; i < localCerts.length; i++) {
      for (let j = 0; j < localCerts.length; j++) {
        if (i === j)
          continue;

        if (pvtsutils.BufferSourceConverter.isEqual(localCerts[i].tbsView, localCerts[j].tbsView)) {
          localCerts.splice(j, 1);
          i = 0;
          break;
        }
      }
    }
    //#endregion

    const leafCert = localCerts[localCerts.length - 1];

    //#region Initial variables
    let result;
    const certificatePath = [leafCert]; // The "end entity" certificate must be the least in CERTS array
    //#endregion

    //#region Build path for "end entity" certificate
    result = await buildPath(leafCert, crypto);
    if (result.length === 0) {
      throw new ChainValidationError(ChainValidationCode.noPath, "Unable to find certificate path");
    }
    //#endregion

    //#region Exclude certificate paths not ended with "trusted roots"
    for (let i = 0; i < result.length; i++) {
      let found = false;

      for (let j = 0; j < (result[i]).length; j++) {
        const certificate = (result[i])[j];

        for (let k = 0; k < this.trustedCerts.length; k++) {
          if (pvtsutils.BufferSourceConverter.isEqual(certificate.tbsView, this.trustedCerts[k].tbsView)) {
            found = true;
            break;
          }
        }

        if (found)
          break;
      }

      if (!found) {
        result.splice(i, 1);
        i = 0;
      }
    }

    if (result.length === 0) {
      throw new ChainValidationError(ChainValidationCode.noValidPath, "No valid certificate paths found");
    }
    //#endregion

    //#region Find shortest certificate path (for the moment it is the only criteria)
    let shortestLength = result[0].length;
    let shortestIndex = 0;

    for (let i = 0; i < result.length; i++) {
      if (result[i].length < shortestLength) {
        shortestLength = result[i].length;
        shortestIndex = i;
      }
    }
    //#endregion

    //#region Create certificate path for basic check
    for (let i = 0; i < result[shortestIndex].length; i++)
      certificatePath.push((result[shortestIndex])[i]);
    //#endregion

    //#region Perform basic checking for all certificates in the path
    result = await basicCheck(certificatePath, this.checkDate);
    if (result.result === false)
      throw result;
    //#endregion

    return certificatePath;
    //#endregion
  }

  /**
   * Major verification function for certificate chain.
   * @param parameters
   * @param crypto Crypto engine
   * @returns
   */
  async verify(parameters: CertificateChainValidationEngineVerifyParams = {}, crypto = common.getCrypto(true)): Promise<CertificateChainValidationEngineVerifyResult> {
    //#region Auxiliary functions for name constraints checking
    /**
     * Compare two dNSName values
     * @param name DNS from name
     * @param constraint Constraint for DNS from name
     * @returns Boolean result - valid or invalid the "name" against the "constraint"
     */
    function compareDNSName(name: string, constraint: string): boolean {
      //#region Make a "string preparation" for both name and constrain
      const namePrepared = Helpers.stringPrep(name);
      const constraintPrepared = Helpers.stringPrep(constraint);
      //#endregion

      //#region Make a "splitted" versions of "constraint" and "name"
      const nameSplitted = namePrepared.split(".");
      const constraintSplitted = constraintPrepared.split(".");
      //#endregion

      //#region Length calculation and additional check
      const nameLen = nameSplitted.length;
      const constrLen = constraintSplitted.length;

      if ((nameLen === 0) || (constrLen === 0) || (nameLen < constrLen)) {
        return false;
      }
      //#endregion

      //#region Check that no part of "name" has zero length
      for (let i = 0; i < nameLen; i++) {
        if (nameSplitted[i].length === 0) {
          return false;
        }
      }
      //#endregion

      //#region Check that no part of "constraint" has zero length
      for (let i = 0; i < constrLen; i++) {
        if (constraintSplitted[i].length === 0) {
          if (i === 0) {
            if (constrLen === 1) {
              return false;
            }

            continue;
          }

          return false;
        }
      }
      //#endregion

      //#region Check that "name" has a tail as "constraint"

      for (let i = 0; i < constrLen; i++) {
        if (constraintSplitted[constrLen - 1 - i].length === 0) {
          continue;
        }

        if (nameSplitted[nameLen - 1 - i].localeCompare(constraintSplitted[constrLen - 1 - i]) !== 0) {
          return false;
        }
      }
      //#endregion

      return true;
    }

    /**
     * Compare two rfc822Name values
     * @param name E-mail address from name
     * @param constraint Constraint for e-mail address from name
     * @returns Boolean result - valid or invalid the "name" against the "constraint"
     */
    function compareRFC822Name(name: string, constraint: string): boolean {
      //#region Make a "string preparation" for both name and constrain
      const namePrepared = Helpers.stringPrep(name);
      const constraintPrepared = Helpers.stringPrep(constraint);
      //#endregion

      //#region Make a "splitted" versions of "constraint" and "name"
      const nameSplitted = namePrepared.split("@");
      const constraintSplitted = constraintPrepared.split("@");
      //#endregion

      //#region Splitted array length checking
      if ((nameSplitted.length === 0) || (constraintSplitted.length === 0) || (nameSplitted.length < constraintSplitted.length))
        return false;
      //#endregion

      if (constraintSplitted.length === 1) {
        const result = compareDNSName(nameSplitted[1], constraintSplitted[0]);

        if (result) {
          //#region Make a "splitted" versions of domain name from "constraint" and "name"
          const ns = nameSplitted[1].split(".");
          const cs = constraintSplitted[0].split(".");
          //#endregion

          if (cs[0].length === 0)
            return true;

          return ns.length === cs.length;
        }

        return false;
      }

      return (namePrepared.localeCompare(constraintPrepared) === 0);
    }

    /**
     * Compare two uniformResourceIdentifier values
     * @param name uniformResourceIdentifier from name
     * @param constraint Constraint for uniformResourceIdentifier from name
     * @returns Boolean result - valid or invalid the "name" against the "constraint"
     */
    function compareUniformResourceIdentifier(name: string, constraint: string): boolean {
      //#region Make a "string preparation" for both name and constrain
      let namePrepared = Helpers.stringPrep(name);
      const constraintPrepared = Helpers.stringPrep(constraint);
      //#endregion

      //#region Find out a major URI part to compare with
      const ns = namePrepared.split("/");
      const cs = constraintPrepared.split("/");

      if (cs.length > 1) // Malformed constraint
        return false;

      if (ns.length > 1) // Full URI string
      {
        for (let i = 0; i < ns.length; i++) {
          if ((ns[i].length > 0) && (ns[i].charAt(ns[i].length - 1) !== ":")) {
            const nsPort = ns[i].split(":");
            namePrepared = nsPort[0];
            break;
          }
        }
      }
      //#endregion

      const result = compareDNSName(namePrepared, constraintPrepared);

      if (result) {
        //#region Make a "splitted" versions of "constraint" and "name"
        const nameSplitted = namePrepared.split(".");
        const constraintSplitted = constraintPrepared.split(".");
        //#endregion

        if (constraintSplitted[0].length === 0)
          return true;

        return nameSplitted.length === constraintSplitted.length;
      }

      return false;
    }

    /**
     * Compare two iPAddress values
     * @param name iPAddress from name
     * @param constraint Constraint for iPAddress from name
     * @returns Boolean result - valid or invalid the "name" against the "constraint"
     */
    function compareIPAddress(name: asn1js.OctetString, constraint: asn1js.OctetString): boolean {
      //#region Common variables
      const nameView = name.valueBlock.valueHexView;
      const constraintView = constraint.valueBlock.valueHexView;
      //#endregion

      //#region Work with IPv4 addresses
      if ((nameView.length === 4) && (constraintView.length === 8)) {
        for (let i = 0; i < 4; i++) {
          if ((nameView[i] ^ constraintView[i]) & constraintView[i + 4])
            return false;
        }

        return true;
      }
      //#endregion

      //#region Work with IPv6 addresses
      if ((nameView.length === 16) && (constraintView.length === 32)) {
        for (let i = 0; i < 16; i++) {
          if ((nameView[i] ^ constraintView[i]) & constraintView[i + 16])
            return false;
        }

        return true;
      }
      //#endregion

      return false;
    }

    /**
     * Compare two directoryName values
     * @param name directoryName from name
     * @param constraint Constraint for directoryName from name
     * @returns Boolean result - valid or invalid the "name" against the "constraint"
     */
    function compareDirectoryName(name: RelativeDistinguishedNames, constraint: RelativeDistinguishedNames): boolean {
      //#region Initial check
      if ((name.typesAndValues.length === 0) || (constraint.typesAndValues.length === 0))
        return true;

      if (name.typesAndValues.length < constraint.typesAndValues.length)
        return false;
      //#endregion

      //#region Initial variables
      let result = true;
      let nameStart = 0;
      //#endregion

      for (let i = 0; i < constraint.typesAndValues.length; i++) {
        let localResult = false;

        for (let j = nameStart; j < name.typesAndValues.length; j++) {
          localResult = name.typesAndValues[j].isEqual(constraint.typesAndValues[i]);

          if (name.typesAndValues[j].type === constraint.typesAndValues[i].type)
            result = result && localResult;

          if (localResult === true) {
            if ((nameStart === 0) || (nameStart === j)) {
              nameStart = j + 1;
              break;
            }
            else // Structure of "name" must be the same with "constraint"
              return false;
          }
        }

        if (localResult === false)
          return false;
      }

      return (nameStart === 0) ? false : result;
    }
    //#endregion

    try {
      //#region Initial checks
      if (this.certs.length === 0)
        throw new Error("Empty certificate array");
      //#endregion

      //#region Get input variables
      const passedWhenNotRevValues = parameters.passedWhenNotRevValues || false;

      const initialPolicySet = parameters.initialPolicySet || [id_AnyPolicy];

      const initialExplicitPolicy = parameters.initialExplicitPolicy || false;
      const initialPolicyMappingInhibit = parameters.initialPolicyMappingInhibit || false;
      const initialInhibitPolicy = parameters.initialInhibitPolicy || false;

      const initialPermittedSubtreesSet = parameters.initialPermittedSubtreesSet || [];
      const initialExcludedSubtreesSet = parameters.initialExcludedSubtreesSet || [];
      const initialRequiredNameForms = parameters.initialRequiredNameForms || [];

      let explicitPolicyIndicator = initialExplicitPolicy;
      let policyMappingInhibitIndicator = initialPolicyMappingInhibit;
      let inhibitAnyPolicyIndicator = initialInhibitPolicy;

      const pendingConstraints = [
        false, // For "explicitPolicyPending"
        false, // For "policyMappingInhibitPending"
        false, // For "inhibitAnyPolicyPending"
      ];

      let explicitPolicyPending = 0;
      let policyMappingInhibitPending = 0;
      let inhibitAnyPolicyPending = 0;

      let permittedSubtrees = initialPermittedSubtreesSet;
      let excludedSubtrees = initialExcludedSubtreesSet;
      const requiredNameForms = initialRequiredNameForms;

      let pathDepth = 1;
      //#endregion

      //#region Sorting certificates in the chain array
      this.certs = await this.sort(passedWhenNotRevValues, crypto);
      //#endregion

      //#region Work with policies
      //#region Support variables
      const allPolicies: string[] = []; // Array of all policies (string values)
      allPolicies.push(id_AnyPolicy); // Put "anyPolicy" at first place

      const policiesAndCerts = []; // In fact "array of array" where rows are for each specific policy, column for each certificate and value is "true/false"

      const anyPolicyArray = new Array(this.certs.length - 1); // Minus "trusted anchor"
      for (let ii = 0; ii < (this.certs.length - 1); ii++)
        anyPolicyArray[ii] = true;

      policiesAndCerts.push(anyPolicyArray);

      const policyMappings = new Array(this.certs.length - 1); // Array of "PolicyMappings" for each certificate
      const certPolicies = new Array(this.certs.length - 1); // Array of "CertificatePolicies" for each certificate

      let explicitPolicyStart = (explicitPolicyIndicator) ? (this.certs.length - 1) : (-1);
      //#endregion

      //#region Gather all necessary information from certificate chain
      for (let i = (this.certs.length - 2); i >= 0; i--, pathDepth++) {
        const cert = this.certs[i];
        if (cert.extensions) {
          //#region Get information about certificate extensions
          for (let j = 0; j < cert.extensions.length; j++) {
            const extension = cert.extensions[j];
            //#region CertificatePolicies
            if (extension.extnID === id_CertificatePolicies) {
              certPolicies[i] = extension.parsedValue;

              //#region Remove entry from "anyPolicies" for the certificate
              for (let s = 0; s < allPolicies.length; s++) {
                if (allPolicies[s] === id_AnyPolicy) {
                  delete (policiesAndCerts[s])[i];
                  break;
                }
              }
              //#endregion

              for (let k = 0; k < extension.parsedValue.certificatePolicies.length; k++) {
                let policyIndex = (-1);
                const policyId = extension.parsedValue.certificatePolicies[k].policyIdentifier;

                //#region Try to find extension in "allPolicies" array
                for (let s = 0; s < allPolicies.length; s++) {
                  if (policyId === allPolicies[s]) {
                    policyIndex = s;
                    break;
                  }
                }
                //#endregion

                if (policyIndex === (-1)) {
                  allPolicies.push(policyId);

                  const certArray = new Array(this.certs.length - 1);
                  certArray[i] = true;

                  policiesAndCerts.push(certArray);
                }
                else
                  (policiesAndCerts[policyIndex])[i] = true;
              }
            }
            //#endregion

            //#region PolicyMappings
            if (extension.extnID === id_PolicyMappings) {
              if (policyMappingInhibitIndicator) {
                return {
                  result: false,
                  resultCode: 98,
                  resultMessage: "Policy mapping prohibited"
                };
              }

              policyMappings[i] = extension.parsedValue;
            }
            //#endregion

            //#region PolicyConstraints
            if (extension.extnID === id_PolicyConstraints) {
              if (explicitPolicyIndicator === false) {
                //#region requireExplicitPolicy
                if (extension.parsedValue.requireExplicitPolicy === 0) {
                  explicitPolicyIndicator = true;
                  explicitPolicyStart = i;
                }
                else {
                  if (pendingConstraints[0] === false) {
                    pendingConstraints[0] = true;
                    explicitPolicyPending = extension.parsedValue.requireExplicitPolicy;
                  }
                  else
                    explicitPolicyPending = (explicitPolicyPending > extension.parsedValue.requireExplicitPolicy) ? extension.parsedValue.requireExplicitPolicy : explicitPolicyPending;
                }
                //#endregion

                //#region inhibitPolicyMapping
                if (extension.parsedValue.inhibitPolicyMapping === 0)
                  policyMappingInhibitIndicator = true;
                else {
                  if (pendingConstraints[1] === false) {
                    pendingConstraints[1] = true;
                    policyMappingInhibitPending = extension.parsedValue.inhibitPolicyMapping + 1;
                  }
                  else
                    policyMappingInhibitPending = (policyMappingInhibitPending > (extension.parsedValue.inhibitPolicyMapping + 1)) ? (extension.parsedValue.inhibitPolicyMapping + 1) : policyMappingInhibitPending;
                }
                //#endregion
              }
            }
            //#endregion

            //#region InhibitAnyPolicy
            if (extension.extnID === id_InhibitAnyPolicy) {
              if (inhibitAnyPolicyIndicator === false) {
                if (extension.parsedValue.valueBlock.valueDec === 0)
                  inhibitAnyPolicyIndicator = true;
                else {
                  if (pendingConstraints[2] === false) {
                    pendingConstraints[2] = true;
                    inhibitAnyPolicyPending = extension.parsedValue.valueBlock.valueDec;
                  }
                  else
                    inhibitAnyPolicyPending = (inhibitAnyPolicyPending > extension.parsedValue.valueBlock.valueDec) ? extension.parsedValue.valueBlock.valueDec : inhibitAnyPolicyPending;
                }
              }
            }
            //#endregion
          }
          //#endregion

          //#region Check "inhibitAnyPolicyIndicator"
          if (inhibitAnyPolicyIndicator === true) {
            let policyIndex = (-1);

            //#region Find "anyPolicy" index
            for (let searchAnyPolicy = 0; searchAnyPolicy < allPolicies.length; searchAnyPolicy++) {
              if (allPolicies[searchAnyPolicy] === id_AnyPolicy) {
                policyIndex = searchAnyPolicy;
                break;
              }
            }
            //#endregion

            if (policyIndex !== (-1))
              delete (policiesAndCerts[0])[i]; // Unset value to "undefined" for "anyPolicies" value for current certificate
          }
          //#endregion

          //#region Process with "pending constraints"
          if (explicitPolicyIndicator === false) {
            if (pendingConstraints[0] === true) {
              explicitPolicyPending--;
              if (explicitPolicyPending === 0) {
                explicitPolicyIndicator = true;
                explicitPolicyStart = i;

                pendingConstraints[0] = false;
              }
            }
          }

          if (policyMappingInhibitIndicator === false) {
            if (pendingConstraints[1] === true) {
              policyMappingInhibitPending--;
              if (policyMappingInhibitPending === 0) {
                policyMappingInhibitIndicator = true;
                pendingConstraints[1] = false;
              }
            }
          }

          if (inhibitAnyPolicyIndicator === false) {
            if (pendingConstraints[2] === true) {
              inhibitAnyPolicyPending--;
              if (inhibitAnyPolicyPending === 0) {
                inhibitAnyPolicyIndicator = true;
                pendingConstraints[2] = false;
              }
            }
          }
          //#endregion
        }
      }
      //#endregion

      //#region Working with policy mappings
      for (let i = 0; i < (this.certs.length - 1); i++) {
        //#region Check that there is "policy mapping" for level "i + 1"
        if ((i < (this.certs.length - 2)) && (typeof policyMappings[i + 1] !== "undefined")) {
          for (let k = 0; k < policyMappings[i + 1].mappings.length; k++) {
            //#region Check that we do not have "anyPolicy" in current mapping
            if ((policyMappings[i + 1].mappings[k].issuerDomainPolicy === id_AnyPolicy) || (policyMappings[i + 1].mappings[k].subjectDomainPolicy === id_AnyPolicy)) {
              return {
                result: false,
                resultCode: 99,
                resultMessage: "The \"anyPolicy\" should not be a part of policy mapping scheme"
              };
            }
            //#endregion

            //#region Initial variables
            let issuerDomainPolicyIndex = (-1);
            let subjectDomainPolicyIndex = (-1);
            //#endregion

            //#region Search for index of policies indexes
            for (let n = 0; n < allPolicies.length; n++) {
              if (allPolicies[n] === policyMappings[i + 1].mappings[k].issuerDomainPolicy)
                issuerDomainPolicyIndex = n;

              if (allPolicies[n] === policyMappings[i + 1].mappings[k].subjectDomainPolicy)
                subjectDomainPolicyIndex = n;
            }
            //#endregion

            //#region Delete existing "issuerDomainPolicy" because on the level we mapped the policy to another one
            if (typeof (policiesAndCerts[issuerDomainPolicyIndex])[i] !== "undefined")
              delete (policiesAndCerts[issuerDomainPolicyIndex])[i];
            //#endregion

            //#region Check all policies for the certificate
            for (let j = 0; j < certPolicies[i].certificatePolicies.length; j++) {
              if (policyMappings[i + 1].mappings[k].subjectDomainPolicy === certPolicies[i].certificatePolicies[j].policyIdentifier) {
                //#region Set mapped policy for current certificate
                if ((issuerDomainPolicyIndex !== (-1)) && (subjectDomainPolicyIndex !== (-1))) {
                  for (let m = 0; m <= i; m++) {
                    if (typeof (policiesAndCerts[subjectDomainPolicyIndex])[m] !== "undefined") {
                      (policiesAndCerts[issuerDomainPolicyIndex])[m] = true;
                      delete (policiesAndCerts[subjectDomainPolicyIndex])[m];
                    }
                  }
                }
                //#endregion
              }
            }
            //#endregion
          }
        }
        //#endregion
      }
      //#endregion

      //#region Working with "explicitPolicyIndicator" and "anyPolicy"
      for (let i = 0; i < allPolicies.length; i++) {
        if (allPolicies[i] === id_AnyPolicy) {
          for (let j = 0; j < explicitPolicyStart; j++)
            delete (policiesAndCerts[i])[j];
        }
      }
      //#endregion

      //#region Create "set of authorities-constrained policies"
      const authConstrPolicies = [];

      for (let i = 0; i < policiesAndCerts.length; i++) {
        let found = true;

        for (let j = 0; j < (this.certs.length - 1); j++) {
          let anyPolicyFound = false;

          if ((j < explicitPolicyStart) && (allPolicies[i] === id_AnyPolicy) && (allPolicies.length > 1)) {
            found = false;
            break;
          }

          if (typeof (policiesAndCerts[i])[j] === "undefined") {
            if (j >= explicitPolicyStart) {
              //#region Search for "anyPolicy" in the policy set
              for (let k = 0; k < allPolicies.length; k++) {
                if (allPolicies[k] === id_AnyPolicy) {
                  if ((policiesAndCerts[k])[j] === true)
                    anyPolicyFound = true;

                  break;
                }
              }
              //#endregion
            }

            if (!anyPolicyFound) {
              found = false;
              break;
            }
          }
        }

        if (found === true)
          authConstrPolicies.push(allPolicies[i]);
      }
      //#endregion

      //#region Create "set of user-constrained policies"
      let userConstrPolicies: string[] = [];

      if ((initialPolicySet.length === 1) && (initialPolicySet[0] === id_AnyPolicy) && (explicitPolicyIndicator === false))
        userConstrPolicies = initialPolicySet;
      else {
        if ((authConstrPolicies.length === 1) && (authConstrPolicies[0] === id_AnyPolicy))
          userConstrPolicies = initialPolicySet;
        else {
          for (let i = 0; i < authConstrPolicies.length; i++) {
            for (let j = 0; j < initialPolicySet.length; j++) {
              if ((initialPolicySet[j] === authConstrPolicies[i]) || (initialPolicySet[j] === id_AnyPolicy)) {
                userConstrPolicies.push(authConstrPolicies[i]);
                break;
              }
            }
          }
        }
      }

      //#endregion

      //#region Combine output object
      const policyResult: CertificateChainValidationEngineVerifyResult = {
        result: (userConstrPolicies.length > 0),
        resultCode: 0,
        resultMessage: (userConstrPolicies.length > 0) ? EMPTY_STRING : "Zero \"userConstrPolicies\" array, no intersections with \"authConstrPolicies\"",
        authConstrPolicies,
        userConstrPolicies,
        explicitPolicyIndicator,
        policyMappings,
        certificatePath: this.certs
      };

      if (userConstrPolicies.length === 0)
        return policyResult;
      //#endregion
      //#endregion

      //#region Work with name constraints
      //#region Check a result from "policy checking" part
      if (policyResult.result === false)
        return policyResult;
      //#endregion

      //#region Check all certificates, excluding "trust anchor"
      pathDepth = 1;

      for (let i = (this.certs.length - 2); i >= 0; i--, pathDepth++) {
        const cert = this.certs[i];
        //#region Support variables
        let subjectAltNames: GeneralName[] = [];

        let certPermittedSubtrees: GeneralSubtree[] = [];
        let certExcludedSubtrees: GeneralSubtree[] = [];
        //#endregion

        if (cert.extensions) {
          for (let j = 0; j < cert.extensions.length; j++) {
            const extension = cert.extensions[j];
            //#region NameConstraints
            if (extension.extnID === id_NameConstraints) {
              if ("permittedSubtrees" in extension.parsedValue)
                certPermittedSubtrees = certPermittedSubtrees.concat(extension.parsedValue.permittedSubtrees);

              if ("excludedSubtrees" in extension.parsedValue)
                certExcludedSubtrees = certExcludedSubtrees.concat(extension.parsedValue.excludedSubtrees);
            }
            //#endregion

            //#region SubjectAltName
            if (extension.extnID === id_SubjectAltName)
              subjectAltNames = subjectAltNames.concat(extension.parsedValue.altNames);
            //#endregion
          }
        }

        //#region Checking for "required name forms"
        let formFound = (requiredNameForms.length <= 0);

        for (let j = 0; j < requiredNameForms.length; j++) {
          switch (requiredNameForms[j].base.type) {
            case 4: // directoryName
              {
                if (requiredNameForms[j].base.value.typesAndValues.length !== cert.subject.typesAndValues.length)
                  continue;

                formFound = true;

                for (let k = 0; k < cert.subject.typesAndValues.length; k++) {
                  if (cert.subject.typesAndValues[k].type !== requiredNameForms[j].base.value.typesAndValues[k].type) {
                    formFound = false;
                    break;
                  }
                }

                if (formFound === true)
                  break;
              }
              break;
            default: // ??? Probably here we should reject the certificate ???
          }
        }

        if (formFound === false) {
          policyResult.result = false;
          policyResult.resultCode = 21;
          policyResult.resultMessage = "No necessary name form found";

          throw policyResult;
        }
        //#endregion

        //#region Checking for "permited sub-trees"
        //#region Make groups for all types of constraints
        const constrGroups: GeneralSubtree[][] = [ // Array of array for groupped constraints
          [], // rfc822Name
          [], // dNSName
          [], // directoryName
          [], // uniformResourceIdentifier
          [], // iPAddress
        ];

        for (let j = 0; j < permittedSubtrees.length; j++) {
          switch (permittedSubtrees[j].base.type) {
            //#region rfc822Name
            case 1:
              constrGroups[0].push(permittedSubtrees[j]);
              break;
            //#endregion
            //#region dNSName
            case 2:
              constrGroups[1].push(permittedSubtrees[j]);
              break;
            //#endregion
            //#region directoryName
            case 4:
              constrGroups[2].push(permittedSubtrees[j]);
              break;
            //#endregion
            //#region uniformResourceIdentifier
            case 6:
              constrGroups[3].push(permittedSubtrees[j]);
              break;
            //#endregion
            //#region iPAddress
            case 7:
              constrGroups[4].push(permittedSubtrees[j]);
              break;
            //#endregion
            //#region default
            default:
            //#endregion
          }
        }
        //#endregion

        //#region Check name constraints groupped by type, one-by-one
        for (let p = 0; p < 5; p++) {
          let groupPermitted = false;
          let valueExists = false;
          const group = constrGroups[p];

          for (let j = 0; j < group.length; j++) {
            switch (p) {
              //#region rfc822Name
              case 0:
                if (subjectAltNames.length > 0) {
                  for (let k = 0; k < subjectAltNames.length; k++) {
                    if (subjectAltNames[k].type === 1) // rfc822Name
                    {
                      valueExists = true;
                      groupPermitted = groupPermitted || compareRFC822Name(subjectAltNames[k].value, group[j].base.value);
                    }
                  }
                }
                else // Try to find out "emailAddress" inside "subject"
                {
                  for (let k = 0; k < cert.subject.typesAndValues.length; k++) {
                    if ((cert.subject.typesAndValues[k].type === "1.2.840.113549.1.9.1") ||    // PKCS#9 e-mail address
                      (cert.subject.typesAndValues[k].type === "0.9.2342.19200300.100.1.3")) // RFC1274 "rfc822Mailbox" e-mail address
                    {
                      valueExists = true;
                      groupPermitted = groupPermitted || compareRFC822Name(cert.subject.typesAndValues[k].value.valueBlock.value, group[j].base.value);
                    }
                  }
                }
                break;
              //#endregion
              //#region dNSName
              case 1:
                if (subjectAltNames.length > 0) {
                  for (let k = 0; k < subjectAltNames.length; k++) {
                    if (subjectAltNames[k].type === 2) // dNSName
                    {
                      valueExists = true;
                      groupPermitted = groupPermitted || compareDNSName(subjectAltNames[k].value, group[j].base.value);
                    }
                  }
                }
                break;
              //#endregion
              //#region directoryName
              case 2:
                valueExists = true;
                groupPermitted = compareDirectoryName(cert.subject, group[j].base.value);
                break;
              //#endregion
              //#region uniformResourceIdentifier
              case 3:
                if (subjectAltNames.length > 0) {
                  for (let k = 0; k < subjectAltNames.length; k++) {
                    if (subjectAltNames[k].type === 6) // uniformResourceIdentifier
                    {
                      valueExists = true;
                      groupPermitted = groupPermitted || compareUniformResourceIdentifier(subjectAltNames[k].value, group[j].base.value);
                    }
                  }
                }
                break;
              //#endregion
              //#region iPAddress
              case 4:
                if (subjectAltNames.length > 0) {
                  for (let k = 0; k < subjectAltNames.length; k++) {
                    if (subjectAltNames[k].type === 7) // iPAddress
                    {
                      valueExists = true;
                      groupPermitted = groupPermitted || compareIPAddress(subjectAltNames[k].value, group[j].base.value);
                    }
                  }
                }
                break;
              //#endregion
              //#region default
              default:
              //#endregion
            }

            if (groupPermitted)
              break;
          }

          if ((groupPermitted === false) && (group.length > 0) && valueExists) {
            policyResult.result = false;
            policyResult.resultCode = 41;
            policyResult.resultMessage = "Failed to meet \"permitted sub-trees\" name constraint";

            throw policyResult;
          }
        }
        //#endregion
        //#endregion

        //#region Checking for "excluded sub-trees"
        let excluded = false;

        for (let j = 0; j < excludedSubtrees.length; j++) {
          switch (excludedSubtrees[j].base.type) {
            //#region rfc822Name
            case 1:
              if (subjectAltNames.length >= 0) {
                for (let k = 0; k < subjectAltNames.length; k++) {
                  if (subjectAltNames[k].type === 1) // rfc822Name
                    excluded = excluded || compareRFC822Name(subjectAltNames[k].value, excludedSubtrees[j].base.value);
                }
              }
              else // Try to find out "emailAddress" inside "subject"
              {
                for (let k = 0; k < cert.subject.typesAndValues.length; k++) {
                  if ((cert.subject.typesAndValues[k].type === "1.2.840.113549.1.9.1") ||    // PKCS#9 e-mail address
                    (cert.subject.typesAndValues[k].type === "0.9.2342.19200300.100.1.3")) // RFC1274 "rfc822Mailbox" e-mail address
                    excluded = excluded || compareRFC822Name(cert.subject.typesAndValues[k].value.valueBlock.value, excludedSubtrees[j].base.value);
                }
              }
              break;
            //#endregion
            //#region dNSName
            case 2:
              if (subjectAltNames.length > 0) {
                for (let k = 0; k < subjectAltNames.length; k++) {
                  if (subjectAltNames[k].type === 2) // dNSName
                    excluded = excluded || compareDNSName(subjectAltNames[k].value, excludedSubtrees[j].base.value);
                }
              }
              break;
            //#endregion
            //#region directoryName
            case 4:
              excluded = excluded || compareDirectoryName(cert.subject, excludedSubtrees[j].base.value);
              break;
            //#endregion
            //#region uniformResourceIdentifier
            case 6:
              if (subjectAltNames.length > 0) {
                for (let k = 0; k < subjectAltNames.length; k++) {
                  if (subjectAltNames[k].type === 6) // uniformResourceIdentifier
                    excluded = excluded || compareUniformResourceIdentifier(subjectAltNames[k].value, excludedSubtrees[j].base.value);
                }
              }
              break;
            //#endregion
            //#region iPAddress
            case 7:
              if (subjectAltNames.length > 0) {
                for (let k = 0; k < subjectAltNames.length; k++) {
                  if (subjectAltNames[k].type === 7) // iPAddress
                    excluded = excluded || compareIPAddress(subjectAltNames[k].value, excludedSubtrees[j].base.value);
                }
              }
              break;
            //#endregion
            //#region default
            default: // No action, but probably here we need to create a warning for "malformed constraint"
            //#endregion
          }

          if (excluded)
            break;
        }

        if (excluded === true) {
          policyResult.result = false;
          policyResult.resultCode = 42;
          policyResult.resultMessage = "Failed to meet \"excluded sub-trees\" name constraint";

          throw policyResult;
        }
        //#endregion

        //#region Append "cert_..._subtrees" to "..._subtrees"
        permittedSubtrees = permittedSubtrees.concat(certPermittedSubtrees);
        excludedSubtrees = excludedSubtrees.concat(certExcludedSubtrees);
        //#endregion
      }
      //#endregion

      return policyResult;
      //#endregion
    } catch (error) {
      if (error instanceof Error) {
        if (error instanceof ChainValidationError) {
          return {
            result: false,
            resultCode: error.code,
            resultMessage: error.message,
            error: error,
          };
        }

        return {
          result: false,
          resultCode: ChainValidationCode.unknown,
          resultMessage: error.message,
          error: error,
        };
      }

      if (error && typeof error === "object" && "resultMessage" in error) {
        return error as CertificateChainValidationEngineVerifyResult;
      }

      return {
        result: false,
        resultCode: -1,
        resultMessage: `${error}`,
      };
    }
  }

}
