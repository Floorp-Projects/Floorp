import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { RelativeDistinguishedNames } from "./RelativeDistinguishedNames";
import * as Schema from "./Schema";

export const TYPE = "type";
export const VALUE = "value";

//#region Additional asn1js schema elements existing inside GeneralName schema

/**
 * Schema for "builtInStandardAttributes" of "ORAddress"
 * @param parameters
 * @property names
 * @param optional
 * @returns
 */
function builtInStandardAttributes(parameters: Schema.SchemaParameters<{
  country_name?: string;
  administration_domain_name?: string;
  network_address?: string;
  terminal_identifier?: string;
  private_domain_name?: string;
  organization_name?: string;
  numeric_user_identifier?: string;
  personal_name?: string;
  organizational_unit_names?: string;
}> = {}, optional = false) {
  //builtInStandardAttributes ::= Sequence {
  //    country-name                  CountryName OPTIONAL,
  //    administration-domain-name    AdministrationDomainName OPTIONAL,
  //    network-address           [0] IMPLICIT NetworkAddress OPTIONAL,
  //    terminal-identifier       [1] IMPLICIT TerminalIdentifier OPTIONAL,
  //    private-domain-name       [2] PrivateDomainName OPTIONAL,
  //    organization-name         [3] IMPLICIT OrganizationName OPTIONAL,
  //    numeric-user-identifier   [4] IMPLICIT NumericUserIdentifier OPTIONAL,
  //    personal-name             [5] IMPLICIT PersonalName OPTIONAL,
  //    organizational-unit-names [6] IMPLICIT OrganizationalUnitNames OPTIONAL }
  const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

  return (new asn1js.Sequence({
    optional,
    value: [
      new asn1js.Constructed({
        optional: true,
        idBlock: {
          tagClass: 2, // APPLICATION-SPECIFIC
          tagNumber: 1 // [1]
        },
        name: (names.country_name || EMPTY_STRING),
        value: [
          new asn1js.Choice({
            value: [
              new asn1js.NumericString(),
              new asn1js.PrintableString()
            ]
          })
        ]
      }),
      new asn1js.Constructed({
        optional: true,
        idBlock: {
          tagClass: 2, // APPLICATION-SPECIFIC
          tagNumber: 2 // [2]
        },
        name: (names.administration_domain_name || EMPTY_STRING),
        value: [
          new asn1js.Choice({
            value: [
              new asn1js.NumericString(),
              new asn1js.PrintableString()
            ]
          })
        ]
      }),
      new asn1js.Primitive({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        name: (names.network_address || EMPTY_STRING),
        isHexOnly: true
      }),
      new asn1js.Primitive({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 1 // [1]
        },
        name: (names.terminal_identifier || EMPTY_STRING),
        isHexOnly: true
      }),
      new asn1js.Constructed({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 2 // [2]
        },
        name: (names.private_domain_name || EMPTY_STRING),
        value: [
          new asn1js.Choice({
            value: [
              new asn1js.NumericString(),
              new asn1js.PrintableString()
            ]
          })
        ]
      }),
      new asn1js.Primitive({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 3 // [3]
        },
        name: (names.organization_name || EMPTY_STRING),
        isHexOnly: true
      }),
      new asn1js.Primitive({
        optional: true,
        name: (names.numeric_user_identifier || EMPTY_STRING),
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 4 // [4]
        },
        isHexOnly: true
      }),
      new asn1js.Constructed({
        optional: true,
        name: (names.personal_name || EMPTY_STRING),
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 5 // [5]
        },
        value: [
          new asn1js.Primitive({
            idBlock: {
              tagClass: 3, // CONTEXT-SPECIFIC
              tagNumber: 0 // [0]
            },
            isHexOnly: true
          }),
          new asn1js.Primitive({
            optional: true,
            idBlock: {
              tagClass: 3, // CONTEXT-SPECIFIC
              tagNumber: 1 // [1]
            },
            isHexOnly: true
          }),
          new asn1js.Primitive({
            optional: true,
            idBlock: {
              tagClass: 3, // CONTEXT-SPECIFIC
              tagNumber: 2 // [2]
            },
            isHexOnly: true
          }),
          new asn1js.Primitive({
            optional: true,
            idBlock: {
              tagClass: 3, // CONTEXT-SPECIFIC
              tagNumber: 3 // [3]
            },
            isHexOnly: true
          })
        ]
      }),
      new asn1js.Constructed({
        optional: true,
        name: (names.organizational_unit_names || EMPTY_STRING),
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 6 // [6]
        },
        value: [
          new asn1js.Repeated({
            value: new asn1js.PrintableString()
          })
        ]
      })
    ]
  }));
}

/**
 * Schema for "builtInDomainDefinedAttributes" of "ORAddress"
 * @param optional
 */
