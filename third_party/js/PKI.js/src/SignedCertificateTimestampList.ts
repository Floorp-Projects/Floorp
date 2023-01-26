import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import * as bs from "bytestreamjs";
import { SignedCertificateTimestamp, SignedCertificateTimestampJson } from "./SignedCertificateTimestamp";
import * as Schema from "./Schema";
import { PkiObject, PkiObjectParameters } from "./PkiObject";

const TIMESTAMPS = "timestamps";

export interface ISignedCertificateTimestampList {
  timestamps: SignedCertificateTimestamp[];
}

export interface SignedCertificateTimestampListJson {
  timestamps: SignedCertificateTimestampJson[];
}

export type SignedCertificateTimestampListParameters = PkiObjectParameters & Partial<ISignedCertificateTimestampList>;

/**
 * Represents the SignedCertificateTimestampList structure described in [RFC6962](https://datatracker.ietf.org/doc/html/rfc6962)
 */
export class SignedCertificateTimestampList extends PkiObject implements ISignedCertificateTimestampList {

  public static override CLASS_NAME = "SignedCertificateTimestampList";

  public timestamps!: SignedCertificateTimestamp[];

  /**
   * Initializes a new instance of the {@link SignedCertificateTimestampList} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: SignedCertificateTimestampListParameters = {}) {
    super();

    this.timestamps = pvutils.getParametersValue(parameters, TIMESTAMPS, SignedCertificateTimestampList.defaultValues(TIMESTAMPS));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof TIMESTAMPS): SignedCertificateTimestamp[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case TIMESTAMPS:
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
      case TIMESTAMPS:
        return (memberValue.length === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * SignedCertificateTimestampList ::= OCTET STRING
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    names.optional ??= false;

    return (new asn1js.OctetString({
      name: (names.blockName || "SignedCertificateTimestampList"),
      optional: names.optional
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    //#region Check the schema is valid
    if ((schema instanceof asn1js.OctetString) === false) {
      throw new Error("Object's schema was not verified against input data for SignedCertificateTimestampList");
    }
    //#endregion
    //#region Get internal properties from parsed schema
    const seqStream = new bs.SeqStream({
      stream: new bs.ByteStream({
        buffer: schema.valueBlock.valueHex
      })
    });

    const dataLength = seqStream.getUint16();
    if (dataLength !== seqStream.length) {
      throw new Error("Object's schema was not verified against input data for SignedCertificateTimestampList");
    }

    while (seqStream.length) {
      this.timestamps.push(new SignedCertificateTimestamp({ stream: seqStream }));
    }
    //#endregion
  }

  public toSchema(): asn1js.OctetString {
    //#region Initial variables
    const stream = new bs.SeqStream();

    let overallLength = 0;

    const timestampsData = [];
    //#endregion
    //#region Get overall length
    for (const timestamp of this.timestamps) {
      const timestampStream = timestamp.toStream();
      timestampsData.push(timestampStream);
      overallLength += timestampStream.stream.buffer.byteLength;
    }
    //#endregion
    stream.appendUint16(overallLength);

    //#region Set data from all timestamps
    for (const timestamp of timestampsData) {
      stream.appendView(timestamp.stream.view);
    }
    //#endregion
    return new asn1js.OctetString({ valueHex: stream.stream.buffer.slice(0) });
  }

  public toJSON(): SignedCertificateTimestampListJson {
    return {
      timestamps: Array.from(this.timestamps, o => o.toJSON())
    };
  }

}
