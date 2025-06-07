// from https://github.com/tc39/proposal-decorators/blob/8936bfe642c63db0c6565572c4847c120400dac3/README.md

export type ClassMethodDecorator = (value: Function, context: {
  kind: "method";
  name: string | symbol;
  access: { get(): unknown };
  static: boolean;
  private: boolean;
  addInitializer(initializer: () => void): void;
}) => Function | void;

export type ClassGetterDecorator = (value: Function, context: {
  kind: "getter";
  name: string | symbol;
  access: { get(): unknown };
  static: boolean;
  private: boolean;
  addInitializer(initializer: () => void): void;
}) => Function | void;

export type ClassSetterDecorator = (value: Function, context: {
  kind: "setter";
  name: string | symbol;
  access: { set(value: unknown): void };
  static: boolean;
  private: boolean;
  addInitializer(initializer: () => void): void;
}) => Function | void;

export type ClassFieldDecorator = (value: undefined, context: {
  kind: "field";
  name: string | symbol;
  access: { get(): unknown; set(value: unknown): void };
  static: boolean;
  private: boolean;
  addInitializer(initializer: () => void): void;
}) => (initialValue: unknown) => unknown | void;

export type ClassDecorator<T = any> = <
  Class extends new (...args: any) => T = new (...args: any) => T,
>(value: Class, context: ClassDecoratorContext<Class>) => Class | void;

export type ClassAutoAccessorDecorator = (
  value: {
    get: () => unknown;
    set: (value: unknown) => void;
  },
  context: {
    kind: "accessor";
    name: string | symbol;
    access: { get(): unknown; set(value: unknown): void };
    static: boolean;
    private: boolean;
    addInitializer(initializer: () => void): void;
  },
) => {
  get?: () => unknown;
  set?: (value: unknown) => void;
  init?: (initialValue: unknown) => unknown;
} | void;
