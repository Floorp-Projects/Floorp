VERSION 5.00
Object = "{1339B53E-3453-11D2-93B9-000000000000}#1.0#0"; "MozillaControl.dll"
Object = "{6B7E6392-850A-101B-AFC0-4210102A8DA7}#1.2#0"; "comctl32.ocx"
Begin VB.Form Form1 
   Caption         =   "Form1"
   ClientHeight    =   5880
   ClientLeft      =   60
   ClientTop       =   345
   ClientWidth     =   7710
   LinkTopic       =   "Form1"
   ScaleHeight     =   392
   ScaleMode       =   3  'Pixel
   ScaleWidth      =   514
   Begin ComctlLib.Toolbar Toolbar1 
      Align           =   1  'Align Top
      Height          =   660
      Left            =   0
      TabIndex        =   0
      Top             =   0
      Width           =   7710
      _ExtentX        =   13600
      _ExtentY        =   1164
      ButtonWidth     =   1032
      ButtonHeight    =   1005
      AllowCustomize  =   0   'False
      Appearance      =   1
      ImageList       =   "ImageList1"
      _Version        =   327682
      BeginProperty Buttons {0713E452-850A-101B-AFC0-4210102A8DA7} 
         NumButtons      =   10
         BeginProperty Button1 {0713F354-850A-101B-AFC0-4210102A8DA7} 
            Caption         =   ""
            Key             =   "goback"
            Description     =   ""
            Object.ToolTipText     =   "Go Back"
            Object.Tag             =   ""
            ImageIndex      =   1
         EndProperty
         BeginProperty Button2 {0713F354-850A-101B-AFC0-4210102A8DA7} 
            Caption         =   ""
            Key             =   "goforward"
            Description     =   ""
            Object.ToolTipText     =   "Go Forward"
            Object.Tag             =   ""
            ImageIndex      =   5
         EndProperty
         BeginProperty Button3 {0713F354-850A-101B-AFC0-4210102A8DA7} 
            Caption         =   ""
            Key             =   "reload"
            Description     =   ""
            Object.ToolTipText     =   "Reload Page"
            Object.Tag             =   ""
            ImageIndex      =   6
         EndProperty
         BeginProperty Button4 {0713F354-850A-101B-AFC0-4210102A8DA7} 
            Key             =   ""
            Object.Tag             =   ""
            Style           =   3
            MixedState      =   -1  'True
         EndProperty
         BeginProperty Button5 {0713F354-850A-101B-AFC0-4210102A8DA7} 
            Caption         =   ""
            Key             =   "gohome"
            Description     =   ""
            Object.ToolTipText     =   "Go Home"
            Object.Tag             =   ""
            ImageIndex      =   3
         EndProperty
         BeginProperty Button6 {0713F354-850A-101B-AFC0-4210102A8DA7} 
            Caption         =   ""
            Key             =   "gosearch"
            Description     =   ""
            Object.ToolTipText     =   "Search Web"
            Object.Tag             =   ""
            ImageIndex      =   8
         EndProperty
         BeginProperty Button7 {0713F354-850A-101B-AFC0-4210102A8DA7} 
            Enabled         =   0   'False
            Key             =   "ph"
            Object.Tag             =   ""
            Style           =   4
            Object.Width           =   220
            MixedState      =   -1  'True
         EndProperty
         BeginProperty Button8 {0713F354-850A-101B-AFC0-4210102A8DA7} 
            Caption         =   ""
            Key             =   "loadpage"
            Description     =   ""
            Object.ToolTipText     =   "Load this URL"
            Object.Tag             =   ""
            ImageIndex      =   4
         EndProperty
         BeginProperty Button9 {0713F354-850A-101B-AFC0-4210102A8DA7} 
            Key             =   ""
            Object.Tag             =   ""
            Style           =   3
            MixedState      =   -1  'True
         EndProperty
         BeginProperty Button10 {0713F354-850A-101B-AFC0-4210102A8DA7} 
            Caption         =   ""
            Key             =   "stop"
            Description     =   ""
            Object.ToolTipText     =   "Stop Loading"
            Object.Tag             =   ""
            ImageIndex      =   2
         EndProperty
      EndProperty
      Begin VB.ComboBox cmbUrl 
         BeginProperty Font 
            Name            =   "MS Sans Serif"
            Size            =   12
            Charset         =   0
            Weight          =   400
            Underline       =   0   'False
            Italic          =   0   'False
            Strikethrough   =   0   'False
         EndProperty
         Height          =   420
         ItemData        =   "browser.frx":0000
         Left            =   3120
         List            =   "browser.frx":0016
         TabIndex        =   3
         Text            =   "http://www.mozilla.com"
         Top             =   120
         Width           =   3015
      End
   End
   Begin MOZILLACONTROLLibCtl.MozillaBrowser Browser1 
      Height          =   4815
      Left            =   0
      OleObjectBlob   =   "browser.frx":00A4
      TabIndex        =   2
      Top             =   720
      Width           =   6855
   End
   Begin ComctlLib.StatusBar StatusBar1 
      Align           =   2  'Align Bottom
      Height          =   255
      Left            =   0
      TabIndex        =   1
      Top             =   5625
      Width           =   7710
      _ExtentX        =   13600
      _ExtentY        =   450
      SimpleText      =   ""
      _Version        =   327682
      BeginProperty Panels {0713E89E-850A-101B-AFC0-4210102A8DA7} 
         NumPanels       =   2
         BeginProperty Panel1 {0713E89F-850A-101B-AFC0-4210102A8DA7} 
            AutoSize        =   1
            Object.Width           =   10954
            MinWidth        =   2646
            TextSave        =   ""
            Key             =   ""
            Object.Tag             =   ""
         EndProperty
         BeginProperty Panel2 {0713E89F-850A-101B-AFC0-4210102A8DA7} 
            Alignment       =   2
            Object.Width           =   2117
            MinWidth        =   2117
            TextSave        =   ""
            Key             =   ""
            Object.Tag             =   ""
         EndProperty
      EndProperty
   End
   Begin ComctlLib.ImageList ImageList1 
      Left            =   6960
      Top             =   360
      _ExtentX        =   1005
      _ExtentY        =   1005
      BackColor       =   -2147483643
      ImageWidth      =   32
      ImageHeight     =   32
      MaskColor       =   12632256
      _Version        =   327682
      BeginProperty Images {0713E8C2-850A-101B-AFC0-4210102A8DA7} 
         NumListImages   =   8
         BeginProperty ListImage1 {0713E8C3-850A-101B-AFC0-4210102A8DA7} 
            Picture         =   "browser.frx":00C8
            Key             =   "back"
         EndProperty
         BeginProperty ListImage2 {0713E8C3-850A-101B-AFC0-4210102A8DA7} 
            Picture         =   "browser.frx":03E2
            Key             =   "stop"
         EndProperty
         BeginProperty ListImage3 {0713E8C3-850A-101B-AFC0-4210102A8DA7} 
            Picture         =   "browser.frx":06FC
            Key             =   "home"
         EndProperty
         BeginProperty ListImage4 {0713E8C3-850A-101B-AFC0-4210102A8DA7} 
            Picture         =   "browser.frx":0A16
            Key             =   "gotopage"
         EndProperty
         BeginProperty ListImage5 {0713E8C3-850A-101B-AFC0-4210102A8DA7} 
            Picture         =   "browser.frx":0D30
            Key             =   "forward"
         EndProperty
         BeginProperty ListImage6 {0713E8C3-850A-101B-AFC0-4210102A8DA7} 
            Picture         =   "browser.frx":104A
            Key             =   "reload"
         EndProperty
         BeginProperty ListImage7 {0713E8C3-850A-101B-AFC0-4210102A8DA7} 
            Picture         =   "browser.frx":1364
            Key             =   "go"
         EndProperty
         BeginProperty ListImage8 {0713E8C3-850A-101B-AFC0-4210102A8DA7} 
            Picture         =   "browser.frx":167E
            Key             =   "gofind"
         EndProperty
      EndProperty
   End
