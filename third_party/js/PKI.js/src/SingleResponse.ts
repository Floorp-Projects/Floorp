import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { CertID, CertIDJson, CertIDSchema } from "./CertID";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { Extension, ExtensionJson } from "./Extension";
import { Extensions, ExtensionsSchema } from "./Extensions";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const CERT_ID = "certID";
const CERT_STATUS = "certStatus";
const THIS_UPDATE = "thisUpdate";
const NEXT_UPDATE = "nextUpdate";
const SINGLE_EXTENSIONS = "singleExtensions";
const CLEAR_PROPS = [
  CERT_ID,
  CERT_STATUS,
  THIS_UPDATE,
  NEXT_UPDATE,
  SINGLE_EXTENSIONS,
];

export interface ISingleResponse {
  certID: CertID;
  certStatus: any;
  thisUpdate: Date;
  nextUpdate?: Date;
  singleExtensions?: Extension[];
}

export type SingleResponseParameters = PkiObjectParameters & Partial<ISingleResponse>;

export type SingleResponseSchema = Schema.SchemaParameters<{
  certID?: CertIDSchema;
  certStatus?: string;
  thisUpdate?: string;
  nextUpdate?: string;
  singleExtensions?: ExtensionsSchema;
}>;

export interface SingleResponseJson {
  certID: CertIDJson;
  certStatus: any;
  thisUpdate: Date;
  nextUpdate?: Date;
  singleExtensions?: ExtensionJson[];
}

/**
 * Represents an SingleResponse described in [RFC6960](https://datatracker.ietf.org/doc/html/rfc6960)
 */
export class SingleResponse extends PkiObject implements ISingleResponse {

  public static override CLASS_NAME = "SingleResponse";

  certID!: CertID;
  certStatus: any;
  thisUpdate!: Date;
  nextUpdate?: Date;
  singleExtensions?: Extension[];

