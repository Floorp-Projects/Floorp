/// <reference path="../../../../@types/gecko/dom/index.d.ts" />

declare global {
  var gFloorp: {
    tabColor?: {
      setEnable: (value: boolean) => void;
    };
  };
  var gBrowser: any;
  var TabContextMenu: any;
  var SessionStore: any;
}

export {};
