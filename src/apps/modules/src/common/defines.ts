export interface NRSettingsParentFunctions {
  getBoolPref(prefName:string): boolean|null
  getIntPref(prefName:string): number|null
  getStringPref(prefName:string): string|null

  setBoolPref(prefName:string,prefValue:boolean): void
  setIntPref(prefName:string,prefValue:number): void
  setStringPref(prefName:string,prefValue:string): void
}