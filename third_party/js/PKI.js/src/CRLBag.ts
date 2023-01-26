import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { CertificateRevocationList } from "./CertificateRevocationList";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { id_CRLBag_X509CRL } from "./ObjectIdentifiers";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const CRL_ID = "crlId";
const CRL_VALUE = "crlValue";
const PARSED_VALUE = "parsedValue";
const CLEAR_PROPS = [
  CRL_ID,
  CRL_VALUE,
];

export interface ICRLBag {
  crlId: string;
  crlValue: any;
  parsedValue?: any;
  certValue?: any;
}

export interface CRLBagJson {
  crlId: string;
  crlValue: any;
}

export type CRLBagParameters = PkiObjectParameters & Partial<ICRLBag>;

/**
 * Represents the CRLBag structure described in [RFC7292](https://datatracker.ietf.org/doc/html/rfc7292)
 */
export class CRLBag extends PkiObject implements ICRLBag {

  public static override CLASS_NAME = "CRLBag";

  public crlId!: string;
  public crlValue: any;
  public parsedValue?: any;
  public certValue?: any;

  /**
   * Initializes a new instance of the {@link CRLBag} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: CRLBagParameters = {}) {
    super();

    this.crlId = pvutils.getParametersValue(parameters, CRL_ID, CRLBag.defaultValues(CRL_ID));
    this.crlValue = pvutils.getParametersValue(parameters, CRL_VALUE, CRLBag.defaultValues(CRL_VALUE));
    if (PARSED_VALUE in parameters) {
      this.parsedValue = pvutils.getParametersValue(parameters, PARSED_VALUE, CRLBag.defaultValues(PARSED_VALUE));
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
  public static override defaultValues(memberName: typeof CRL_ID): string;
  public static override defaultValues(memberName: typeof CRL_VALUE): any;
  public static override defaultValues(memberName: typeof PARSED_VALUE): any;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case CRL_ID:
        return EMPTY_STRING;
      case CRL_VALUE:
        return (new asn1js.Any());
      case PARSED_VALUE:
        return {};
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
      case CRL_ID:
        return (memberValue === EMPTY_STRING);
      case CRL_VALUE:
        return (memberValue instanceof asn1js.Any);
      case PARSED_VALUE:
        return ((memberValue instanceof Object) && (Object.keys(memberValue).length === 0));
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * CRLBag ::= SEQUENCE {
   *    crlId      BAG-TYPE.&id ({CRLTypes}),
   *    crlValue   [0] EXPLICIT BAG-TYPE.&Type ({CRLTypes}{@crlId})
   *}
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    id?: string;
    value?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.ObjectIdentifier({ name: (names.id || "id") }),
        new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [new asn1js.Any({ name: (names.value || "value") })] // EXPLICIT ANY value
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
      CRLBag.schema({
        names: {
          id: CRL_ID,
          value: CRL_VALUE
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.crlId = asn1.result.crlId.valueBlock.toString();
    this.crlValue = asn1.result.crlValue;

    switch (this.crlId) {
      case id_CRLBag_X509CRL: // x509CRL
        {
          this.parsedValue = CertificateRevocationList.fromBER(this.certValue.valueBlock.valueHex);
        }
        break;
      default:
        throw new Error(`Incorrect CRL_ID value in CRLBag: ${this.crlId}`);
    }
  }

  public toSchema(): asn1js.Sequence {
    // Construct and return new ASN.1 schema for this object
    if (this.parsedValue) {
      this.crlId = id_CRLBag_X509CRL;
      this.crlValue = new asn1js.OctetString({ valueHex: this.parsedValue.toSchema().toBER(false) });
    }

    return (new asn1js.Sequence({
      value: [
        new asn1js.ObjectIdentifier({ value: this.crlId }),
        new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [this.crlValue.toSchema()]
        })
      ]
    }));
  }

  public toJSON(): CRLBagJson {
    return {
      crlId: this.crlId,
      crlValue: this.crlValue.toJSON()
    };
  }

}
