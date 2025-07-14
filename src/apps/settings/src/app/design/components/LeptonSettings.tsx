import { useEffect, useState } from "react";
import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Switch } from "@/components/common/switch.tsx";
import { useTranslation } from "react-i18next";
import { useNavigate } from "react-router-dom";
import {
  ExternalLink,
  Eye,
  EyeOff,
  LayoutGrid,
  Navigation,
  Settings,
  Sidebar,
} from "lucide-react";
import {
  getLeptonSettings,
  type LeptonFormData,
  saveLeptonSettings,
} from "../dataManager.ts";

interface LeptonSettingsProps {
  onClose?: () => void;
}

export function LeptonSettings({ onClose }: LeptonSettingsProps) {
  const { t } = useTranslation();
  const navigate = useNavigate();
  const [settings, setSettings] = useState<LeptonFormData>({
    autohideTab: false,
    autohideNavbar: false,
    autohideSidebar: false,
    autohideBackButton: false,
    autohideForwardButton: false,
    autohidePageAction: false,
    hiddenTabIcon: false,
    hiddenTabbar: false,
    hiddenNavbar: false,
    hiddenSidebarHeader: false,
    hiddenUrlbarIconbox: false,
    hiddenBookmarkbarIcon: false,
    hiddenBookmarkbarLabel: false,
    hiddenDisabledMenu: false,
    iconDisabled: false,
    iconMenu: false,
    centeredTab: false,
    centeredUrlbar: false,
    centeredBookmarkbar: false,
    urlViewMoveIconToLeft: false,
    urlViewGoButtonWhenTyping: false,
    urlViewAlwaysShowPageActions: false,
    tabbarAsTitlebar: false,
    tabbarOneLiner: false,
    sidebarOverlap: false,
  });

  useEffect(() => {
    const loadSettings = async () => {
      try {
        const leptonSettings = await getLeptonSettings();
        setSettings(leptonSettings);
      } catch (error) {
        console.error("Failed to load Lepton settings:", error);
      }
    };
    loadSettings();
  }, []);

  const handleSettingChange = async (
    key: keyof LeptonFormData,
    value: boolean,
  ) => {
    const newSettings = { ...settings, [key]: value };
    setSettings(newSettings);

    try {
      await saveLeptonSettings(newSettings);
    } catch (error) {
      console.error("Failed to save Lepton settings:", error);
    }
  };

  return (
    <div className="p-6 space-y-6">
      <div className="flex items-center justify-between">
        <div className="flex flex-col items-start">
          <h1 className="text-3xl font-bold mb-2">
            {t("design.lepton-preferences.title")}
          </h1>
          <p className="text-sm text-muted-foreground">
            {t("design.lepton-preferences.description")}
          </p>
        </div>
        <button
          onClick={() => onClose ? onClose() : navigate("/features/design")}
          type="button"
          className="btn btn-primary"
        >
          {t("design.lepton-preferences.back")}
        </button>
      </div>

      {/* Experimental Warning */}
      <div className="bg-yellow-50 dark:bg-yellow-900/20 border border-yellow-200 dark:border-yellow-800 rounded-lg p-4">
        <div className="flex flex-col space-y-3">
          <h2 className="text-base font-semibold text-yellow-800 dark:text-yellow-200">
            {t("design.lepton-preferences.experimentalWarning.title")}
          </h2>
          <p className="text-sm text-yellow-700 dark:text-yellow-300">
            {t("design.lepton-preferences.experimentalWarning.description")}
          </p>
          <div className="flex items-center justify-between">
            <span className="text-sm font-medium text-yellow-800 dark:text-yellow-200">
              {t(
                "design.lepton-preferences.experimentalWarning.leptonRepository",
              )}:
            </span>
            <a
              href="https://github.com/black7375/Firefox-UI-Fix"
              target="_blank"
              rel="noreferrer"
              className="inline-flex items-center gap-2 text-sm text-blue-600 dark:text-blue-400 hover:text-blue-800 dark:hover:text-blue-300 underline"
            >
              {t(
                "design.lepton-preferences.experimentalWarning.visitRepository",
              )}
              <ExternalLink className="size-4" />
            </a>
          </div>
        </div>
      </div>

      <div className="space-y-6">
        {/* Auto-hide Settings */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <EyeOff className="size-5" />
              {t("design.lepton-preferences.autohide.title")}
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between">
              <label htmlFor="autohide-tab">
                {t("design.lepton-preferences.autohide.tab")}
              </label>
              <Switch
                id="autohide-tab"
                checked={settings.autohideTab}
                onChange={(e) =>
                  handleSettingChange("autohideTab", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="autohide-navbar">
                {t("design.lepton-preferences.autohide.navbar")}
              </label>
              <Switch
                id="autohide-navbar"
                checked={settings.autohideNavbar}
                onChange={(e) =>
                  handleSettingChange("autohideNavbar", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="autohide-sidebar">
                {t("design.lepton-preferences.autohide.sidebar")}
              </label>
              <Switch
                id="autohide-sidebar"
                checked={settings.autohideSidebar}
                onChange={(e) =>
                  handleSettingChange("autohideSidebar", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="autohide-back-button">
                {t("design.lepton-preferences.autohide.backButton")}
              </label>
              <Switch
                id="autohide-back-button"
                checked={settings.autohideBackButton}
                onChange={(e) =>
                  handleSettingChange("autohideBackButton", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="autohide-forward-button">
                {t("design.lepton-preferences.autohide.forwardButton")}
              </label>
              <Switch
                id="autohide-forward-button"
                checked={settings.autohideForwardButton}
                onChange={(e) =>
                  handleSettingChange(
                    "autohideForwardButton",
                    e.target.checked,
                  )}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="autohide-page-action">
                {t("design.lepton-preferences.autohide.pageAction")}
              </label>
              <Switch
                id="autohide-page-action"
                checked={settings.autohidePageAction}
                onChange={(e) =>
                  handleSettingChange("autohidePageAction", e.target.checked)}
              />
            </div>
          </CardContent>
        </Card>

        {/* Hide Settings */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Eye className="size-5" />
              {t("design.lepton-preferences.hidden.title")}
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between">
              <label htmlFor="hidden-tab-icon">
                {t("design.lepton-preferences.hidden.tabIcon")}
              </label>
              <Switch
                id="hidden-tab-icon"
                checked={settings.hiddenTabIcon}
                onChange={(e) =>
                  handleSettingChange("hiddenTabIcon", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="hidden-tabbar">
                {t("design.lepton-preferences.hidden.tabbar")}
              </label>
              <Switch
                id="hidden-tabbar"
                checked={settings.hiddenTabbar}
                onChange={(e) =>
                  handleSettingChange("hiddenTabbar", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="hidden-navbar">
                {t("design.lepton-preferences.hidden.navbar")}
              </label>
              <Switch
                id="hidden-navbar"
                checked={settings.hiddenNavbar}
                onChange={(e) =>
                  handleSettingChange("hiddenNavbar", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="hidden-sidebar-header">
                {t("design.lepton-preferences.hidden.sidebarHeader")}
              </label>
              <Switch
                id="hidden-sidebar-header"
                checked={settings.hiddenSidebarHeader}
                onChange={(e) =>
                  handleSettingChange("hiddenSidebarHeader", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="hidden-urlbar-iconbox">
                {t("design.lepton-preferences.hidden.urlbarIconbox")}
              </label>
              <Switch
                id="hidden-urlbar-iconbox"
                checked={settings.hiddenUrlbarIconbox}
                onChange={(e) =>
                  handleSettingChange("hiddenUrlbarIconbox", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="hidden-bookmarkbar-icon">
                {t("design.lepton-preferences.hidden.bookmarkbarIcon")}
              </label>
              <Switch
                id="hidden-bookmarkbar-icon"
                checked={settings.hiddenBookmarkbarIcon}
                onChange={(e) =>
                  handleSettingChange(
                    "hiddenBookmarkbarIcon",
                    e.target.checked,
                  )}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="hidden-bookmarkbar-label">
                {t("design.lepton-preferences.hidden.bookmarkbarLabel")}
              </label>
              <Switch
                id="hidden-bookmarkbar-label"
                checked={settings.hiddenBookmarkbarLabel}
                onChange={(e) =>
                  handleSettingChange(
                    "hiddenBookmarkbarLabel",
                    e.target.checked,
                  )}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="hidden-disabled-menu">
                {t("design.lepton-preferences.hidden.disabledMenu")}
              </label>
              <Switch
                id="hidden-disabled-menu"
                checked={settings.hiddenDisabledMenu}
                onChange={(e) =>
                  handleSettingChange("hiddenDisabledMenu", e.target.checked)}
              />
            </div>
          </CardContent>
        </Card>

        {/* Icon Settings */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Settings className="size-5" />
              {t("design.lepton-preferences.icon.title")}
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between">
              <label htmlFor="icon-disabled">
                {t("design.lepton-preferences.icon.disabled")}
              </label>
              <Switch
                id="icon-disabled"
                checked={settings.iconDisabled}
                onChange={(e) =>
                  handleSettingChange("iconDisabled", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="icon-menu">
                {t("design.lepton-preferences.icon.menu")}
              </label>
              <Switch
                id="icon-menu"
                checked={settings.iconMenu}
                onChange={(e) =>
                  handleSettingChange("iconMenu", e.target.checked)}
              />
            </div>
          </CardContent>
        </Card>

        {/* Centered Settings */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Navigation className="size-5" />
              {t("design.lepton-preferences.centered.title")}
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between">
              <label htmlFor="centered-tab">
                {t("design.lepton-preferences.centered.tab")}
              </label>
              <Switch
                id="centered-tab"
                checked={settings.centeredTab}
                onChange={(e) =>
                  handleSettingChange("centeredTab", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="centered-urlbar">
                {t("design.lepton-preferences.centered.urlbar")}
              </label>
              <Switch
                id="centered-urlbar"
                checked={settings.centeredUrlbar}
                onChange={(e) =>
                  handleSettingChange("centeredUrlbar", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="centered-bookmarkbar">
                {t("design.lepton-preferences.centered.bookmarkbar")}
              </label>
              <Switch
                id="centered-bookmarkbar"
                checked={settings.centeredBookmarkbar}
                onChange={(e) =>
                  handleSettingChange("centeredBookmarkbar", e.target.checked)}
              />
            </div>
          </CardContent>
        </Card>

        {/* URL View Settings */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Navigation className="size-5" />
              {t("design.lepton-preferences.urlView.title")}
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between">
              <label htmlFor="url-view-move-icon-to-left">
                {t("design.lepton-preferences.urlView.moveIconToLeft")}
              </label>
              <Switch
                id="url-view-move-icon-to-left"
                checked={settings.urlViewMoveIconToLeft}
                onChange={(e) =>
                  handleSettingChange(
                    "urlViewMoveIconToLeft",
                    e.target.checked,
                  )}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="url-view-go-button-when-typing">
                {t("design.lepton-preferences.urlView.goButtonWhenTyping")}
              </label>
              <Switch
                id="url-view-go-button-when-typing"
                checked={settings.urlViewGoButtonWhenTyping}
                onChange={(e) =>
                  handleSettingChange(
                    "urlViewGoButtonWhenTyping",
                    e.target.checked,
                  )}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="url-view-always-show-page-actions">
                {t("design.lepton-preferences.urlView.alwaysShowPageActions")}
              </label>
              <Switch
                id="url-view-always-show-page-actions"
                checked={settings.urlViewAlwaysShowPageActions}
                onChange={(e) =>
                  handleSettingChange(
                    "urlViewAlwaysShowPageActions",
                    e.target.checked,
                  )}
              />
            </div>
          </CardContent>
        </Card>

        {/* Tab Bar Settings */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <LayoutGrid className="size-5" />
              {t("design.lepton-preferences.tabbar.title")}
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between">
              <label htmlFor="tabbar-as-titlebar">
                {t("design.lepton-preferences.tabbar.asTitlebar")}
              </label>
              <Switch
                id="tabbar-as-titlebar"
                checked={settings.tabbarAsTitlebar}
                onChange={(e) =>
                  handleSettingChange("tabbarAsTitlebar", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="tabbar-one-liner">
                {t("design.lepton-preferences.tabbar.oneLiner")}
              </label>
              <Switch
                id="tabbar-one-liner"
                checked={settings.tabbarOneLiner}
                onChange={(e) =>
                  handleSettingChange("tabbarOneLiner", e.target.checked)}
              />
            </div>
          </CardContent>
        </Card>

        {/* Sidebar Settings */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Sidebar className="size-5" />
              {t("design.lepton-preferences.sidebar.title")}
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between">
              <label htmlFor="sidebar-overlap">
                {t("design.lepton-preferences.sidebar.overlap")}
              </label>
              <Switch
                id="sidebar-overlap"
                checked={settings.sidebarOverlap}
                onChange={(e) =>
                  handleSettingChange("sidebarOverlap", e.target.checked)}
              />
            </div>
          </CardContent>
        </Card>
      </div>
    </div>
  );
}
