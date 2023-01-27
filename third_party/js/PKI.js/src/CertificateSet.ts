import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { Certificate, CertificateJson } from "./Certificate";
import { AttributeCertificateV1, AttributeCertificateV1Json } from "./AttributeCertificateV1";
import { AttributeCertificateV2, AttributeCertificateV2Json } from "./AttributeCertificateV2";
import { OtherCertificateFormat, OtherCertificateFormatJson } from "./OtherCertificateFormat";
import * as Schema from "./Schema";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { AsnError } from "./errors";
import { EMPTY_STRING } from "./constants";

const CERTIFICATES = "certificates";
const CLEAR_PROPS = [
  CERTIFICATES,
];

export interface ICertificateSet {
  certificates: CertificateSetItem[];
}

export interface CertificateSetJson {
  certificates: CertificateSetItemJson[];
}

export type CertificateSetItemJson = CertificateJson | AttributeCertificateV1Json | AttributeCertificateV2Json | OtherCertificateFormatJson;

export type CertificateSetItem = Certificate | AttributeCertificateV1 | AttributeCertificateV2 | OtherCertificateFormat;

export type CertificateSetParameters = PkiObjectParameters & Partial<ICertificateSet>;

/**
 * Represents the CertificateSet structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class CertificateSet extends PkiObject implements ICertificateSet {

  public static override CLASS_NAME = "CertificateSet";

  public certificates!: CertificateSetItem[];

  /**
   * Initializes a new instance of the {@link CertificateSet} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: CertificateSetParameters = {}) {
    super();

    this.certificates = pvutils.getParametersValue(parameters, CERTIFICATES, CertificateSet.defaultValues(CERTIFICATES));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof CERTIFICATES): CertificateSetItem[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case CERTIFICATES:
        return [];
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * CertificateSet ::= SET OF CertificateChoices
   *
   * CertificateChoices ::= CHOICE {
   *    certificate Certificate,
   *    extendedCertificate [0] IMPLICIT ExtendedCertificate,  -- Obsolete
   *    v1AttrCert [1] IMPLICIT AttributeCertificateV1,        -- Obsolete
   *    v2AttrCert [2] IMPLICIT AttributeCertificateV2,
   *    other [3] IMPLICIT OtherCertificateFormat }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    certificates?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (
      new asn1js.Set({
        name: (names.blockName || EMPTY_STRING),
        value: [
          new asn1js.Repeated({
            name: (names.certificates || CERTIFICATES),
            value: new asn1js.Choice({
              value: [
                Certificate.schema(),
                new asn1js.Constructed({
                  idBlock: {
                    tagClass: 3, // CONTEXT-SPECIFIC
                    tagNumber: 0 // [0]
                  },
                  value: [
                    new asn1js.Any()
                  ]
                }), // JUST A STUB
                new asn1js.Constructed({
                  idBlock: {
                    tagClass: 3, // CONTEXT-SPECIFIC
                    tagNumber: 1 // [1]
                  },
                  value: [
                    new asn1js.Sequence
                  ]
                }),
                new asn1js.Constructed({
                  idBlock: {
                    tagClass: 3, // CONTEXT-SPECIFIC
                    tagNumber: 2 // [2]
                  },
                  value: AttributeCertificateV2.schema().valueBlock.value
                }),
                new asn1js.Constructed({
                  idBlock: {
                    tagClass: 3, // CONTEXT-SPECIFIC
                    tagNumber: 3 // [3]
                  },
                  value: OtherCertificateFormat.schema().valueBlock.value
                })
              ]
            })
          })
        ]
      })
    );
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      CertificateSet.schema()
    );
    AsnError.assertSchema(asn1, this.className);

    //#region Get internal properties from parsed schema
    this.certificates = Array.from(asn1.result.certificates || [], (element: any) => {
      const initialTagNumber = element.idBlock.tagNumber;

      if (element.idBlock.tagClass === 1)
        return new Certificate({ schema: element });

      //#region Making "Sequence" from "Constructed" value
      const elementSequence = new asn1js.Sequence({
        value: element.valueBlock.value
      });
      //#endregion

      switch (initialTagNumber) {
        case 1:
          // WARN: It's possible that CMS contains AttributeCertificateV2 instead of AttributeCertificateV1
          // Check the certificate version
          if ((elementSequence.valueBlock.value[0] as any).valueBlock.value[0].valueBlock.valueDec === 1) {
            return new AttributeCertificateV2({ schema: elementSequence });
          } else {
            return new AttributeCertificateV1({ schema: elementSequence });
          }
        case 2:
          return new AttributeCertificateV2({ schema: elementSequence });
        case 3:
          return new OtherCertificateFormat({ schema: elementSequence });
        case 0:
        default:
      }

      return element;
    });
    //#endregion
  }

  public toSchema(): asn1js.Set {
    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Set({
      value: Array.from(this.certificates, element => {
        switch (true) {
          case (element instanceof Certificate):
            return element.toSchema();
          case (element instanceof AttributeCertificateV1):
            return new asn1js.Constructed({
              idBlock: {
                tagClass: 3,
                tagNumber: 1 // [1]
              },
              value: element.toSchema().valueBlock.value
            });
          case (element instanceof AttributeCertificateV2):
            return new asn1js.Constructed({
              idBlock: {
                tagClass: 3,
                tagNumber: 2 // [2]
              },
              value: element.toSchema().valueBlock.value
            });
          case (element instanceof OtherCertificateFormat):
            return new asn1js.Constructed({
              idBlock: {
                tagClass: 3,
                tagNumber: 3 // [3]
              },
              value: element.toSchema().valueBlock.value
            });
          default:
        }

        return element.toSchema();
      })
    }));
  }

  public toJSON(): CertificateSetJson {
    return {
      certificates: Array.from(this.certificates, o => o.toJSON())
    };
  }

}
