/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWinMetroUtils.h"
#include "MetroUtils.h"
#include "nsXULAppAPI.h"
#include "FrameworkView.h"
#include "MetroApp.h"
#include "ToastNotificationHandler.h"

#include <shldisp.h>
#include <shellapi.h>
#include <windows.ui.viewmanagement.h>
#include <windows.ui.startscreen.h>

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::UI::StartScreen;
using namespace ABI::Windows::UI::ViewManagement;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace mozilla::widget::winrt;

namespace mozilla {
namespace widget {
namespace winrt {
extern ComPtr<MetroApp> sMetroApp;
extern nsTArray<nsString>* sSettingsArray;
extern ComPtr<FrameworkView> sFrameworkView;
} } }

namespace mozilla {
namespace widget {

bool nsWinMetroUtils::sUpdatePending = false;

NS_IMPL_ISUPPORTS1(nsWinMetroUtils, nsIWinMetroUtils)

nsWinMetroUtils::nsWinMetroUtils()
{
}

nsWinMetroUtils::~nsWinMetroUtils()
{
}

/**
 * Pins a new tile to the Windows 8 start screen.
 *
 * @param aTileID         An ID which can later be used to remove the tile
 * @param aShortName      A short name for the tile
 * @param aDiplayName     The name that will be displayed on the tile
 * @param aActivationArgs The arguments to pass to the browser upon
 *                        activation of the tile
 * @param aTileImage An image for the normal tile view
 * @param aSmallTileImage An image for the small tile view
 */
NS_IMETHODIMP
nsWinMetroUtils::PinTileAsync(const nsAString &aTileID,
                              const nsAString &aShortName,
                              const nsAString &aDisplayName,
                              const nsAString &aActivationArgs,
                              const nsAString &aTileImage,
                              const nsAString &aSmallTileImage)
{
  if (XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Desktop) {
    NS_WARNING("PinTileAsync can't be called on the desktop.");
    return NS_ERROR_FAILURE;
  }
  HRESULT hr;

  HString logoStr, smallLogoStr, displayName, shortName;

  logoStr.Set(aTileImage.BeginReading());
  smallLogoStr.Set(aSmallTileImage.BeginReading());
  displayName.Set(aDisplayName.BeginReading());
  shortName.Set(aShortName.BeginReading());

  ComPtr<IUriRuntimeClass> logo, smallLogo;
  AssertRetHRESULT(MetroUtils::CreateUri(logoStr, logo), NS_ERROR_FAILURE);
  AssertRetHRESULT(MetroUtils::CreateUri(smallLogoStr, smallLogo), NS_ERROR_FAILURE);

  HString tileActivationArgumentsStr, tileIdStr;
  tileActivationArgumentsStr.Set(aActivationArgs.BeginReading());
  tileIdStr.Set(aTileID.BeginReading());

  ComPtr<ISecondaryTileFactory> tileFactory;
  ComPtr<ISecondaryTile> secondaryTile;
  hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_UI_StartScreen_SecondaryTile).Get(),
                            tileFactory.GetAddressOf());
  AssertRetHRESULT(hr, NS_ERROR_FAILURE);
  hr = tileFactory->CreateWithId(tileIdStr.Get(), secondaryTile.GetAddressOf());
  AssertRetHRESULT(hr, NS_ERROR_FAILURE);

  secondaryTile->put_Logo(logo.Get());
  secondaryTile->put_SmallLogo(smallLogo.Get());
  secondaryTile->put_DisplayName(displayName.Get());
  secondaryTile->put_ShortName(shortName.Get());
  secondaryTile->put_Arguments(tileActivationArgumentsStr.Get());
  secondaryTile->put_TileOptions(TileOptions::TileOptions_ShowNameOnLogo);

  // The tile is created and we can now attempt to pin the tile.
  ComPtr<IAsyncOperationCompletedHandler<bool>> callback(Callback<IAsyncOperationCompletedHandler<bool>>(
    sMetroApp.Get(), &MetroApp::OnAsyncTileCreated));
  ComPtr<IAsyncOperation<bool>> operation;
  AssertRetHRESULT(secondaryTile->RequestCreateAsync(operation.GetAddressOf()), NS_ERROR_FAILURE);
  operation->put_Completed(callback.Get());
  return NS_OK;
}