  /**
   * Initializes a new instance of the {@link SingleResponse} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: SingleResponseParameters = {}) {
    super();

    this.certID = pvutils.getParametersValue(parameters, CERT_ID, SingleResponse.defaultValues(CERT_ID));
    this.certStatus = pvutils.getParametersValue(parameters, CERT_STATUS, SingleResponse.defaultValues(CERT_STATUS));
    this.thisUpdate = pvutils.getParametersValue(parameters, THIS_UPDATE, SingleResponse.defaultValues(THIS_UPDATE));
    if (NEXT_UPDATE in parameters) {
      this.nextUpdate = pvutils.getParametersValue(parameters, NEXT_UPDATE, SingleResponse.defaultValues(NEXT_UPDATE));
    }
    if (SINGLE_EXTENSIONS in parameters) {
      this.singleExtensions = pvutils.getParametersValue(parameters, SINGLE_EXTENSIONS, SingleResponse.defaultValues(SINGLE_EXTENSIONS));
    }

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof CERT_ID): CertID;
  public static override defaultValues(memberName: typeof CERT_STATUS): any;
  public static override defaultValues(memberName: typeof THIS_UPDATE): Date;
  public static override defaultValues(memberName: typeof NEXT_UPDATE): Date;
  public static override defaultValues(memberName: typeof SINGLE_EXTENSIONS): Extension[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case CERT_ID:
        return new CertID();
      case CERT_STATUS:
        return {};
      case THIS_UPDATE:
      case NEXT_UPDATE:
        return new Date(0, 0, 0);
      case SINGLE_EXTENSIONS:
        return [];
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
      case CERT_ID:
        return ((CertID.compareWithDefault("hashAlgorithm", memberValue.hashAlgorithm)) &&
          (CertID.compareWithDefault("issuerNameHash", memberValue.issuerNameHash)) &&
          (CertID.compareWithDefault("issuerKeyHash", memberValue.issuerKeyHash)) &&
          (CertID.compareWithDefault("serialNumber", memberValue.serialNumber)));
      case CERT_STATUS:
        return (Object.keys(memberValue).length === 0);
      case THIS_UPDATE:
      case NEXT_UPDATE:
        return (memberValue === SingleResponse.defaultValues(memberName as typeof NEXT_UPDATE));
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * SingleResponse ::= SEQUENCE {
   *    certID                       CertID,
   *    certStatus                   CertStatus,
   *    thisUpdate                   GeneralizedTime,
   *    nextUpdate         [0]       EXPLICIT GeneralizedTime OPTIONAL,
   *    singleExtensions   [1]       EXPLICIT Extensions OPTIONAL }
   *
   * CertStatus ::= CHOICE {
   *    good        [0]     IMPLICIT NULL,
   *    revoked     [1]     IMPLICIT RevokedInfo,
   *    unknown     [2]     IMPLICIT UnknownInfo }
   *
   * RevokedInfo ::= SEQUENCE {
   *    revocationTime              GeneralizedTime,
   *    revocationReason    [0]     EXPLICIT CRLReason OPTIONAL }
   *
   * UnknownInfo ::= NULL
   *```
   */
  public static override schema(parameters: SingleResponseSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        CertID.schema(names.certID || {}),
        new asn1js.Choice({
          value: [
            new asn1js.Primitive({
              name: (names.certStatus || EMPTY_STRING),
              idBlock: {
                tagClass: 3, // CONTEXT-SPECIFIC
                tagNumber: 0 // [0]
              },
            }), // IMPLICIT NULL (no "valueBlock")
            new asn1js.Constructed({
              name: (names.certStatus || EMPTY_STRING),
              idBlock: {
                tagClass: 3, // CONTEXT-SPECIFIC
                tagNumber: 1 // [1]
              },
              value: [
                new asn1js.GeneralizedTime(),
                new asn1js.Constructed({
                  optional: true,
                  idBlock: {
                    tagClass: 3, // CONTEXT-SPECIFIC
                    tagNumber: 0 // [0]
                  },
                  value: [new asn1js.Enumerated()]
                })
              ]
            }),
            new asn1js.Primitive({
              name: (names.certStatus || EMPTY_STRING),
              idBlock: {
                tagClass: 3, // CONTEXT-SPECIFIC
                tagNumber: 2 // [2]
              },
              lenBlock: { length: 1 }
            }) // IMPLICIT NULL (no "valueBlock")
          ]
        }),
        new asn1js.GeneralizedTime({ name: (names.thisUpdate || EMPTY_STRING) }),
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [new asn1js.GeneralizedTime({ name: (names.nextUpdate || EMPTY_STRING) })]
        }),
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          },
          value: [Extensions.schema(names.singleExtensions || {})]
        }) // EXPLICIT SEQUENCE value
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      SingleResponse.schema({
        names: {
          certID: {
            names: {
              blockName: CERT_ID
            }
          },
          certStatus: CERT_STATUS,
          thisUpdate: THIS_UPDATE,
          nextUpdate: NEXT_UPDATE,
          singleExtensions: {
            names: {
              blockName:
                SINGLE_EXTENSIONS
            }
          }
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.certID = new CertID({ schema: asn1.result.certID });
    this.certStatus = asn1.result.certStatus;
    this.thisUpdate = asn1.result.thisUpdate.toDate();
    if (NEXT_UPDATE in asn1.result)
      this.nextUpdate = asn1.result.nextUpdate.toDate();
    if (SINGLE_EXTENSIONS in asn1.result)
      this.singleExtensions = Array.from(asn1.result.singleExtensions.valueBlock.value, element => new Extension({ schema: element }));
  }

  public toSchema(): asn1js.Sequence {
    // Create value array for output sequence
    const outputArray = [];

    outputArray.push(this.certID.toSchema());
    outputArray.push(this.certStatus);
    outputArray.push(new asn1js.GeneralizedTime({ valueDate: this.thisUpdate }));
    if (this.nextUpdate) {
      outputArray.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: [new asn1js.GeneralizedTime({ valueDate: this.nextUpdate })]
      }));
    }

    if (this.singleExtensions) {
      outputArray.push(new asn1js.Sequence({
        value: Array.from(this.singleExtensions, o => o.toSchema())
      }));
    }

    return (new asn1js.Sequence({
      value: outputArray
    }));
  }

  public toJSON(): SingleResponseJson {
    const res: SingleResponseJson = {
      certID: this.certID.toJSON(),
      certStatus: this.certStatus.toJSON(),
      thisUpdate: this.thisUpdate
    };

    if (this.nextUpdate) {
      res.nextUpdate = this.nextUpdate;
    }

    if (this.singleExtensions) {
      res.singleExtensions = Array.from(this.singleExtensions, o => o.toJSON());
    }

    return res;
  }

}
