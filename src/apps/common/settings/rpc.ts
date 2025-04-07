export interface NRSettingsParentFunctions {
  // pref
  getBoolPref(prefName: string): Promise<boolean | null>;
  getIntPref(prefName: string): Promise<number | null>;
  getStringPref(prefName: string): Promise<string | null>;
  setBoolPref(prefName: string, prefValue: boolean): Promise<void>;
  setIntPref(prefName: string, prefValue: number): Promise<void>;
  setStringPref(prefName: string, prefValue: string): Promise<void>;
  // settings
  selectFolder(): Promise<string | null>;
  getRandomImageFromFolder(path: string): Promise<string | null>;
}
