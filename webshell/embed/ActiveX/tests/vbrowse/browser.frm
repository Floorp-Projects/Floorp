VERSION 5.00
Object = "{1339B53E-3453-11D2-93B9-000000000000}#1.0#0"; "MozillaControl.dll"
Object = "{6B7E6392-850A-101B-AFC0-4210102A8DA7}#1.2#0"; "comctl32.ocx"
Begin VB.Form Form1 
   Caption         =   "Form1"
   ClientHeight    =   5880
   ClientLeft      =   60
   ClientTop       =   345
   ClientWidth     =   7485
   LinkTopic       =   "Form1"
   ScaleHeight     =   5880
   ScaleWidth      =   7485
   StartUpPosition =   3  'Windows Default
   Begin ComctlLib.StatusBar StatusBar1 
      Align           =   2  'Align Bottom
      Height          =   255
      Left            =   0
      TabIndex        =   8
      Top             =   5625
      Width           =   7485
      _ExtentX        =   13203
      _ExtentY        =   450
      SimpleText      =   ""
      _Version        =   327682
      BeginProperty Panels {0713E89E-850A-101B-AFC0-4210102A8DA7} 
         NumPanels       =   1
         BeginProperty Panel1 {0713E89F-850A-101B-AFC0-4210102A8DA7} 
            AutoSize        =   1
            Object.Width           =   12700
            Text            =   ""
            TextSave        =   ""
            Key             =   ""
            Object.Tag             =   ""
            Object.ToolTipText     =   ""
         EndProperty
      EndProperty
   End
   Begin VB.CommandButton btnGo 
      Caption         =   "Go"
      Height          =   375
      Left            =   5280
      TabIndex        =   7
      Top             =   120
      Width           =   735
   End
   Begin VB.ComboBox cmbUrl 
      Height          =   315
      ItemData        =   "browser.frx":0000
      Left            =   3120
      List            =   "browser.frx":0016
      TabIndex        =   6
      Text            =   "http://www.mozilla.com"
      Top             =   120
      Width           =   2055
   End
   Begin MOZILLACONTROLLibCtl.MozillaBrowser Browser1 
      Height          =   4815
      Left            =   0
      OleObjectBlob   =   "browser.frx":00A4
      TabIndex        =   5
      Top             =   600
      Width           =   6975
   End
   Begin VB.CommandButton btnStop 
      Caption         =   "Stop"
      Height          =   375
      Left            =   6120
      TabIndex        =   4
      Top             =   120
      Width           =   735
   End
   Begin VB.CommandButton btnSearch 
      Caption         =   "Search"
      Height          =   375
      Left            =   2280
      TabIndex        =   3
      Top             =   120
      Width           =   735
   End
   Begin VB.CommandButton btnHome 
      Caption         =   "Home"
      Height          =   375
      Left            =   1440
      TabIndex        =   2
      Top             =   120
      Width           =   735
   End
   Begin VB.CommandButton btnForward 
      Caption         =   ">>"
      Height          =   375
      Left            =   720
      TabIndex        =   1
      Top             =   120
      Width           =   495
   End
   Begin VB.CommandButton btnBack 
      Caption         =   "<<"
      Height          =   375
      Left            =   120
      TabIndex        =   0
      Top             =   120
      Width           =   495
   End
   Begin VB.Shape Throbber1 
      FillColor       =   &H0000FF00&
      FillStyle       =   0  'Solid
      Height          =   375
      Left            =   6960
      Top             =   120
      Width           =   375
   End
End
Attribute VB_Name = "Form1"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Private Sub Browser1_BeforeNavigate(ByVal URL As String, ByVal Flags As Long, ByVal TargetFrameName As String, PostData As Variant, ByVal Headers As String, Cancel As Boolean)
    StatusBar1.Panels(1).Text = "Loading " & URL
    Throbber1.FillColor = &HFF&
End Sub

Private Sub Browser1_NavigateComplete(ByVal URL As String)
    StatusBar1.Panels(1).Text = "Loaded " & URL
    Throbber1.FillColor = &HFF00&
End Sub

Private Sub btnBack_Click()
    Browser1.GoBack
End Sub

Private Sub btnForward_Click()
    Browser1.GoForward
End Sub

Private Sub btnGo_Click()
    Browser1.Navigate cmbUrl.Text
End Sub

Private Sub btnHome_Click()
    Browser1.GoHome
End Sub

Private Sub btnSearch_Click()
    Browser1.GoSearch
End Sub

Private Sub btnStop_Click()
    Browser1.Stop
End Sub

Private Sub txtURL_Change()
    Browser1.Navigate txtURL.Text
End Sub

Private Sub Form_Resize()
    Browser1.Width = ScaleWidth
    Browser1.Height = ScaleHeight - Browser1.Top - StatusBar1.Height
End Sub
