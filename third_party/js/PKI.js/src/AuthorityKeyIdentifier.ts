import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { GeneralName, GeneralNameJson } from "./GeneralName";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const KEY_IDENTIFIER = "keyIdentifier";
const AUTHORITY_CERT_ISSUER = "authorityCertIssuer";
const AUTHORITY_CERT_SERIAL_NUMBER = "authorityCertSerialNumber";
const CLEAR_PROPS = [
  KEY_IDENTIFIER,
  AUTHORITY_CERT_ISSUER,
  AUTHORITY_CERT_SERIAL_NUMBER,
];

export interface IAuthorityKeyIdentifier {
  keyIdentifier?: asn1js.OctetString;
  authorityCertIssuer?: GeneralName[];
  authorityCertSerialNumber?: asn1js.Integer;
}

export type AuthorityKeyIdentifierParameters = PkiObjectParameters & Partial<IAuthorityKeyIdentifier>;

export interface AuthorityKeyIdentifierJson {
  keyIdentifier?: asn1js.OctetStringJson;
  authorityCertIssuer?: GeneralNameJson[];
  authorityCertSerialNumber?: asn1js.IntegerJson;
}

/**
 * Represents the AuthorityKeyIdentifier structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class AuthorityKeyIdentifier extends PkiObject implements IAuthorityKeyIdentifier {

  public static override CLASS_NAME = "AuthorityKeyIdentifier";

  public keyIdentifier?: asn1js.OctetString;
  public authorityCertIssuer?: GeneralName[];
  public authorityCertSerialNumber?: asn1js.Integer;

  /**
   * Initializes a new instance of the {@link AuthorityKeyIdentifier} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: AuthorityKeyIdentifierParameters = {}) {
    super();

    if (KEY_IDENTIFIER in parameters) {
      this.keyIdentifier = pvutils.getParametersValue(parameters, KEY_IDENTIFIER, AuthorityKeyIdentifier.defaultValues(KEY_IDENTIFIER));
    }
    if (AUTHORITY_CERT_ISSUER in parameters) {
      this.authorityCertIssuer = pvutils.getParametersValue(parameters, AUTHORITY_CERT_ISSUER, AuthorityKeyIdentifier.defaultValues(AUTHORITY_CERT_ISSUER));
    }
    if (AUTHORITY_CERT_SERIAL_NUMBER in parameters) {
      this.authorityCertSerialNumber = pvutils.getParametersValue(parameters, AUTHORITY_CERT_SERIAL_NUMBER, AuthorityKeyIdentifier.defaultValues(AUTHORITY_CERT_SERIAL_NUMBER));
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
  public static override defaultValues(memberName: typeof KEY_IDENTIFIER): asn1js.OctetString;
  public static override defaultValues(memberName: typeof AUTHORITY_CERT_ISSUER): GeneralName[];
  public static override defaultValues(memberName: typeof AUTHORITY_CERT_SERIAL_NUMBER): asn1js.Integer;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case KEY_IDENTIFIER:
        return new asn1js.OctetString();
      case AUTHORITY_CERT_ISSUER:
        return [];
      case AUTHORITY_CERT_SERIAL_NUMBER:
        return new asn1js.Integer();
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * AuthorityKeyIdentifier OID ::= 2.5.29.35
   *
   * AuthorityKeyIdentifier ::= SEQUENCE {
   *    keyIdentifier             [0] KeyIdentifier           OPTIONAL,
   *    authorityCertIssuer       [1] GeneralNames            OPTIONAL,
   *    authorityCertSerialNumber [2] CertificateSerialNumber OPTIONAL  }
   *
   * KeyIdentifier ::= OCTET STRING
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    keyIdentifier?: string;
    authorityCertIssuer?: string;
    authorityCertSerialNumber?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Primitive({
          name: (names.keyIdentifier || EMPTY_STRING),
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          }
        }),
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          },
          value: [
            new asn1js.Repeated({
              name: (names.authorityCertIssuer || EMPTY_STRING),
              value: GeneralName.schema()
            })
          ]
        }),
        new asn1js.Primitive({
          name: (names.authorityCertSerialNumber || EMPTY_STRING),
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 2 // [2]
          }
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
      AuthorityKeyIdentifier.schema({
        names: {
          keyIdentifier: KEY_IDENTIFIER,
          authorityCertIssuer: AUTHORITY_CERT_ISSUER,
          authorityCertSerialNumber: AUTHORITY_CERT_SERIAL_NUMBER
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    if (KEY_IDENTIFIER in asn1.result)
      this.keyIdentifier = new asn1js.OctetString({ valueHex: asn1.result.keyIdentifier.valueBlock.valueHex });

    if (AUTHORITY_CERT_ISSUER in asn1.result)
      this.authorityCertIssuer = Array.from(asn1.result.authorityCertIssuer, o => new GeneralName({ schema: o }));

    if (AUTHORITY_CERT_SERIAL_NUMBER in asn1.result)
      this.authorityCertSerialNumber = new asn1js.Integer({ valueHex: asn1.result.authorityCertSerialNumber.valueBlock.valueHex });
    //#endregion
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [];

    if (this.keyIdentifier) {
      outputArray.push(new asn1js.Primitive({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        valueHex: this.keyIdentifier.valueBlock.valueHexView
      }));
    }

    if (this.authorityCertIssuer) {
      outputArray.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 1 // [1]
        },
        value: Array.from(this.authorityCertIssuer, o => o.toSchema())
      }));
    }

    if (this.authorityCertSerialNumber) {
      outputArray.push(new asn1js.Primitive({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 2 // [2]
        },
        valueHex: this.authorityCertSerialNumber.valueBlock.valueHexView
      }));
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): AuthorityKeyIdentifierJson {
    const object: AuthorityKeyIdentifierJson = {};

    if (this.keyIdentifier) {
      object.keyIdentifier = this.keyIdentifier.toJSON();
    }
    if (this.authorityCertIssuer) {
      object.authorityCertIssuer = Array.from(this.authorityCertIssuer, o => o.toJSON());
    }
    if (this.authorityCertSerialNumber) {
      object.authorityCertSerialNumber = this.authorityCertSerialNumber.toJSON();
    }

    return object;
  }

}

