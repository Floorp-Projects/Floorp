interface PrefDatumBase {
  prefType: string;
  prefName: string;
}

type PrefDatumBoolean = PrefDatumBase & {
  prefType: "boolean";
  prefName: string;
};

type PrefDatumInteger = PrefDatumBase & {
  prefType: "integer";
  prefName: string;
};

type PrefDatumString = PrefDatumBase & {
  prefType: "string";
  prefName: string;
};

export type PrefDatum = PrefDatumBoolean | PrefDatumInteger | PrefDatumString;

type PrefDatumBooleanWithValue = PrefDatumBoolean & {
  prefValue: boolean;
};

type PrefDatumIntegerWithValue = PrefDatumInteger & {
  prefValue: number;
};

type PrefDatumStringWithValue = PrefDatumString & {
  prefValue: string;
};

export type PrefDatumWithValue =
  | PrefDatumBooleanWithValue
  | PrefDatumIntegerWithValue
  | PrefDatumStringWithValue;