End
Attribute VB_Name = "Form1"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Private Sub Browser1_BeforeNavigate(ByVal URL As String, ByVal Flags As Long, ByVal TargetFrameName As String, PostData As Variant, ByVal Headers As String, Cancel As Boolean)
    StatusBar1.Panels(1).Text = "Loading " & URL
'    Throbber1.FillColor = &HFF&
End Sub

Private Sub Browser1_NavigateComplete(ByVal URL As String)
    StatusBar1.Panels(1).Text = "Loaded " & URL
    StatusBar1.Panels(2).Text = ""
'    Throbber1.FillColor = &HFF00&
End Sub

Private Sub Browser1_ProgressChange(ByVal Progress As Long, ByVal ProgressMax As Long)
    Dim fProgress As Double
    If Progress = 0 Then
'        fProgress = 0
    ElseIf ProgressMax > 0 Then
'        fProgress = (Progress * 100) / ProgressMax
    Else
        ' fProgress = 0#
        Debug.Print "Progress error - Progress = " & Progress & ", ProgressMax = " & ProgressMax
    End If
'    StatusBar1.Panels(2).Text = Int(fProgress) & "%"
End Sub

Private Sub txtURL_Change()
    Browser1.Navigate txtURL.Text
End Sub

Private Sub Form_Resize()
    Browser1.Width = ScaleWidth
    Browser1.Height = ScaleHeight - Browser1.Top - StatusBar1.Height
End Sub

Private Sub Toolbar1_ButtonClick(ByVal Button As ComctlLib.Button)
    Select Case Button.Key
    Case "goback"
        Browser1.GoBack
    Case "goforward"
        Browser1.GoForward
    Case "reload"
        Browser1.Refresh
    Case "gohome"
        Browser1.GoHome
    Case "gosearch"
        Browser1.GoSearch
    Case "loadpage"
        Browser1.Navigate cmbUrl.Text
    Case "stop"
        Browser1.Stop
    Case Else
    End Select
End Sub
