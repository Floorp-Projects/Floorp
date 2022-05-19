cask "safari-technology-preview" do
  if MacOS.version == :monterey
    version "145,012-01341-20220511-067B826E-7921-4625-8D92-E8648B6351C3"
    url "https://secure-appldnld.apple.com/STP/#{version.after_comma}/SafariTechnologyPreview.dmg"
    sha256 "0e986721a7eb5ec91615eda72e7f93a467ba97989e439eafc3592d99b4b6c021"
  elsif MacOS.version == :big_sur
    version "145,012-02933-20220511-B69DC656-4904-4957-BB27-0CD4E65770E0"
    url "https://secure-appldnld.apple.com/STP/#{version.after_comma}/SafariTechnologyPreview.dmg"
    sha256 "2dc800b17529e7ad75f0656921bd24c1acb9659acf17ede62a63486e259e8671"
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
