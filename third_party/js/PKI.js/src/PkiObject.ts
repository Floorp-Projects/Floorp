/* eslint-disable @typescript-eslint/no-unused-vars */
import * as asn1js from "asn1js";
import * as pvtsutils from "pvtsutils";
import { AsnError } from "./errors";
import * as Schema from "./Schema";

export interface PkiObjectParameters {
  schema?: Schema.SchemaType;
}

interface PkiObjectConstructor<T extends PkiObject = PkiObject> {
  new(params: PkiObjectParameters): T;
  CLASS_NAME: string;
}

export abstract class PkiObject {

  /**
   * Name of the class
   */
  public static CLASS_NAME = "PkiObject";

  /**
   * Returns block name
   * @returns Returns string block name
   */
  public static blockName(): string {
    return this.CLASS_NAME;
  }

  /**
   * Creates PKI object from the raw data
   * @param raw ASN.1 encoded raw data
   * @returns Initialized and filled current class object
   */
  public static fromBER<T extends PkiObject>(this: PkiObjectConstructor<T>, raw: BufferSource): T {
    const asn1 = asn1js.fromBER(raw);
    AsnError.assert(asn1, this.name);

    try {
      return new this({ schema: asn1.result });
    } catch (e) {
      throw new AsnError(`Cannot create '${this.CLASS_NAME}' from ASN.1 object`);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static defaultValues(memberName: string): any {
    throw new Error(`Invalid member name for ${this.CLASS_NAME} class: ${memberName}`);
  }

  /**
   * Returns value of pre-defined ASN.1 schema for current class
   * @param parameters Input parameters for the schema
   * @returns ASN.1 schema object
   */
  public static schema(parameters: Schema.SchemaParameters = {}): Schema.SchemaType {
    throw new Error(`Method '${this.CLASS_NAME}.schema' should be overridden`);
  }

  public get className(): string {
    return (this.constructor as unknown as { CLASS_NAME: string; }).CLASS_NAME;
  }

  /**
   * Converts parsed ASN.1 object into current class
   * @param schema ASN.1 schema
   */
  public abstract fromSchema(schema: Schema.SchemaType): void;

  /**
   * Converts current object to ASN.1 object and sets correct values
   * @param encodeFlag If param equal to `false` then creates schema via decoding stored value. In other case creates schema via assembling from cached parts
   * @returns ASN.1 object
   */
  public abstract toSchema(encodeFlag?: boolean): Schema.SchemaType;

  /**
   * Converts the class to JSON object
   * @returns JSON object
   */
  public abstract toJSON(): any;

  public toString(encoding: "hex" | "base64" | "base64url" = "hex"): string {
    let schema: Schema.SchemaType;

    try {
      schema = this.toSchema();
    } catch {
      schema = this.toSchema(true);
    }

    return pvtsutils.Convert.ToString(schema.toBER(), encoding);
  }

}