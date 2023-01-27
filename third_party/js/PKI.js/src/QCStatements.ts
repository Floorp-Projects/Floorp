import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const ID = "id";
const TYPE = "type";
const VALUES = "values";
const QC_STATEMENT_CLEAR_PROPS = [
  ID,
  TYPE
];
const QC_STATEMENTS_CLEAR_PROPS = [
  VALUES
];

export interface IQCStatement {
  id: string;
  type?: any;
}

export interface QCStatementJson {
  id: string;
  type?: any;
}

export type QCStatementParameters = PkiObjectParameters & Partial<IQCStatement>;

export type QCStatementSchema = Schema.SchemaParameters<{
  id?: string;
  type?: string;
}>;

/**
 * Represents the QCStatement structure described in [RFC3739](https://datatracker.ietf.org/doc/html/rfc3739)
 */
export class QCStatement extends PkiObject implements IQCStatement {

  public static override CLASS_NAME = "QCStatement";

  public id!: string;
  public type?: any;

  /**
   * Initializes a new instance of the {@link QCStatement} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: QCStatementParameters = {}) {
    super();

    this.id = pvutils.getParametersValue(parameters, ID, QCStatement.defaultValues(ID));
    if (TYPE in parameters) {
      this.type = pvutils.getParametersValue(parameters, TYPE, QCStatement.defaultValues(TYPE));
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
  public static override defaultValues(memberName: typeof ID): string;
  public static override defaultValues(memberName: typeof TYPE): any;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case ID:
        return EMPTY_STRING;
      case TYPE:
        return new asn1js.Null();
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
      case ID:
        return (memberValue === EMPTY_STRING);
      case TYPE:
        return (memberValue instanceof asn1js.Null);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * QCStatement ::= SEQUENCE {
   *       statementId   QC-STATEMENT.&id({SupportedStatements}),
   *       statementInfo QC-STATEMENT.&Type({SupportedStatements}{@statementId}) OPTIONAL
   *   }
   *```
   */
  public static override schema(parameters: QCStatementSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.ObjectIdentifier({ name: (names.id || EMPTY_STRING) }),
        new asn1js.Any({
          name: (names.type || EMPTY_STRING),
          optional: true
        })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, QC_STATEMENT_CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      QCStatement.schema({
        names: {
          id: ID,
          type: TYPE
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.id = asn1.result.id.valueBlock.toString();
    if (TYPE in asn1.result)
      this.type = asn1.result.type;
  }

  public toSchema(): asn1js.Sequence {
    const value = [
      new asn1js.ObjectIdentifier({ value: this.id })
    ];

    if (TYPE in this)
      value.push(this.type);

    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value,
    }));
  }

  public toJSON(): QCStatementJson {
    const object: any = {
      id: this.id
    };

    if (this.type) {
      object.type = this.type.toJSON();
    }

    return object;
  }

}

export interface IQCStatements {
  values: QCStatement[];
}

export interface QCStatementsJson {
  values: QCStatementJson[];
}

export type QCStatementsParameters = PkiObjectParameters & Partial<IQCStatements>;

/**
 * Represents the QCStatements structure described in [RFC3739](https://datatracker.ietf.org/doc/html/rfc3739)
 */
export class QCStatements extends PkiObject implements IQCStatements {

  public static override CLASS_NAME = "QCStatements";

  public values!: QCStatement[];

  /**
   * Initializes a new instance of the {@link QCStatement} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: QCStatementParameters = {}) {
    super();

    this.values = pvutils.getParametersValue(parameters, VALUES, QCStatements.defaultValues(VALUES));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof VALUES): QCStatement[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case VALUES:
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
      case VALUES:
        return (memberValue.length === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * QCStatements ::= SEQUENCE OF QCStatement
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    values?: string;
    value?: QCStatementSchema;
  }> = {}): Schema.SchemaType {
    /**
     * @type {Object}
     * @property {string} [blockName]
     * @property {string} [values]
     */
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Repeated({
          name: (names.values || EMPTY_STRING),
          value: QCStatement.schema(names.value || {})
        }),
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, QC_STATEMENTS_CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      QCStatements.schema({
        names: {
          values: VALUES
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.values = Array.from(asn1.result.values, element => new QCStatement({ schema: element }));
  }

  public toSchema(): asn1js.Sequence {
    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: Array.from(this.values, o => o.toSchema())
    }));
  }

  public toJSON(): QCStatementsJson {
    return {
      values: Array.from(this.values, o => o.toJSON())
    };
  }

}