/**
 * Unpins a tile from the Windows 8 start screen.
 *
 * @param aTileID An existing ID which was previously pinned
 */
NS_IMETHODIMP
nsWinMetroUtils::UnpinTileAsync(const nsAString &aTileID)
{
  if (XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Desktop) {
    NS_WARNING("UnpinTileAsync can't be called on the desktop.");
    return NS_ERROR_FAILURE;
  }
  HRESULT hr;
  HString tileIdStr;
  tileIdStr.Set(aTileID.BeginReading());

  ComPtr<ISecondaryTileFactory> tileFactory;
  ComPtr<ISecondaryTile> secondaryTile;
  hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_UI_StartScreen_SecondaryTile).Get(),
                            tileFactory.GetAddressOf());
  AssertRetHRESULT(hr, NS_ERROR_FAILURE);
  hr = tileFactory->CreateWithId(tileIdStr.Get(), secondaryTile.GetAddressOf());
  AssertRetHRESULT(hr, NS_ERROR_FAILURE);

  // Attempt to unpin the tile
  ComPtr<IAsyncOperationCompletedHandler<bool>> callback(Callback<IAsyncOperationCompletedHandler<bool>>(
    sMetroApp.Get(), &MetroApp::OnAsyncTileCreated));
  ComPtr<IAsyncOperation<bool>> operation;
  AssertRetHRESULT(secondaryTile->RequestDeleteAsync(operation.GetAddressOf()), NS_ERROR_FAILURE);
  operation->put_Completed(callback.Get());
  return NS_OK;
}

/**
 * Determines if a tile is pinned to the Windows 8 start screen.
 *
 * @param aTileID   An ID which may have been pinned with pinTileAsync
 * @param aIsPinned Out parameter for determining if the tile is pinned or not
 */
NS_IMETHODIMP
nsWinMetroUtils::IsTilePinned(const nsAString &aTileID, bool *aIsPinned)
{
  if (XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Desktop) {
    NS_WARNING("IsTilePinned can't be called on the desktop.");
    return NS_ERROR_FAILURE;
  }
  NS_ENSURE_ARG_POINTER(aIsPinned);

  HRESULT hr;
  HString tileIdStr;
  tileIdStr.Set(aTileID.BeginReading());

  ComPtr<ISecondaryTileStatics> tileStatics;
  hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_UI_StartScreen_SecondaryTile).Get(),
                            tileStatics.GetAddressOf());
  AssertRetHRESULT(hr, NS_ERROR_FAILURE);
  boolean result = false;
  tileStatics->Exists(tileIdStr.Get(), &result);
  *aIsPinned = result;
  return NS_OK;
}

