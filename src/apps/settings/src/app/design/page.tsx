import React from "react";
import { useForm, FormProvider, useWatch } from "react-hook-form";
import { useTranslation } from "react-i18next";
import { getDesignSettings, saveDesignSettings } from "@/app/design/dataManager.ts";
import { useInterfaceDesigns } from "@/app/design/useInterfaceDesigns.ts";

export default function Page() {
    const { t } = useTranslation();
    const methods = useForm({ defaultValues: {} });
    const { setValue, control, getValues } = methods;
    const watchAll = useWatch({ control });
    const interfaceOptions = useInterfaceDesigns();

    React.useEffect(() => {
        const fetchDefaultValues = async () => {
            const values = await getDesignSettings();
            for (const key in values) {
                setValue(key, values[key]);
            }
        };
        fetchDefaultValues();
        document.documentElement.addEventListener("onfocus", fetchDefaultValues);
        return () => {
            document.documentElement.removeEventListener("onfocus", fetchDefaultValues);
        };
    }, [setValue]);

    React.useEffect(() => {
        saveDesignSettings(watchAll);
    }, [watchAll]);

    return (
        <div className="max-w-[700px] mx-auto py-8 flex flex-col items-center">
            <h1 className="text-3xl mb-10">{t("design.tabAndAppearance")}</h1>
            <p className="mb-8">{t("design.customizePositionOfToolbars")}</p>

            <FormProvider {...methods}>
                <form className="w-full space-y-6">
                    {/* Interface Section */}
                    <section className="border rounded p-4">
                        <h2 className="text-xl mb-2">{t("design.interface")}</h2>
                        <p className="text-sm mb-4">{t("design.interfaceDescription")}</p>
                        <div className="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 gap-3">
                            {interfaceOptions.map((option) => (
                                <label key={option.value} className="flex flex-col items-center border rounded p-2 cursor-pointer">
                                    <img src={option.image} alt={option.title} className="w-16 h-16 mb-2" />
                                    <span>{option.title}</span>
                                    <input
                                        type="radio"
                                        name="design"
                                        value={option.value}
                                        className="hidden"
                                        checked={getValues("design") === option.value}
                                        onChange={() => setValue("design", option.value)}
                                    />
                                </label>
                            ))}
                        </div>
                        {["lepton", "photon", "protonfix"].includes(getValues("design") || "") && (
                            <div className="mt-4 p-2 bg-blue-100 rounded">
                                <p className="text-sm">
                                    {t("design.advancedLeptonThemeSettingsDescription")}{" "}
                                    <a href="https://support.mozilla.org" className="text-blue-500">
                                        {t("design.advancedLeptonThemeSettings")}
                                    </a>
                                </p>
                            </div>
                        )}
                    </section>

                    {/* Tabbar Section */}
                    <section className="border rounded p-4">
                        <h2 className="text-xl mb-2">{t("design.tabBar")}</h2>
                        <p className="text-sm mb-4">{t("design.styleDescription")}</p>
                        <div>
                            <p className="mb-2">{t("design.style")}</p>
                            <div className="flex space-x-4">
                                <label className="flex items-center space-x-2">
                                    <input
                                        type="radio"
                                        name="style"
                                        value="horizontal"
                                        checked={getValues("style") === "horizontal"}
                                        onChange={(e) => setValue("style", e.target.value)}
                                    />
                                    <span>{t("design.horizontal")}</span>
                                </label>
                                <label className="flex items-center space-x-2">
                                    <input
                                        type="radio"
                                        name="style"
                                        value="multirow"
                                        checked={getValues("style") === "multirow"}
                                        onChange={(e) => setValue("style", e.target.value)}
                                    />
                                    <span>{t("design.multirow")}</span>
                                </label>
                            </div>
                        </div>
                        <div className="mt-4 p-2 bg-blue-100 rounded">
                            <p className="text-sm">
                                {t("design.verticalTabIsRemovedFromBrowser")}
                                <br />
                                <a
                                    href="https://docs.floorp.app/docs/features/tab-bar-customization/#tab-bar-layout"
                                    className="text-blue-500"
                                >
                                    {t("design.learnMore")}
                                </a>
                            </p>
                        </div>
                        <div className="mt-4">
                            <p className="mb-2">{t("design.position")}</p>
                            <div className="flex flex-col space-y-2">
                                {[
                                    { value: "default", label: t("design.default") },
                                    { value: "hide-horizontal-tabbar", label: t("design.hideTabBar") },
                                    { value: "optimise-to-vertical-tabbar", label: t("design.hideTabBarAndTitleBar") },
                                    { value: "bottom-of-navigation-toolbar", label: t("design.showTabBarOnBottomOfNavigationBar") },
                                    { value: "bottom-of-window", label: t("design.showTabBarOnBottomOfWindow") },
                                ].map((opt) => (
                                    <label key={opt.value} className="flex items-center space-x-2">
                                        <input
                                            type="radio"
                                            name="position"
                                            value={opt.value}
                                            checked={getValues("position") === opt.value}
                                            onChange={(e) => setValue("position", e.target.value)}
                                        />
                                        <span>{opt.label}</span>
                                    </label>
                                ))}
                            </div>
                        </div>
                    </section>

                    {/* Tab Section */}
                    <section className="border rounded p-4">
                        <h2 className="text-xl mb-2">{t("design.tab.title")}</h2>
                        <div>
                            <p className="mb-2">{t("design.tab.scroll")}</p>
                            <div className="flex items-center justify-between">
                                <label>{t("design.tab.scrollTab")}</label>
                                <input
                                    type="checkbox"
                                    checked={!!getValues("tabScroll")}
                                    onChange={(e) => setValue("tabScroll", e.target.checked)}
                                />
                            </div>
                        </div>
                        <div className="mt-2">
                            <div className="flex items-center justify-between">
                                <label>{t("design.tab.reverseScroll")}</label>
                                <input
                                    type="checkbox"
                                    disabled={!getValues("tabScroll")}
                                    checked={!!getValues("tabScrollReverse")}
                                    onChange={(e) => setValue("tabScrollReverse", e.target.checked)}
                                />
                            </div>
                        </div>
                        <div className="mt-2">
                            <div className="flex items-center justify-between">
                                <label>{t("design.tab.scrollWrap")}</label>
                                <input
                                    type="checkbox"
                                    disabled={!getValues("tabScroll")}
                                    checked={!!getValues("tabScrollWrap")}
                                    onChange={(e) => setValue("tabScrollWrap", e.target.checked)}
                                />
                            </div>
                        </div>
                        {!getValues("tabScroll") && (getValues("tabScrollReverse") || getValues("tabScrollWrap")) && (
                            <div className="mt-4 p-2 bg-blue-100 rounded">
                                <p className="text-sm">
                                    {t("design.tab.scrollPrefInfo", {
                                        reverseScrollPrefName: t("design.tab.reverseScroll"),
                                        scrollWrapPrefName: t("design.tab.scrollWrap"),
                                        scrollPrefName: t("design.tab.scrollTab"),
                                    })}
                                </p>
                            </div>
                        )}
                        <div className="mt-4">
                            <p className="mb-2">{t("design.tab.openPosition")}</p>
                            <div className="flex flex-col space-y-2">
                                {[
                                    { value: -1, label: t("design.tab.openDefault") },
                                    { value: 0, label: t("design.tab.openLast") },
                                    { value: 1, label: t("design.tab.openNext") },
                                ].map((item) => (
                                    <label key={item.value} className="flex items-center space-x-2">
                                        <input
                                            type="radio"
                                            name="tabOpenPosition"
                                            value={item.value}
                                            checked={getValues("tabOpenPosition") === item.value}
                                            onChange={(e) => setValue("tabOpenPosition", Number(e.target.value))}
                                        />
                                        <span>{item.label}</span>
                                    </label>
                                ))}
                            </div>
                        </div>
                        <div className="mt-4">
                            <div className="flex items-center justify-between">
                                <label>{t("design.tab.pinTitle")}</label>
                                <input
                                    type="checkbox"
                                    checked={!!getValues("tabPinTitle")}
                                    onChange={(e) => setValue("tabPinTitle", e.target.checked)}
                                />
                            </div>
                        </div>
                        <div className="mt-2">
                            <div className="flex items-center justify-between">
                                <label>{t("design.tab.doubleClickToClose")}</label>
                                <input
                                    type="checkbox"
                                    checked={!!getValues("tabDubleClickToClose")}
                                    onChange={(e) => setValue("tabDubleClickToClose", e.target.checked)}
                                />
                            </div>
                        </div>
                        <div className="mt-4">
                            <label className="block mb-1">{t("design.tab.minWidth")}</label>
                            <input
                                type="number"
                                className="border rounded w-full p-2"
                                value={getValues("tabMinWidth") || ""}
                                onChange={(e) => setValue("tabMinWidth", Number(e.target.value))}
                            />
                            {(getValues("tabMinWidth") < 60 || getValues("tabMinWidth") > 300) && (
                                <p className="text-red-500 text-sm">
                                    {t("design.tab.minWidthError", { min: 60, max: 300 })}
                                </p>
                            )}
                        </div>
                        <div className="mt-4">
                            <label className="block mb-1">{t("design.tab.minHeight")}</label>
                            <input
                                type="number"
                                className="border rounded w-full p-2"
                                value={getValues("tabMinHeight") || ""}
                                onChange={(e) => setValue("tabMinHeight", Number(e.target.value))}
                            />
                            {(getValues("tabMinHeight") < 20 || getValues("tabMinHeight") > 100) && (
                                <p className="text-red-500 text-sm">
                                    {t("design.tab.minHeightError", { min: 20, max: 100 })}
                                </p>
                            )}
                        </div>
                    </section>
                </form>
            </FormProvider>
        </div>
    );
}
