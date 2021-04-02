cask "safari-technology-preview" do
  if MacOS.version <= :catalina
    version "123,071-23564-20210330-f6b89a7f-4800-4af7-93cb-0dbb005db0e8"
    sha256 "16e7ed118cd3e839d48c7c68827c59fd2f9d2d3341514d7b301fb5d8c30d55fb"
  else
    version "123,071-23628-20210330-8224de12-9cd5-4c33-b944-149b5f4cd543"
    sha256 "9d1319f78bc27cf6163691f61b70f9457a59a125b9adfb44253ac7a597a87767"
  end

  url "https://secure-appldnld.apple.com/STP/#{version.after_comma}/SafariTechnologyPreview.dmg"
  appcast "https://developer.apple.com/safari/download/"
  name "Safari Technology Preview"
  homepage "https://developer.apple.com/safari/download/"

  auto_updates true
  depends_on macos: ">= :catalina"

  pkg "Safari Technology Preview.pkg"

  uninstall delete: "/Applications/Safari Technology Preview.app"

  zap trash: [
    "~/Library/Application Support/com.apple.sharedfilelist/com.apple.LSSharedFileList.ApplicationRecentDocuments/com.apple.safaritechnologypreview.sfl*",
    "~/Library/Caches/com.apple.SafariTechnologyPreview",
    "~/Library/Preferences/com.apple.SafariTechnologyPreview.plist",
    "~/Library/SafariTechnologyPreview",
    "~/Library/Saved Application State/com.apple.SafariTechnologyPreview.savedState",
    "~/Library/SyncedPreferences/com.apple.SafariTechnologyPreview-com.apple.Safari.UserRequests.plist",
    "~/Library/SyncedPreferences/com.apple.SafariTechnologyPreview-com.apple.Safari.WebFeedSubscriptions.plist",
    "~/Library/SyncedPreferences/com.apple.SafariTechnologyPreview.plist",
    "~/Library/WebKit/com.apple.SafariTechnologyPreview",
  ]
end
