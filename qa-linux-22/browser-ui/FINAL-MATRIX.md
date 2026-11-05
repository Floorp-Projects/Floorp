# 担当B最終matrix

|key|実施試験|分類|残条件|PR|Jira comment ID|
|---|---|---|---|---|---|
|KAN-3|PYTHONPATH=python python3 run.py floorp 29284 split|実再現・CSS欠陥修正Draft|元Fedora desktop/package・物理ドラッグ/メニュー; full production build; 候補の正式product build（今回source CSS差替fixture）|https://github.com/Floorp-Projects/Floorp/pull/3033|10294|
|KAN-5|PYTHONPATH=python python3 run.py floorp 29284 desktop-drag|限定条件で未再現・既存修正と整合|Fedora44実desktop/元package/Flatpak; 物理入力; 既存窓へのproxyドラッグ（native adoptのみ実施）|https://github.com/Floorp-Projects/Floorp/pull/2763|10283|
|KAN-29|dbus-run-session -- bash KAN-29/nested-panel.sh; python3 KAN-29/analyze-wayland.py; QA_PORT=29665 bash KAN-29/nested-localized.sh; QA_PORT=29666 QA_PRODUCT=floorp-pr2877 bash KAN-29/nested-localized.sh; PYTHONPATH=python python3 KAN-29/analyze-localized.py|原LTR限定未再現／実localized RTLで関連欠陥再現・既存PR2877候補で改善|原LTR CachyOS/Flatpak・distro package症状の再現/解消; 物理入力・実GPU/原compositor; full official candidate product build（今回既存PR計算のみ診断copy）|https://github.com/Floorp-Projects/Floorp/pull/2877|10284|
|KAN-31|PYTHONPATH=python python3 run.py floorp 29284 search|限定条件で未再現|Fedora44実desktop/元package/元配布形式; 起動直後のより早期入力/他言語|—|10285|
|KAN-39|PYTHONPATH=python python3 run.py floorp 29284 wheel|正規化実測・原症状の解消は未確定|Fedora44実desktop/元package/物理wheel・DPI/加速; 通常tabbarとの有効な同入力距離比較|https://github.com/Floorp-Projects/Floorp/pull/2763|10286|
|KAN-40|PYTHONPATH=python python3 run.py floorp 29284 middle|限定条件で未再現・既存修正と整合|Fedora44実desktop/元package/Flatpak/物理中クリック; 通常タブ・stack全体中クリックの追加対照|https://github.com/Floorp-Projects/Floorp/pull/2763|10287|
|KAN-54|dbus-run-session -- bash nested.sh (isolated KWin + Floorp visual/controls/layout)|表示重複を実観測・Sprint対象外|Manjaro/完全なPlasma session; minimize/maximize含む3ボタン/仕様確認|—|10288|
|KAN-87|dbus-run-session -- bash qaLinuxKAN87/nested-zen.sh 29655 paint; python3 qaLinuxKAN87/measure-pixels.py 29655; bash qaLinuxKAN87/signed/nested.sh 29701 paint; python3 qaLinuxKAN87/signed/measure-pixels.py 29701|限定条件で未再現・既存修正と整合|原CachyOS/Niri/Hyprland/MangoWM; 物理入力・hardware GPU/monitor|https://github.com/Floorp-Projects/Floorp/pull/2767|10289|
|KAN-93|dbus-run-session -- bash nested-floorp-final.sh; dbus-run-session -- bash nested-firefox.sh|限定条件で未再現|KDE Neon/実theme・accent・font条件; 元版12.17.1/Firefox154+Lepton|—|10290|
|KAN-98|PYTHONPATH=python python3 gesture-run.py floorp 29311 gesture; PYTHONPATH=python python3 kan98-sparse-product-run.py floorp 29333 sparse-product|実再現・独立した2欠陥を限定修正Draft|Arch/Sway/Wayland/物理入力/DPI; full production build|https://github.com/Floorp-Projects/Floorp/pull/3031; https://github.com/Floorp-Projects/Floorp/pull/3034|10291|
