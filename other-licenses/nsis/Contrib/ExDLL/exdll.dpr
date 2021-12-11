{
  NSIS ExDLL example
  (C) 2001 - Peter Windridge

  Fixed and formatted by Brett Dever
  http://editor.nfscheats.com/

  Tested in Delphi 7.0
}

library exdll;

uses Windows;

type
  VarConstants = (
    INST_0,
    INST_1,       // $1
    INST_2,       // $2
    INST_3,       // $3
    INST_4,       // $4
    INST_5,       // $5
    INST_6,       // $6
    INST_7,       // $7
    INST_8,       // $8
    INST_9,       // $9
    INST_R0,      // $R0
    INST_R1,      // $R1
    INST_R2,      // $R2
    INST_R3,      // $R3
    INST_R4,      // $R4
    INST_R5,      // $R5
    INST_R6,      // $R6
    INST_R7,      // $R7
    INST_R8,      // $R8
    INST_R9,      // $R9
    INST_CMDLINE, // $CMDLINE
    INST_INSTDIR, // $INSTDIR
    INST_OUTDIR,  // $OUTDIR
    INST_EXEDIR,  // $EXEDIR
    INST_LANG,    // $LANGUAGE
    __INST_LAST
    );
  TVariableList = INST_0..__INST_LAST;
  pstack_t = ^stack_t;
  stack_t = record
    next: pstack_t;
    text: PChar;
  end;

var
  g_stringsize: integer;
  g_stacktop: ^pstack_t;
  g_variables: PChar;
  g_hwndParent: HWND;

function PopString(): string;
var
  th: pstack_t;
begin
  if integer(g_stacktop^) <> 0 then begin
    th := g_stacktop^;
    Result := PChar(@th.text);
    g_stacktop^ := th.next;
    GlobalFree(HGLOBAL(th));
  end;
end;

procedure PushString(const str: string='');
var
  th: pstack_t;
begin
  if integer(g_stacktop) <> 0 then begin
    th := pstack_t(GlobalAlloc(GPTR, SizeOf(stack_t) + g_stringsize));
    lstrcpyn(@th.text, PChar(str), g_stringsize);
    th.next := g_stacktop^;
    g_stacktop^ := th;
  end;
end;

function GetUserVariable(const varnum: TVariableList): string;
begin
  if (integer(varnum) >= 0) and (integer(varnum) < integer(__INST_LAST)) then
    Result := g_variables + integer(varnum) * g_stringsize
  else
    Result := '';
end;

procedure SetUserVariable(const varnum: TVariableList; const value: string);
begin
  if (value <> '') and (integer(varnum) >= 0) and (integer(varnum) < integer(__INST_LAST)) then
    lstrcpy(g_variables + integer(varnum) * g_stringsize, PChar(value))
end;

procedure NSISDialog(const text, caption: string; const buttons: integer);
begin
  MessageBox(g_hwndParent, PChar(text), PChar(caption), buttons);
end;

procedure ex_dll(const hwndParent: HWND; const string_size: integer; const variables: PChar; const stacktop: pointer); cdecl;
begin
  // setup global variables
  g_stringsize := string_size;
  g_hwndParent := hwndParent;
  g_stacktop := stacktop;
  g_variables := variables;
  // end global variable setup

  NSISDialog(GetUserVariable(INST_0), 'The value of $0', MB_OK);
  NSISDialog(PopString, 'pop', MB_OK);
  PushString('Hello, this is a push');
  SetUserVariable(INST_0, 'This is user var $0');
end;

exports ex_dll;

begin
end.
