/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsPrintDialogX.h"
#include "nsIPrintSettings.h"
#include "nsPrintSettingsX.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsIWebProgressListener.h"
#include "nsIStringBundle.h"
#include "nsIWebBrowserPrint.h"
#include "nsCRT.h"

#import <Cocoa/Cocoa.h>
#include "nsObjCExceptions.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS1(nsPrintDialogServiceX, nsIPrintDialogService)

nsPrintDialogServiceX::nsPrintDialogServiceX()
{
}

nsPrintDialogServiceX::~nsPrintDialogServiceX()
{
}

NS_IMETHODIMP
nsPrintDialogServiceX::Init()
{
  return NS_OK;
}

NS_IMETHODIMP
nsPrintDialogServiceX::Show(nsIDOMWindow *aParent, nsIPrintSettings *aSettings,
                            nsIWebBrowserPrint *aWebBrowserPrint)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_PRECONDITION(aSettings, "aSettings must not be null");

  nsCOMPtr<nsPrintSettingsX> settingsX(do_QueryInterface(aSettings));
  if (!settingsX)
    return NS_ERROR_FAILURE;

  // Set the print job title
  PRUnichar** docTitles;
  PRUint32 titleCount;
  nsresult rv = aWebBrowserPrint->EnumerateDocumentNames(&titleCount, &docTitles);
  if (NS_SUCCEEDED(rv) && titleCount > 0) {
    CFStringRef cfTitleString = CFStringCreateWithCharacters(NULL, docTitles[0], nsCRT::strlen(docTitles[0]));
    if (cfTitleString) {
      ::PMPrintSettingsSetJobName(settingsX->GetPMPrintSettings(), cfTitleString);
      CFRelease(cfTitleString);
    }
    for (PRInt32 i = titleCount - 1; i >= 0; i--) {
      NS_Free(docTitles[i]);
    }
    NS_Free(docTitles);
    docTitles = NULL;
    titleCount = 0;
  }

  NSPrintInfo* printInfo = settingsX->GetCocoaPrintInfo();

  // Put the print info into the current print operation, since that's where
  // [panel runModal] will look for it. We create the view because otherwise
  // we'll get unrelated warnings printed to the console.
  NSView* tmpView = [[NSView alloc] init];
  NSPrintOperation* printOperation = [NSPrintOperation printOperationWithView:tmpView printInfo:printInfo];
  [NSPrintOperation setCurrentOperation:printOperation];

  NSPrintPanel* panel = [NSPrintPanel printPanel];
  PrintPanelAccessoryController* viewController =
    [[PrintPanelAccessoryController alloc] initWithSettings:aSettings];
  [panel addAccessoryController:viewController];
  [viewController release];

  // Show the dialog.
  nsCocoaUtils::PrepareForNativeAppModalDialog();
  int button = [panel runModal];
  nsCocoaUtils::CleanUpAfterNativeAppModalDialog();

  settingsX->SetCocoaPrintInfo([[[NSPrintOperation currentOperation] printInfo] copy]);
  [NSPrintOperation setCurrentOperation:nil];
  [printInfo release];
  [tmpView release];

  if (button != NSOKButton)
    return NS_ERROR_ABORT;

  // Export settings.
  [viewController exportSettings];

  PRInt16 pageRange;
  aSettings->GetPrintRange(&pageRange);
  if (pageRange != nsIPrintSettings::kRangeSelection) {
    PMPrintSettings nativePrintSettings = settingsX->GetPMPrintSettings();
    UInt32 firstPage, lastPage;
    OSStatus status = ::PMGetFirstPage(nativePrintSettings, &firstPage);
    if (status == noErr) {
      status = ::PMGetLastPage(nativePrintSettings, &lastPage);
      if (status == noErr && lastPage != PR_UINT32_MAX) {
        aSettings->SetPrintRange(nsIPrintSettings::kRangeSpecifiedPageRange);
        aSettings->SetStartPageRange(firstPage);
        aSettings->SetEndPageRange(lastPage);
      }
    }
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsPrintDialogServiceX::ShowPageSetup(nsIDOMWindow *aParent,
                                     nsIPrintSettings *aNSSettings)
{
  NS_PRECONDITION(aParent, "aParent must not be null");
  NS_PRECONDITION(aNSSettings, "aSettings must not be null");
  NS_ENSURE_TRUE(aNSSettings, NS_ERROR_FAILURE);

  nsCOMPtr<nsPrintSettingsX> settingsX(do_QueryInterface(aNSSettings));
  if (!settingsX)
    return NS_ERROR_FAILURE;

  NSPrintInfo* printInfo = settingsX->GetCocoaPrintInfo();
  NSPageLayout *pageLayout = [NSPageLayout pageLayout];
  nsCocoaUtils::PrepareForNativeAppModalDialog();
  int button = [pageLayout runModalWithPrintInfo:printInfo];
  nsCocoaUtils::CleanUpAfterNativeAppModalDialog();

  return button == NSOKButton ? NS_OK : NS_ERROR_ABORT;
}

// Accessory view

@interface PrintPanelAccessoryView (Private)

- (NSString*)localizedString:(const char*)aKey;

- (PRInt16)chosenFrameSetting;

- (const char*)headerFooterStringForList:(NSPopUpButton*)aList;

- (void)exportHeaderFooterSettings;

- (void)initBundle;

- (NSTextField*)label:(const char*)aLabel
            withFrame:(NSRect)aRect
            alignment:(NSTextAlignment)aAlignment;

- (void)addLabel:(const char*)aLabel
       withFrame:(NSRect)aRect
       alignment:(NSTextAlignment)aAlignment;

- (void)addLabel:(const char*)aLabel withFrame:(NSRect)aRect;

- (void)addCenteredLabel:(const char*)aLabel withFrame:(NSRect)aRect;

- (NSButton*)checkboxWithLabel:(const char*)aLabel andFrame:(NSRect)aRect;

- (NSPopUpButton*)headerFooterItemListWithFrame:(NSRect)aRect
                                   selectedItem:(const PRUnichar*)aCurrentString;

- (void)addOptionsSection;

- (void)addAppearanceSection;

- (void)addFramesSection;

- (void)addHeaderFooterSection;

- (NSString*)summaryValueForCheckbox:(NSButton*)aCheckbox;

- (NSString*)framesSummaryValue;

- (NSString*)headerSummaryValue;

- (NSString*)footerSummaryValue;

@end

static const char sHeaderFooterTags[][4] =  {"", "&T", "&U", "&D", "&P", "&PT"};

@implementation PrintPanelAccessoryView

// Public methods

- (id)initWithSettings:(nsIPrintSettings*)aSettings
{
  [super initWithFrame:NSMakeRect(0, 0, 540, 270)];

  mSettings = aSettings;
  [self initBundle];
  [self addOptionsSection];
  [self addAppearanceSection];
  [self addFramesSection];
  [self addHeaderFooterSection];

  return self;
}

- (void)exportSettings
{
  mSettings->SetPrintRange([mPrintSelectionOnlyCheckbox state] == NSOnState ?
                             (PRInt16)nsIPrintSettings::kRangeSelection :
                             (PRInt16)nsIPrintSettings::kRangeAllPages);
  mSettings->SetShrinkToFit([mShrinkToFitCheckbox state] == NSOnState);
  mSettings->SetPrintBGColors([mPrintBGColorsCheckbox state] == NSOnState);
  mSettings->SetPrintBGImages([mPrintBGImagesCheckbox state] == NSOnState);
  mSettings->SetPrintFrameType([self chosenFrameSetting]);

  [self exportHeaderFooterSettings];
}

- (void)dealloc
{
  NS_IF_RELEASE(mPrintBundle);
  [super dealloc];
}

// Localization

- (void)initBundle
{
  nsCOMPtr<nsIStringBundleService> bundleSvc = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  bundleSvc->CreateBundle("chrome://global/locale/printdialog.properties", &mPrintBundle);
}

- (NSString*)localizedString:(const char*)aKey
{
  if (!mPrintBundle)
    return @"";

  nsXPIDLString intlString;
  mPrintBundle->GetStringFromName(NS_ConvertUTF8toUTF16(aKey).get(), getter_Copies(intlString));
  NSMutableString* s = [NSMutableString stringWithUTF8String:NS_ConvertUTF16toUTF8(intlString).get()];

  // Remove all underscores (they're used in the GTK dialog for accesskeys).
  [s replaceOccurrencesOfString:@"_" withString:@"" options:0 range:NSMakeRange(0, [s length])];
  return s;
}

// Widget helpers

- (NSTextField*)label:(const char*)aLabel
            withFrame:(NSRect)aRect
            alignment:(NSTextAlignment)aAlignment
{
  NSTextField* label = [[[NSTextField alloc] initWithFrame:aRect] autorelease];
  [label setStringValue:[self localizedString:aLabel]];
  [label setEditable:NO];
  [label setSelectable:NO];
  [label setBezeled:NO];
  [label setBordered:NO];
  [label setDrawsBackground:NO];
  [label setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
  [label setAlignment:aAlignment];
  return label;
}

- (void)addLabel:(const char*)aLabel
       withFrame:(NSRect)aRect
       alignment:(NSTextAlignment)aAlignment
{
  NSTextField* label = [self label:aLabel withFrame:aRect alignment:aAlignment];
  [self addSubview:label];
}

- (void)addLabel:(const char*)aLabel withFrame:(NSRect)aRect
{
  [self addLabel:aLabel withFrame:aRect alignment:NSRightTextAlignment];
}

- (void)addCenteredLabel:(const char*)aLabel withFrame:(NSRect)aRect
{
  [self addLabel:aLabel withFrame:aRect alignment:NSCenterTextAlignment];
}

- (NSButton*)checkboxWithLabel:(const char*)aLabel andFrame:(NSRect)aRect
{
  aRect.origin.y += 4.0f;
  NSButton* checkbox = [[[NSButton alloc] initWithFrame:aRect] autorelease];
  [checkbox setButtonType:NSSwitchButton];
  [checkbox setTitle:[self localizedString:aLabel]];
  [checkbox setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
  [checkbox sizeToFit];
  return checkbox;
}

- (NSPopUpButton*)headerFooterItemListWithFrame:(NSRect)aRect
                                   selectedItem:(const PRUnichar*)aCurrentString
{
  NSPopUpButton* list = [[[NSPopUpButton alloc] initWithFrame:aRect pullsDown:NO] autorelease];
  [list setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
  [[list cell] setControlSize:NSSmallControlSize];
  NSArray* items =
    [NSArray arrayWithObjects:[self localizedString:"headerFooterBlank"],
                              [self localizedString:"headerFooterTitle"],
                              [self localizedString:"headerFooterURL"],
                              [self localizedString:"headerFooterDate"],
                              [self localizedString:"headerFooterPage"],
                              [self localizedString:"headerFooterPageTotal"],
                              nil];
  [list addItemsWithTitles:items];

  NS_ConvertUTF16toUTF8 currentStringUTF8(aCurrentString);
  for (unsigned int i = 0; i < ArrayLength(sHeaderFooterTags); i++) {
    if (!strcmp(currentStringUTF8.get(), sHeaderFooterTags[i])) {
      [list selectItemAtIndex:i];
      break;
    }
  }

  return list;
}

// Build sections

- (void)addOptionsSection
{
  // Title
  [self addLabel:"optionsTitleMac" withFrame:NSMakeRect(0, 240, 151, 22)];

  // "Print Selection Only"
  mPrintSelectionOnlyCheckbox = [self checkboxWithLabel:"selectionOnly"
                                               andFrame:NSMakeRect(156, 240, 0, 0)];

  bool canPrintSelection;
  mSettings->GetPrintOptions(nsIPrintSettings::kEnableSelectionRB,
                             &canPrintSelection);
  [mPrintSelectionOnlyCheckbox setEnabled:canPrintSelection];

  PRInt16 printRange;
  mSettings->GetPrintRange(&printRange);
  if (printRange == nsIPrintSettings::kRangeSelection) {
    [mPrintSelectionOnlyCheckbox setState:NSOnState];
  }

  [self addSubview:mPrintSelectionOnlyCheckbox];

  // "Shrink To Fit"
  mShrinkToFitCheckbox = [self checkboxWithLabel:"shrinkToFit"
                                        andFrame:NSMakeRect(156, 218, 0, 0)];

  bool shrinkToFit;
  mSettings->GetShrinkToFit(&shrinkToFit);
  [mShrinkToFitCheckbox setState:(shrinkToFit ? NSOnState : NSOffState)];

  [self addSubview:mShrinkToFitCheckbox];
}

- (void)addAppearanceSection
{
  // Title
  [self addLabel:"appearanceTitleMac" withFrame:NSMakeRect(0, 188, 151, 22)];

  // "Print Background Colors"
  mPrintBGColorsCheckbox = [self checkboxWithLabel:"printBGColors"
                                          andFrame:NSMakeRect(156, 188, 0, 0)];

  bool geckoBool;
  mSettings->GetPrintBGColors(&geckoBool);
  [mPrintBGColorsCheckbox setState:(geckoBool ? NSOnState : NSOffState)];

  [self addSubview:mPrintBGColorsCheckbox];

  // "Print Background Images"
  mPrintBGImagesCheckbox = [self checkboxWithLabel:"printBGImages"
                                          andFrame:NSMakeRect(156, 166, 0, 0)];

  mSettings->GetPrintBGImages(&geckoBool);
  [mPrintBGImagesCheckbox setState:(geckoBool ? NSOnState : NSOffState)];

  [self addSubview:mPrintBGImagesCheckbox];
}

- (void)addFramesSection
{
  // Title
  [self addLabel:"framesTitleMac" withFrame:NSMakeRect(0, 124, 151, 22)];

  // Radio matrix
  NSButtonCell *radio = [[NSButtonCell alloc] init];
  [radio setButtonType:NSRadioButton];
  [radio setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
  NSMatrix *matrix = [[NSMatrix alloc] initWithFrame:NSMakeRect(156, 81, 400, 66)
                                                mode:NSRadioModeMatrix
                                           prototype:(NSCell*)radio
                                        numberOfRows:3
                                     numberOfColumns:1];
  [radio release];
  [matrix setCellSize:NSMakeSize(400, 21)];
  [self addSubview:matrix];
  [matrix release];
  NSArray *cellArray = [matrix cells];
  mAsLaidOutRadio = [cellArray objectAtIndex:0];
  mSelectedFrameRadio = [cellArray objectAtIndex:1];
  mSeparateFramesRadio = [cellArray objectAtIndex:2];
  [mAsLaidOutRadio setTitle:[self localizedString:"asLaidOut"]];
  [mSelectedFrameRadio setTitle:[self localizedString:"selectedFrame"]];
  [mSeparateFramesRadio setTitle:[self localizedString:"separateFrames"]];

  // Radio enabled state
  PRInt16 frameUIFlag;
  mSettings->GetHowToEnableFrameUI(&frameUIFlag);
  if (frameUIFlag == nsIPrintSettings::kFrameEnableNone) {
    [mAsLaidOutRadio setEnabled:NO];
    [mSelectedFrameRadio setEnabled:NO];
    [mSeparateFramesRadio setEnabled:NO];
  } else if (frameUIFlag == nsIPrintSettings::kFrameEnableAsIsAndEach) {
    [mSelectedFrameRadio setEnabled:NO];
  }

  // Radio values
  PRInt16 printFrameType;
  mSettings->GetPrintFrameType(&printFrameType);
  switch (printFrameType) {
    case nsIPrintSettings::kFramesAsIs:
      [mAsLaidOutRadio setState:NSOnState];
      break;
    case nsIPrintSettings::kSelectedFrame:
      [mSelectedFrameRadio setState:NSOnState];
      break;
    case nsIPrintSettings::kEachFrameSep:
      [mSeparateFramesRadio setState:NSOnState];
      break;
  }
}

- (void)addHeaderFooterSection
{
  // Labels
  [self addLabel:"pageHeadersTitleMac" withFrame:NSMakeRect(0, 44, 151, 22)];
  [self addLabel:"pageFootersTitleMac" withFrame:NSMakeRect(0, 0, 151, 22)];
  [self addCenteredLabel:"left" withFrame:NSMakeRect(156, 22, 100, 22)];
  [self addCenteredLabel:"center" withFrame:NSMakeRect(256, 22, 100, 22)];
  [self addCenteredLabel:"right" withFrame:NSMakeRect(356, 22, 100, 22)];

  // Lists
  nsXPIDLString sel;

  mSettings->GetHeaderStrLeft(getter_Copies(sel));
  mHeaderLeftList = [self headerFooterItemListWithFrame:NSMakeRect(156, 44, 100, 22)
                                           selectedItem:sel];
  [self addSubview:mHeaderLeftList];

  mSettings->GetHeaderStrCenter(getter_Copies(sel));
  mHeaderCenterList = [self headerFooterItemListWithFrame:NSMakeRect(256, 44, 100, 22)
                                             selectedItem:sel];
  [self addSubview:mHeaderCenterList];

  mSettings->GetHeaderStrRight(getter_Copies(sel));
  mHeaderRightList = [self headerFooterItemListWithFrame:NSMakeRect(356, 44, 100, 22)
                                            selectedItem:sel];
  [self addSubview:mHeaderRightList];

  mSettings->GetFooterStrLeft(getter_Copies(sel));
  mFooterLeftList = [self headerFooterItemListWithFrame:NSMakeRect(156, 0, 100, 22)
                                           selectedItem:sel];
  [self addSubview:mFooterLeftList];

  mSettings->GetFooterStrCenter(getter_Copies(sel));
  mFooterCenterList = [self headerFooterItemListWithFrame:NSMakeRect(256, 0, 100, 22)
                                             selectedItem:sel];
  [self addSubview:mFooterCenterList];

  mSettings->GetFooterStrRight(getter_Copies(sel));
  mFooterRightList = [self headerFooterItemListWithFrame:NSMakeRect(356, 0, 100, 22)
                                            selectedItem:sel];
  [self addSubview:mFooterRightList];
}

// Export settings

- (PRInt16)chosenFrameSetting
{
  if ([mAsLaidOutRadio state] == NSOnState)
    return nsIPrintSettings::kFramesAsIs;
  if ([mSelectedFrameRadio state] == NSOnState)
    return nsIPrintSettings::kSelectedFrame;
  if ([mSeparateFramesRadio state] == NSOnState)
    return nsIPrintSettings::kEachFrameSep;
  return nsIPrintSettings::kNoFrames;
}

- (const char*)headerFooterStringForList:(NSPopUpButton*)aList
{
  NSInteger index = [aList indexOfSelectedItem];
  NS_ASSERTION(index < NSInteger(ArrayLength(sHeaderFooterTags)), "Index of dropdown is higher than expected!");
  return sHeaderFooterTags[index];
}

- (void)exportHeaderFooterSettings
{
  const char* headerFooterStr;
  headerFooterStr = [self headerFooterStringForList:mHeaderLeftList];
  mSettings->SetHeaderStrLeft(NS_ConvertUTF8toUTF16(headerFooterStr).get());

  headerFooterStr = [self headerFooterStringForList:mHeaderCenterList];
  mSettings->SetHeaderStrCenter(NS_ConvertUTF8toUTF16(headerFooterStr).get());

  headerFooterStr = [self headerFooterStringForList:mHeaderRightList];
  mSettings->SetHeaderStrRight(NS_ConvertUTF8toUTF16(headerFooterStr).get());

  headerFooterStr = [self headerFooterStringForList:mFooterLeftList];
  mSettings->SetFooterStrLeft(NS_ConvertUTF8toUTF16(headerFooterStr).get());

  headerFooterStr = [self headerFooterStringForList:mFooterCenterList];
  mSettings->SetFooterStrCenter(NS_ConvertUTF8toUTF16(headerFooterStr).get());

  headerFooterStr = [self headerFooterStringForList:mFooterRightList];
  mSettings->SetFooterStrRight(NS_ConvertUTF8toUTF16(headerFooterStr).get());
}

// Summary

- (NSString*)summaryValueForCheckbox:(NSButton*)aCheckbox
{
  if (![aCheckbox isEnabled])
    return [self localizedString:"summaryNAValue"];

  return [aCheckbox state] == NSOnState ?
    [self localizedString:"summaryOnValue"] :
    [self localizedString:"summaryOffValue"];
}

- (NSString*)framesSummaryValue
{
  switch([self chosenFrameSetting]) {
    case nsIPrintSettings::kFramesAsIs:
      return [self localizedString:"asLaidOut"];
    case nsIPrintSettings::kSelectedFrame:
      return [self localizedString:"selectedFrame"];
    case nsIPrintSettings::kEachFrameSep:
      return [self localizedString:"separateFrames"];
  }
  return [self localizedString:"summaryNAValue"];
}

- (NSString*)headerSummaryValue
{
  return [[mHeaderLeftList titleOfSelectedItem] stringByAppendingString:
    [@", " stringByAppendingString:
      [[mHeaderCenterList titleOfSelectedItem] stringByAppendingString:
        [@", " stringByAppendingString:
          [mHeaderRightList titleOfSelectedItem]]]]];
}

- (NSString*)footerSummaryValue
{
  return [[mFooterLeftList titleOfSelectedItem] stringByAppendingString:
    [@", " stringByAppendingString:
      [[mFooterCenterList titleOfSelectedItem] stringByAppendingString:
        [@", " stringByAppendingString:
          [mFooterRightList titleOfSelectedItem]]]]];
}

- (NSArray*)localizedSummaryItems
{
  return [NSArray arrayWithObjects:
    [NSDictionary dictionaryWithObjectsAndKeys:
      [self localizedString:"summaryFramesTitle"], NSPrintPanelAccessorySummaryItemNameKey,
      [self framesSummaryValue], NSPrintPanelAccessorySummaryItemDescriptionKey, nil],
    [NSDictionary dictionaryWithObjectsAndKeys:
      [self localizedString:"summarySelectionOnlyTitle"], NSPrintPanelAccessorySummaryItemNameKey,
      [self summaryValueForCheckbox:mPrintSelectionOnlyCheckbox], NSPrintPanelAccessorySummaryItemDescriptionKey, nil],
    [NSDictionary dictionaryWithObjectsAndKeys:
      [self localizedString:"summaryShrinkToFitTitle"], NSPrintPanelAccessorySummaryItemNameKey,
      [self summaryValueForCheckbox:mShrinkToFitCheckbox], NSPrintPanelAccessorySummaryItemDescriptionKey, nil],
    [NSDictionary dictionaryWithObjectsAndKeys:
      [self localizedString:"summaryPrintBGColorsTitle"], NSPrintPanelAccessorySummaryItemNameKey,
      [self summaryValueForCheckbox:mPrintBGColorsCheckbox], NSPrintPanelAccessorySummaryItemDescriptionKey, nil],
    [NSDictionary dictionaryWithObjectsAndKeys:
      [self localizedString:"summaryPrintBGImagesTitle"], NSPrintPanelAccessorySummaryItemNameKey,
      [self summaryValueForCheckbox:mPrintBGImagesCheckbox], NSPrintPanelAccessorySummaryItemDescriptionKey, nil],
    [NSDictionary dictionaryWithObjectsAndKeys:
      [self localizedString:"summaryHeaderTitle"], NSPrintPanelAccessorySummaryItemNameKey,
      [self headerSummaryValue], NSPrintPanelAccessorySummaryItemDescriptionKey, nil],
    [NSDictionary dictionaryWithObjectsAndKeys:
      [self localizedString:"summaryFooterTitle"], NSPrintPanelAccessorySummaryItemNameKey,
      [self footerSummaryValue], NSPrintPanelAccessorySummaryItemDescriptionKey, nil],
    nil];
}

@end

// Accessory controller

@implementation PrintPanelAccessoryController

- (id)initWithSettings:(nsIPrintSettings*)aSettings
{
  [super initWithNibName:nil bundle:nil];

  NSView* accView = [[PrintPanelAccessoryView alloc] initWithSettings:aSettings];
  [self setView:accView];
  [accView release];
  return self;
}

- (void)exportSettings
{
  return [(PrintPanelAccessoryView*)[self view] exportSettings];
}

- (NSArray *)localizedSummaryItems
{
  return [(PrintPanelAccessoryView*)[self view] localizedSummaryItems];
}

@end
