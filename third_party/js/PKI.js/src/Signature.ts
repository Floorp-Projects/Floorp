import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "./AlgorithmIdentifier";
import { Certificate, CertificateJson } from "./Certificate";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const SIGNATURE_ALGORITHM = "signatureAlgorithm";
const SIGNATURE = "signature";
const CERTS = "certs";

export interface ISignature {
  signatureAlgorithm: AlgorithmIdentifier;
  signature: asn1js.BitString;
  certs?: Certificate[];
}

export interface SignatureJson {
  signatureAlgorithm: AlgorithmIdentifierJson;
  signature: asn1js.BitStringJson;
  certs?: CertificateJson[];
}

export type SignatureParameters = PkiObjectParameters & Partial<ISignature>;

export type SignatureSchema = Schema.SchemaParameters<{
  signatureAlgorithm?: AlgorithmIdentifierSchema;
  signature?: string;
  certs?: string;
}>;

/**
 * Represents the Signature structure described in [RFC6960](https://datatracker.ietf.org/doc/html/rfc6960)
 */
export class Signature extends PkiObject implements ISignature {

  public static override CLASS_NAME = "Signature";

  public signatureAlgorithm!: AlgorithmIdentifier;
  public signature!: asn1js.BitString;
  public certs?: Certificate[];

  /**
   * Initializes a new instance of the {@link Signature} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: SignatureParameters = {}) {
    super();

    this.signatureAlgorithm = pvutils.getParametersValue(parameters, SIGNATURE_ALGORITHM, Signature.defaultValues(SIGNATURE_ALGORITHM));
    this.signature = pvutils.getParametersValue(parameters, SIGNATURE, Signature.defaultValues(SIGNATURE));
    if (CERTS in parameters) {
      this.certs = pvutils.getParametersValue(parameters, CERTS, Signature.defaultValues(CERTS));
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
  public static override defaultValues(memberName: typeof SIGNATURE_ALGORITHM): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof SIGNATURE): asn1js.BitString;
  public static override defaultValues(memberName: typeof CERTS): Certificate[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case SIGNATURE_ALGORITHM:
        return new AlgorithmIdentifier();
      case SIGNATURE:
        return new asn1js.BitString();
      case CERTS:
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
      case SIGNATURE_ALGORITHM:
        return ((memberValue.algorithmId === EMPTY_STRING) && (("algorithmParams" in memberValue) === false));
      case SIGNATURE:
        return (memberValue.isEqual(Signature.defaultValues(memberName)));
      case CERTS:
        return (memberValue.length === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * Signature ::= SEQUENCE {
   *    signatureAlgorithm      AlgorithmIdentifier,
   *    signature               BIT STRING,
   *    certs               [0] EXPLICIT SEQUENCE OF Certificate OPTIONAL }
   *```
   */
  public static override schema(parameters: SignatureSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        AlgorithmIdentifier.schema(names.signatureAlgorithm || {}),
        new asn1js.BitString({ name: (names.signature || EMPTY_STRING) }),
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [
            new asn1js.Sequence({
              value: [new asn1js.Repeated({
                name: (names.certs || EMPTY_STRING),
                // TODO Double check
                // value: Certificate.schema(names.certs || {})
                value: Certificate.schema({})
              })]
            })
          ]
        })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, [
      SIGNATURE_ALGORITHM,
      SIGNATURE,
      CERTS
    ]);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      Signature.schema({
        names: {
          signatureAlgorithm: {
            names: {
              blockName: SIGNATURE_ALGORITHM
            }
          },
          signature: SIGNATURE,
          certs: CERTS
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.signatureAlgorithm = new AlgorithmIdentifier({ schema: asn1.result.signatureAlgorithm });
    this.signature = asn1.result.signature;
    if (CERTS in asn1.result)
      this.certs = Array.from(asn1.result.certs, element => new Certificate({ schema: element }));
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array of output sequence
    const outputArray = [];

    outputArray.push(this.signatureAlgorithm.toSchema());
    outputArray.push(this.signature);

    if (this.certs) {
      outputArray.push(new asn1js.Constructed({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: [
          new asn1js.Sequence({
            value: Array.from(this.certs, o => o.toSchema())
          })
        ]
      }));
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): SignatureJson {
    const res: SignatureJson = {
      signatureAlgorithm: this.signatureAlgorithm.toJSON(),
      signature: this.signature.toJSON(),
    };

    if (this.certs) {
      res.certs = Array.from(this.certs, o => o.toJSON());
    }

    return res;
  }

}
