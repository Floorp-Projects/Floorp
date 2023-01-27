import * as asn1js from "asn1js";
import * as OID from "./ObjectIdentifiers";
import * as Schema from "./Schema";

export type ExtensionParsedValue = (Schema.SchemaCompatible & {
  parsingError?: string;
}) | Schema.SchemaType;

export interface ExtensionValueType {
  name: string;
  type: ExtensionValueConstructor;
}

export interface ExtensionValueConstructor {
  new(params?: { schema: any; }): Schema.SchemaCompatible;
}

export class ExtensionValueFactory {

  public static types?: Record<string, ExtensionValueType>;

  private static getItems(): Record<string, ExtensionValueType> {
    if (!this.types) {
      this.types = {};

      // Register wellknown extensions
      ExtensionValueFactory.register(OID.id_SubjectAltName, "SubjectAltName", AltName);
      ExtensionValueFactory.register(OID.id_IssuerAltName, "IssuerAltName", AltName);
      ExtensionValueFactory.register(OID.id_AuthorityKeyIdentifier, "AuthorityKeyIdentifier", AuthorityKeyIdentifier);
      ExtensionValueFactory.register(OID.id_BasicConstraints, "BasicConstraints", BasicConstraints);
      ExtensionValueFactory.register(OID.id_MicrosoftCaVersion, "MicrosoftCaVersion", CAVersion);
      ExtensionValueFactory.register(OID.id_CertificatePolicies, "CertificatePolicies", CertificatePolicies);
      ExtensionValueFactory.register(OID.id_MicrosoftAppPolicies, "CertificatePoliciesMicrosoft", CertificatePolicies);
      ExtensionValueFactory.register(OID.id_MicrosoftCertTemplateV2, "MicrosoftCertTemplateV2", CertificateTemplate);
      ExtensionValueFactory.register(OID.id_CRLDistributionPoints, "CRLDistributionPoints", CRLDistributionPoints);
      ExtensionValueFactory.register(OID.id_FreshestCRL, "FreshestCRL", CRLDistributionPoints);
      ExtensionValueFactory.register(OID.id_ExtKeyUsage, "ExtKeyUsage", ExtKeyUsage);
      ExtensionValueFactory.register(OID.id_CertificateIssuer, "CertificateIssuer", GeneralNames);
      ExtensionValueFactory.register(OID.id_AuthorityInfoAccess, "AuthorityInfoAccess", InfoAccess);
      ExtensionValueFactory.register(OID.id_SubjectInfoAccess, "SubjectInfoAccess", InfoAccess);
      ExtensionValueFactory.register(OID.id_IssuingDistributionPoint, "IssuingDistributionPoint", IssuingDistributionPoint);
      ExtensionValueFactory.register(OID.id_NameConstraints, "NameConstraints", NameConstraints);
      ExtensionValueFactory.register(OID.id_PolicyConstraints, "PolicyConstraints", PolicyConstraints);
      ExtensionValueFactory.register(OID.id_PolicyMappings, "PolicyMappings", PolicyMappings);
      ExtensionValueFactory.register(OID.id_PrivateKeyUsagePeriod, "PrivateKeyUsagePeriod", PrivateKeyUsagePeriod);
      ExtensionValueFactory.register(OID.id_QCStatements, "QCStatements", QCStatements);
      ExtensionValueFactory.register(OID.id_SignedCertificateTimestampList, "SignedCertificateTimestampList", SignedCertificateTimestampList);
      ExtensionValueFactory.register(OID.id_SubjectDirectoryAttributes, "SubjectDirectoryAttributes", SubjectDirectoryAttributes);
    }

    return this.types;
  }

  public static fromBER(id: string, raw: BufferSource): ExtensionParsedValue | null {
    const asn1 = asn1js.fromBER(raw);
    if (asn1.offset === -1) {
      return null;
    }

    const item = this.find(id);
    if (item) {
      try {
        return new item.type({ schema: asn1.result });
      } catch (ex) {
        const res: ExtensionParsedValue = new item.type();
        res.parsingError = `Incorrectly formatted value of extension ${item.name} (${id})`;

        return res;
      }
    }

    return asn1.result;
  }

  public static find(id: string): ExtensionValueType | null {
    const types = this.getItems();

    return types[id] || null;
  }

  public static register(id: string, name: string, type: ExtensionValueConstructor) {
    this.getItems()[id] = { name, type };
  }

}

import { AltName } from "./AltName";
import { AuthorityKeyIdentifier } from "./AuthorityKeyIdentifier";
import { BasicConstraints } from "./BasicConstraints";
import { CAVersion } from "./CAVersion";
import { CertificatePolicies } from "./CertificatePolicies";
import { CertificateTemplate } from "./CertificateTemplate";
import { CRLDistributionPoints } from "./CRLDistributionPoints";
import { ExtKeyUsage } from "./ExtKeyUsage";
import { GeneralNames } from "./GeneralNames";
import { InfoAccess } from "./InfoAccess";
import { IssuingDistributionPoint } from "./IssuingDistributionPoint";
import { NameConstraints } from "./NameConstraints";
import { PolicyConstraints } from "./PolicyConstraints";
import { PolicyMappings } from "./PolicyMappings";
import { PrivateKeyUsagePeriod } from "./PrivateKeyUsagePeriod";
import { QCStatements } from "./QCStatements";
import { SignedCertificateTimestampList } from "./SignedCertificateTimestampList";
import { SubjectDirectoryAttributes } from "./SubjectDirectoryAttributes";


