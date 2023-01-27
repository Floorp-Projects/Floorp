import { EMPTY_STRING } from "../constants";
import { ArgumentError } from "./ArgumentError";

export class ParameterError extends TypeError {

  public static readonly NAME = "ParameterError";

  public static assert(target: string, params: any, ...fields: string[]): void;
  public static assert(params: any, ...fields: string[]): void;
  public static assert(...args: any[]): void {
    let target: string | null = null;
    let params: any;
    let fields: string[];
    if (typeof args[0] === "string") {
      target = args[0];
      params = args[1];
      fields = args.slice(2);
    } else {
      params = args[0];
      fields = args.slice(1);
    }
    ArgumentError.assert(params, "parameters", "object");
    for (const field of fields) {
      const value = params[field];
      if (value === undefined || value === null) {
        throw new ParameterError(field, target);
      }
    }
  }

  public static assertEmpty(value: unknown, name: string, target?: string): asserts value {
    if (value === undefined || value === null) {
      throw new ParameterError(name, target);
    }
  }

  public override name: typeof ParameterError.NAME = ParameterError.NAME;

  public field: string;
  public target?: string;

  constructor(field: string, target: string | null = null, message?: string) {
    super();

    this.field = field;
    if (target) {
      this.target = target;
    }

    if (message) {
      this.message = message;
    } else {
      this.message = `Absent mandatory parameter '${field}' ${target ? ` in '${target}'` : EMPTY_STRING}`;
    }
  }

}