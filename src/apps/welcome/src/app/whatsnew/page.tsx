import { useTranslation } from "react-i18next";
import panelSidebarSvg from "../features/assets/panelsidebar.svg";
import workspacesSvg from "../features/assets/workspaces.svg";
import pwaSvg from "../features/assets/pwa.svg";
import mouseGestureSvg from "../features/assets/mousegesture.svg";
import headerCover from "./header-cover.webp";
import {
  ArrowRight,
  Github,
  Globe,
  HelpCircle,
  MessageCircle,
  Twitter,
  Youtube,
} from "lucide-react";

export default function WhatsNewPage() {
  const { t } = useTranslation();

  const closePage = () => {
    globalThis.open("about:newtab", "_blank");
    setTimeout(() => globalThis.close(), 50);
  };

  return (
    <div className="min-h-screen flex flex-col">
      <header className="relative overflow-hidden">
        <div
          className="relative m-5 rounded-2xl"
          style={{
            backgroundImage: `url(${headerCover})`,
            backgroundSize: "cover",
            backgroundPosition: "center",
          }}
        >
          <div className="container mx-auto px-6 py-32 relative">
            <div className="text-center">
              <h1 className="text-5xl md:text-5xl font-bold text-gray-200 mb-8">
                Floorp の新機能
              </h1>
            </div>
          </div>
        </div>
      </header>

      <main className="bg-base-100 flex-1">
        <div className="container mx-auto max-w-6xl px-6">
          <FeatureSection
            tag="生産性"
            title={t("featuresPage.panelSidebar.title")}
            description={t("featuresPage.panelSidebar.description")}
            image={panelSidebarSvg}
            bullets={(t("featuresPage.panelSidebar.features", {
              returnObjects: true,
            }) as string[]) || []}
          />

          <FeatureSection
            tag="ワークスペース"
            title={t("featuresPage.workspaces.title")}
            description={t("featuresPage.workspaces.description")}
            image={workspacesSvg}
            bullets={(t("featuresPage.workspaces.features", {
              returnObjects: true,
            }) as string[]) || []}
            reverse
          />

          <FeatureSection
            tag="PWA"
            title={t("featuresPage.pwa.title")}
            description={t("featuresPage.pwa.description")}
            image={pwaSvg}
            bullets={(t("featuresPage.pwa.features", {
              returnObjects: true,
            }) as string[]) || []}
          />

          <FeatureSection
            tag="ジェスチャー"
            title={t("featuresPage.mouseGesture.title")}
            description={t("featuresPage.mouseGesture.description")}
            image={mouseGestureSvg}
            bullets={(t("featuresPage.mouseGesture.features", {
              returnObjects: true,
            }) as string[]) || []}
            reverse
          />

          <div className="py-16 text-center">
            <button
              type="button"
              className="btn btn-primary btn-lg"
              onClick={closePage}
            >
              <ArrowRight className="mr-2" size={20} />{" "}
              {t("finishPage.getStarted")}
            </button>
          </div>
        </div>
      </main>

      {/* Footer */}
      <footer className="bg-gray-800 text-gray-300 py-12">
        <div className="container mx-auto max-w-6xl px-6">
          <div className="flex flex-col lg:flex-row justify-between items-start lg:items-center gap-8">
            {/* Social Media Links */}
            <div className="flex flex-col gap-4">
              <h3 className="text-white font-medium">フォロー</h3>
              <div className="flex gap-4">
                <a
                  href="https://github.com/Floorp-Projects/Floorp"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="p-2 rounded-lg hover:bg-gray-700 transition-colors"
                  aria-label="GitHub"
                >
                  <Github size={24} />
                </a>
                <a
                  href="https://twitter.com/floorp_browser"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="p-2 rounded-lg hover:bg-gray-700 transition-colors"
                  aria-label="X (Twitter)"
                >
                  <Twitter size={24} />
                </a>
                <a
                  href="https://floorp.app/discord"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="p-2 rounded-lg hover:bg-gray-700 transition-colors"
                  aria-label="Discord"
                >
                  <MessageCircle size={24} />
                </a>
              </div>
            </div>

            {/* Brand and Links */}
            <div className="flex flex-col lg:flex-row items-start lg:items-center gap-6">
              <div className="flex items-center gap-2">
                <img
                  src="chrome://branding/content/icon128.png"
                  alt="Floorp"
                  className="w-6 h-6"
                />
                <span className="text-white font-medium">Floorp</span>
              </div>

              <div className="flex flex-wrap gap-6 text-sm">
                <a
                  href="https://floorp.app/privacy"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="hover:text-white transition-colors"
                >
                  プライバシーと利用規約
                </a>
                <a
                  href="https://docs.floorp.app"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="hover:text-white transition-colors"
                >
                  Floorp について
                </a>
              </div>
            </div>
          </div>

          <div className="mt-8 pt-8 border-t border-gray-700">
            <p className="text-center text-sm text-gray-400">
              © 2025 Floorp Projects. All rights reserved.
            </p>
          </div>
        </div>
      </footer>
    </div>
  );
}

function FeatureSection(props: {
  tag: string;
  title: string;
  description: string;
  image: string;
  bullets: string[];
  reverse?: boolean;
}) {
  return (
    <section className="py-16">
      <div
        className={`flex flex-col lg:flex-row gap-12 items-center ${
          props.reverse ? "lg:flex-row-reverse" : ""
        }`}
      >
        <div className="lg:w-1/2 space-y-6">
          <div>
            <div className="inline-block bg-primary text-primary-content text-sm font-medium px-3 py-1 rounded-full mb-4">
              {props.tag}
            </div>
            <h2 className="text-3xl lg:text-4xl font-bold mb-4">
              {props.title}
            </h2>
            <p className="text-lg text-base-content/80 leading-relaxed mb-6">
              {props.description}
            </p>
          </div>

          {props.bullets.length > 0 && (
            <div className="space-y-3">
              <ol className="space-y-2">
                {props.bullets.map((feature, index) => (
                  <li key={index} className="flex items-start gap-3">
                    <span className="flex-shrink-0 w-6 h-6 bg-primary text-primary-content rounded-full flex items-center justify-center text-sm font-medium">
                      {index + 1}
                    </span>
                    <span className="text-base-content/80">{feature}</span>
                  </li>
                ))}
              </ol>
            </div>
          )}
        </div>

        <div className="lg:w-1/2">
          <div className="bg-base-200 rounded-2xl p-8 shadow-lg">
            <img
              src={props.image}
              alt={props.title}
              className="w-full h-auto object-contain max-h-80"
            />
          </div>
        </div>
      </div>
    </section>
  );
}