function builtInDomainDefinedAttributes(optional = false): Schema.SchemaType {
  return (new asn1js.Sequence({
    optional,
    value: [
      new asn1js.PrintableString(),
      new asn1js.PrintableString()
    ]
  }));
}

/**
 * Schema for "builtInDomainDefinedAttributes" of "ORAddress"
 * @param optional
 */
function extensionAttributes(optional = false): Schema.SchemaType {
  return (new asn1js.Set({
    optional,
    value: [
      new asn1js.Primitive({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        isHexOnly: true
      }),
      new asn1js.Constructed({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 1 // [1]
        },
        value: [new asn1js.Any()]
      })
    ]
  }));
}

//#endregion

export interface IGeneralName {
  /**
   * value type - from a tagged value (0 for "otherName", 1 for "rfc822Name" etc.)
   */
  type: number;
  /**
   * ASN.1 object having GeneralName value (type depends on TYPE value)
   */
  value: any;
}

export type GeneralNameParameters = PkiObjectParameters & Partial<{ type: 1 | 2 | 6; value: string; } | { type: 0 | 3 | 4 | 7 | 8; value: any; }>;

export interface GeneralNameSchema {
  names?: {
    blockName?: string;
    directoryName?: object;
    builtInStandardAttributes?: object;
    otherName?: string;
    rfc822Name?: string;
    dNSName?: string;
    x400Address?: string;
    ediPartyName?: string;
    uniformResourceIdentifier?: string;
    iPAddress?: string;
    registeredID?: string;
  };
}

export interface GeneralNameJson {
  type: number;
  value: string;
}

