#ifndef _DIALOGS_H_
#define _DIALOGS_H_

LRESULT CALLBACK  DlgProcMain(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK  DlgProcWelcome(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcLicense(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcSetupType(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcSelectComponents(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcWindowsIntegration(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcProgramFolder(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcStartInstall(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcReboot(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcMessage(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  NewListBoxWndProc( HWND, UINT, WPARAM, LPARAM);

void              AskCancelDlg(HWND hDlg);
void              lbAddItem(HWND hList, siC *siCComponent);
void              InstantiateDialog(DWORD dwDlgID, LPSTR szTitle, WNDPROC wpDlgProc);
void              DlgSequenceNext(void);
void              DlgSequencePrev(void);
BOOL              BrowseForDirectory(HWND hDlg, char *szCurrDir);
LRESULT CALLBACK  BrowseHookProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
void              ShowMessage(LPSTR szMessage, BOOL bShow);
void              DrawCheck(LPDRAWITEMSTRUCT lpdis, HDC hdcMem);
void              InvalidateLBCheckbox(HWND hwndListBox);
void              ProcessWindowsMessages(void);
void              CheckWizardStateCustom(DWORD dwDefault);


#endif
