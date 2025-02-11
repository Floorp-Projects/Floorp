export interface TSettingsCard {
  title: string;
  description: string;
  icon: string;
  settings: (TStringSetting | TBooleanSetting | TNumberSetting)[];
}

export interface TSetting {
  prefType: "string" | "number" | "boolean";
  settingsType: "input" | "radio" | "select" | "checkbox";

  key: string;
  default: string | number | boolean;

  options?: string[];

  title: string;
  description: string;
}

export interface TStringSetting extends TSetting {
  prefType: "string";
  settingsType: "input" | "radio" | "select";

  key: string;
  default: string;

  options?: string[];

  title: string;
  description: string;
}

export interface TBooleanSetting extends TSetting {
  prefType: "boolean";
  settingsType: "checkbox";

  key: string;
  default: boolean;

  options?: string[];

  title: string;
  description: string;
}

export interface TNumberSetting extends TSetting {
  prefType: "number";
  settingsType: "input";

  key: string;
  default: number;

  options?: string[];

  title: string;
  description: string;
}