/**
 * Represents the GeneralName structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class GeneralName extends PkiObject implements IGeneralName {

  public static override CLASS_NAME = "GeneralName";

  public type!: number;
  public value: any;

  /**
   * Initializes a new instance of the {@link GeneralName} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: GeneralNameParameters = {}) {
    super();

    this.type = pvutils.getParametersValue(parameters, TYPE, GeneralName.defaultValues(TYPE));
    this.value = pvutils.getParametersValue(parameters, VALUE, GeneralName.defaultValues(VALUE));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof TYPE): number;
  public static override defaultValues(memberName: typeof VALUE): any;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case TYPE:
        return 9;
      case VALUE:
        return {};
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * Compares values with default values for all class members
   * @param memberName String name for a class member
   * @param memberValue Value to compare with default value
   */
  public static compareWithDefault(memberName: string, memberValue: any): boolean {
    switch (memberName) {
      case TYPE:
        return (memberValue === GeneralName.defaultValues(memberName));
      case VALUE:
        return (Object.keys(memberValue).length === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * GeneralName ::= Choice {
   *    otherName                       [0]     OtherName,
   *    rfc822Name                      [1]     IA5String,
   *    dNSName                         [2]     IA5String,
   *    x400Address                     [3]     ORAddress,
   *    directoryName                   [4]     value,
   *    ediPartyName                    [5]     EDIPartyName,
   *    uniformResourceIdentifier       [6]     IA5String,
   *    iPAddress                       [7]     OCTET STRING,
   *    registeredID                    [8]     OBJECT IDENTIFIER }
   *```
   */
  static override schema(parameters: GeneralNameSchema = {}): asn1js.Choice {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Choice({
      value: [
        new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          name: (names.blockName || EMPTY_STRING),
          value: [
            new asn1js.ObjectIdentifier(),
            new asn1js.Constructed({
              idBlock: {
                tagClass: 3, // CONTEXT-SPECIFIC
                tagNumber: 0 // [0]
              },
              value: [new asn1js.Any()]
            })
          ]
        }),
        new asn1js.Primitive({
          name: (names.blockName || EMPTY_STRING),
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          }
        }),
        new asn1js.Primitive({
          name: (names.blockName || EMPTY_STRING),
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 2 // [2]
          }
        }),
        new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 3 // [3]
          },
          name: (names.blockName || EMPTY_STRING),
          value: [
            builtInStandardAttributes((names.builtInStandardAttributes || {}), false),
            builtInDomainDefinedAttributes(true),
            extensionAttributes(true)
          ]
        }),
        new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 4 // [4]
          },
          name: (names.blockName || EMPTY_STRING),
          value: [RelativeDistinguishedNames.schema(names.directoryName || {})]
        }),
        new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 5 // [5]
          },
          name: (names.blockName || EMPTY_STRING),
          value: [
            new asn1js.Constructed({
              optional: true,
              idBlock: {
                tagClass: 3, // CONTEXT-SPECIFIC
                tagNumber: 0 // [0]
              },
              value: [
                new asn1js.Choice({
                  value: [
                    new asn1js.TeletexString(),
                    new asn1js.PrintableString(),
                    new asn1js.UniversalString(),
                    new asn1js.Utf8String(),
                    new asn1js.BmpString()
                  ]
                })
              ]
            }),
            new asn1js.Constructed({
              idBlock: {
                tagClass: 3, // CONTEXT-SPECIFIC
                tagNumber: 1 // [1]
              },
              value: [
                new asn1js.Choice({
                  value: [
                    new asn1js.TeletexString(),
                    new asn1js.PrintableString(),
                    new asn1js.UniversalString(),
                    new asn1js.Utf8String(),
                    new asn1js.BmpString()
                  ]
                })
              ]
            })
          ]
        }),
        new asn1js.Primitive({
          name: (names.blockName || EMPTY_STRING),
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 6 // [6]
          }
        }),
        new asn1js.Primitive({
          name: (names.blockName || EMPTY_STRING),
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 7 // [7]
          }
        }),
        new asn1js.Primitive({
          name: (names.blockName || EMPTY_STRING),
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 8 // [8]
          }
        })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    //#region Clear input data first
    pvutils.clearProps(schema, [
      "blockName",
      "otherName",
      "rfc822Name",
      "dNSName",
      "x400Address",
      "directoryName",
      "ediPartyName",
      "uniformResourceIdentifier",
      "iPAddress",
      "registeredID"
    ]);
    //#endregion

    //#region Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      GeneralName.schema({
        names: {
          blockName: "blockName",
          otherName: "otherName",
          rfc822Name: "rfc822Name",
          dNSName: "dNSName",
          x400Address: "x400Address",
          directoryName: {
            names: {
              blockName: "directoryName"
            }
          },
          ediPartyName: "ediPartyName",
          uniformResourceIdentifier: "uniformResourceIdentifier",
          iPAddress: "iPAddress",
          registeredID: "registeredID"
        }
      })
    );

    AsnError.assertSchema(asn1, this.className);
    //#endregion

    //#region Get internal properties from parsed schema
    this.type = asn1.result.blockName.idBlock.tagNumber;

    switch (this.type) {
      case 0: // otherName
        this.value = asn1.result.blockName;
        break;
      case 1: // rfc822Name + dNSName + uniformResourceIdentifier
      case 2:
      case 6:
        {
          const value = asn1.result.blockName;

          value.idBlock.tagClass = 1; // UNIVERSAL
          value.idBlock.tagNumber = 22; // IA5STRING

          const valueBER = value.toBER(false);

          const asnValue = asn1js.fromBER(valueBER);
          AsnError.assert(asnValue, "GeneralName value");

          this.value = (asnValue.result as asn1js.BaseStringBlock).valueBlock.value;
        }
        break;
      case 3: // x400Address
        this.value = asn1.result.blockName;
        break;
      case 4: // directoryName
        this.value = new RelativeDistinguishedNames({ schema: asn1.result.directoryName });
        break;
      case 5: // ediPartyName
        this.value = asn1.result.ediPartyName;
        break;
      case 7: // iPAddress
        this.value = new asn1js.OctetString({ valueHex: asn1.result.blockName.valueBlock.valueHex });
        break;
      case 8: // registeredID
        {
          const value = asn1.result.blockName;

          value.idBlock.tagClass = 1; // UNIVERSAL
          value.idBlock.tagNumber = 6; // ObjectIdentifier

          const valueBER = value.toBER(false);

          const asnValue = asn1js.fromBER(valueBER);
          AsnError.assert(asnValue, "GeneralName registeredID");
          this.value = asnValue.result.valueBlock.toString(); // Getting a string representation of the ObjectIdentifier
        }
        break;
      default:
    }
    //#endregion
  }

  public toSchema(): asn1js.Constructed | asn1js.IA5String | asn1js.ObjectIdentifier | asn1js.Choice {
    //#region Construct and return new ASN.1 schema for this object
    switch (this.type) {
      case 0:
      case 3:
      case 5:
        return new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: this.type
          },
          value: [
            this.value
          ]
        });
      case 1:
      case 2:
      case 6:
        {
          const value = new asn1js.IA5String({ value: this.value });

          value.idBlock.tagClass = 3;
          value.idBlock.tagNumber = this.type;

          return value;
        }
      case 4:
        return new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 4
          },
          value: [this.value.toSchema()]
        });
      case 7:
        {
          const value = this.value;

          value.idBlock.tagClass = 3;
          value.idBlock.tagNumber = this.type;

          return value;
        }
      case 8:
        {
          const value = new asn1js.ObjectIdentifier({ value: this.value });

          value.idBlock.tagClass = 3;
          value.idBlock.tagNumber = this.type;

          return value;
        }
      default:
        return GeneralName.schema();
    }
    //#endregion
  }

  public toJSON(): GeneralNameJson {
    const _object = {
      type: this.type,
      value: EMPTY_STRING
    } as GeneralNameJson;

    if ((typeof this.value) === "string")
      _object.value = this.value;
    else {
      try {
        _object.value = this.value.toJSON();
      }
      catch (ex) {
        // nothing
      }
    }

    return _object;
  }

}
