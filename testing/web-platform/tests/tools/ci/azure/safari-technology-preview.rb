cask "safari-technology-preview" do
  if MacOS.version == :monterey
    version "141,002-72472-20220303-BD427703-8F4A-4CEA-A299-DD1C734C6568"
    url "https://secure-appldnld.apple.com/STP/#{version.after_comma}/SafariTechnologyPreview.dmg"
    sha256 "8d8aed9bf940972e5ba09fe8b09cfad29bfef3fb6ca010e43a4175191b8bf3f9"
  elsif MacOS.version == :big_sur
    version "141,002-73837-20220303-4C2EB093-1581-44C1-A404-7292FEEDB4D4"
    url "https://secure-appldnld.apple.com/STP/#{version.after_comma}/SafariTechnologyPreview.dmg"
    sha256 "bd14aff0536b861c58a6ee36ff5bb325c6676817f6dbbbf4300917f20e36503a"
  end

  appcast "https://developer.apple.com/safari/download/"
  name "Safari Technology Preview"
  homepage "https://developer.apple.com/safari/download/"

  auto_updates true
  depends_on macos: ">= :big_sur"

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