/**
 * Launches the specified application with the specified arguments and
 * switches to Desktop mode if in metro mode.
*/
NS_IMETHODIMP
nsWinMetroUtils::LaunchInDesktop(const nsAString &aPath, const nsAString &aArguments)
{
  SHELLEXECUTEINFOW sinfo;
  memset(&sinfo, 0, sizeof(SHELLEXECUTEINFOW));
  sinfo.cbSize       = sizeof(SHELLEXECUTEINFOW);
  // Per the Metro style enabled desktop browser, for some reason,
  // SEE_MASK_FLAG_LOG_USAGE is needed to change from immersive mode
  // to desktop.
  sinfo.fMask        = SEE_MASK_FLAG_LOG_USAGE;
  sinfo.hwnd         = nullptr;
  sinfo.lpFile       = aPath.BeginReading();
  sinfo.lpParameters = aArguments.BeginReading();
  sinfo.lpVerb       = L"open";
  sinfo.nShow        = SW_SHOWNORMAL;

  if (!ShellExecuteEx(&sinfo)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWinMetroUtils::ShowNativeToast(const nsAString &aTitle,
  const nsAString &aMessage, const nsAString &anImage,
  const nsAString &aCookie)
{
  // Firefox is in the foreground, no need for a notification.
  if (::GetActiveWindow() == ::GetForegroundWindow()) {
    return NS_OK;
  }

  ToastNotificationHandler* notification_handler =
      new ToastNotificationHandler;

  HSTRING title = HStringReference(aTitle.BeginReading()).Get();
  HSTRING msg = HStringReference(aMessage.BeginReading()).Get();

  if (anImage.Length() > 0) {
    HSTRING imagePath = HStringReference(anImage.BeginReading()).Get();
    notification_handler->DisplayNotification(title, msg, imagePath, aCookie);
  } else {
    notification_handler->DisplayTextNotification(title, msg, aCookie);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWinMetroUtils::ShowSettingsFlyout()
{
  if (XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Desktop) {
    NS_WARNING("Settings flyout can't be shown on the desktop.");
    return NS_ERROR_FAILURE;
  }

  HRESULT hr = MetroUtils::ShowSettingsFlyout();
  return SUCCEEDED(hr) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWinMetroUtils::GetImmersive(bool *aImersive)
{
  *aImersive =
    XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Metro;
  return NS_OK;
}

NS_IMETHODIMP
nsWinMetroUtils::GetActivationURI(nsAString &aActivationURI)
{
  if (!sFrameworkView) {
    NS_WARNING("GetActivationURI used before view is created!");
    return NS_OK;
  }
  sFrameworkView->GetActivationURI(aActivationURI);
  return NS_OK;
}

NS_IMETHODIMP
nsWinMetroUtils::GetPreviousExecutionState(int32_t *out)
{
  if (!sFrameworkView) {
    NS_WARNING("GetPreviousExecutionState used before view is created!");
    return NS_OK;
  }
  *out = sFrameworkView->GetPreviousExecutionState();
  return NS_OK;
}

NS_IMETHODIMP
nsWinMetroUtils::GetKeyboardVisible(bool *aImersive)
{
  *aImersive = mozilla::widget::winrt::FrameworkView::IsKeyboardVisible();
  return NS_OK;
}

NS_IMETHODIMP
nsWinMetroUtils::GetKeyboardX(uint32_t *aX)
{
  *aX = (uint32_t)floor(mozilla::widget::winrt::FrameworkView::KeyboardVisibleRect().X);
  return NS_OK;
}

NS_IMETHODIMP
nsWinMetroUtils::GetKeyboardY(uint32_t *aY)
{
  *aY = (uint32_t)floor(mozilla::widget::winrt::FrameworkView::KeyboardVisibleRect().Y);
  return NS_OK;
}

NS_IMETHODIMP
nsWinMetroUtils::GetKeyboardWidth(uint32_t *aWidth)
{
  *aWidth = (uint32_t)ceil(mozilla::widget::winrt::FrameworkView::KeyboardVisibleRect().Width);
  return NS_OK;
}

NS_IMETHODIMP
nsWinMetroUtils::GetKeyboardHeight(uint32_t *aHeight)
{
  *aHeight = (uint32_t)ceil(mozilla::widget::winrt::FrameworkView::KeyboardVisibleRect().Height);
  return NS_OK;
}

NS_IMETHODIMP
nsWinMetroUtils::AddSettingsPanelEntry(const nsAString &aLabel, uint32_t *aId)
{
  NS_ENSURE_ARG_POINTER(aId);
  if (!sSettingsArray)
    return NS_ERROR_UNEXPECTED;

  *aId = sSettingsArray->Length();
  sSettingsArray->AppendElement(nsString(aLabel));
  return NS_OK;
}

NS_IMETHODIMP
nsWinMetroUtils::SwapMouseButton(bool aValue, bool *aOriginalValue)
{
  *aOriginalValue = ::SwapMouseButton(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsWinMetroUtils::GetUpdatePending(bool *aUpdatePending)
{
  *aUpdatePending = sUpdatePending;
  return NS_OK;
}

NS_IMETHODIMP
nsWinMetroUtils::SetUpdatePending(bool aUpdatePending)
{
  sUpdatePending = aUpdatePending;
  return NS_OK;
}

} // widget
} // mozilla
