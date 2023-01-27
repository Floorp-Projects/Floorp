import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { CertificateSet, CertificateSetJson } from "./CertificateSet";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { RevocationInfoChoices, RevocationInfoChoicesJson } from "./RevocationInfoChoices";
import * as Schema from "./Schema";

const CERTS = "certs";
const CRLS = "crls";
const CLEAR_PROPS = [
  CERTS,
  CRLS,
];

export interface IOriginatorInfo {
  /**
   * Collection of certificates. In may contain originator certificates associated with several different
   * key management algorithms. It may also contain attribute certificates associated with the originator.
   */
  certs?: CertificateSet;
  /**
   * Collection of CRLs. It is intended that the set contain information sufficient to determine whether
   * or not the certificates in the certs field are valid, but such correspondence is not necessary
   */
  crls?: RevocationInfoChoices;
}

export interface OriginatorInfoJson {
  certs?: CertificateSetJson;
  crls?: RevocationInfoChoicesJson;
}

export type OriginatorInfoParameters = PkiObjectParameters & Partial<IOriginatorInfo>;

/**
 * Represents the OriginatorInfo structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class OriginatorInfo extends PkiObject implements IOriginatorInfo {

  public static override CLASS_NAME = "OriginatorInfo";

  public certs?: CertificateSet;
  public crls?: RevocationInfoChoices;

  /**
   * Initializes a new instance of the {@link CertificateSet} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: OriginatorInfoParameters = {}) {
    super();

    this.crls = pvutils.getParametersValue(parameters, CRLS, OriginatorInfo.defaultValues(CRLS));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof CERTS): CertificateSet;
  public static override defaultValues(memberName: typeof CRLS): RevocationInfoChoices;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case CERTS:
        return new CertificateSet();
      case CRLS:
        return new RevocationInfoChoices();
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * Compare values with default values for all class members
   * @param memberName String name for a class member
   * @param memberValue Value to compare with default value
   */
  public static compareWithDefault(memberName: string, memberValue: any): boolean {
    switch (memberName) {
      case CERTS:
        return (memberValue.certificates.length === 0);
      case CRLS:
        return ((memberValue.crls.length === 0) && (memberValue.otherRevocationInfos.length === 0));
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * OriginatorInfo ::= SEQUENCE {
   *    certs [0] IMPLICIT CertificateSet OPTIONAL,
   *    crls [1] IMPLICIT RevocationInfoChoices OPTIONAL }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    certs?: string;
    crls?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Constructed({
          name: (names.certs || EMPTY_STRING),
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: CertificateSet.schema().valueBlock.value
        }),
        new asn1js.Constructed({
          name: (names.crls || EMPTY_STRING),
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          },
          value: RevocationInfoChoices.schema().valueBlock.value
        })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      OriginatorInfo.schema({
        names: {
          certs: CERTS,
          crls: CRLS
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    if (CERTS in asn1.result) {
      this.certs = new CertificateSet({
        schema: new asn1js.Set({
          value: asn1.result.certs.valueBlock.value
        })
      });
    }
    if (CRLS in asn1.result) {
      this.crls = new RevocationInfoChoices({
        schema: new asn1js.Set({
          value: asn1.result.crls.valueBlock.value
        })
      });
    }
  }

  public toSchema(): asn1js.Sequence {
    const sequenceValue = [];

    if (this.certs) {
      sequenceValue.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: this.certs.toSchema().valueBlock.value
      }));
    }

    if (this.crls) {
      sequenceValue.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 1 // [1]
        },
        value: this.crls.toSchema().valueBlock.value
      }));
    }

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: sequenceValue
    }));
    //#endregion
  }

  public toJSON(): OriginatorInfoJson {
    const res: OriginatorInfoJson = {};

    if (this.certs) {
      res.certs = this.certs.toJSON();
    }

    if (this.crls) {
      res.crls = this.crls.toJSON();
    }

    return res;
  }

}
