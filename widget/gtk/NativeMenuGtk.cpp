/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NativeMenuGtk.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/XULCommandEvent.h"
#include "mozilla/WidgetUtilsGtk.h"
#include "mozilla/EventDispatcher.h"
#include "nsPresContext.h"
#include "nsIWidget.h"
#include "nsWindow.h"
#include "nsStubMutationObserver.h"
#include "mozilla/dom/Element.h"
#include "mozilla/StaticPrefs_widget.h"

#include <dlfcn.h>
#include <gtk/gtk.h>

namespace mozilla::widget {

using GtkMenuPopupAtRect = void (*)(GtkMenu* menu, GdkWindow* rect_window,
                                    const GdkRectangle* rect,
                                    GdkGravity rect_anchor,
                                    GdkGravity menu_anchor,
                                    const GdkEvent* trigger_event);

static bool IsDisabled(const dom::Element& aElement) {
  return aElement.AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                              nsGkAtoms::_true, eCaseMatters) ||
         aElement.AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidden,
                              nsGkAtoms::_true, eCaseMatters);
}
static bool NodeIsRelevant(const nsINode& aNode) {
  return aNode.IsAnyOfXULElements(nsGkAtoms::menu, nsGkAtoms::menuseparator,
                                  nsGkAtoms::menuitem, nsGkAtoms::menugroup);
}

// If this is a radio / checkbox menuitem, get the current value.
static Maybe<bool> GetChecked(const dom::Element& aMenuItem) {
  static dom::Element::AttrValuesArray strings[] = {nsGkAtoms::checkbox,
                                                    nsGkAtoms::radio, nullptr};
  switch (aMenuItem.FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::type, strings,
                                    eCaseMatters)) {
    case 0:
      break;
    case 1:
      break;
    default:
      return Nothing();
  }

  return Some(aMenuItem.AttrValueIs(kNameSpaceID_None, nsGkAtoms::checked,
                                    nsGkAtoms::_true, eCaseMatters));
}

struct Actions {
  RefPtr<GSimpleActionGroup> mGroup;
  size_t mNextActionIndex = 0;

  nsPrintfCString Register(const dom::Element&, bool aForSubmenu);
  void Clear();
};

static MOZ_CAN_RUN_SCRIPT void ActivateItem(dom::Element& aElement) {
  if (Maybe<bool> checked = GetChecked(aElement)) {
    if (!aElement.AttrValueIs(kNameSpaceID_None, nsGkAtoms::autocheck,
                              nsGkAtoms::_false, eCaseMatters)) {
      bool newValue = !*checked;
      if (newValue) {
        aElement.SetAttr(kNameSpaceID_None, nsGkAtoms::checked, u"true"_ns,
                         true);
      } else {
        aElement.UnsetAttr(kNameSpaceID_None, nsGkAtoms::checked, true);
      }
    }
  }

  RefPtr doc = aElement.OwnerDoc();
  RefPtr event = new dom::XULCommandEvent(doc, doc->GetPresContext(), nullptr);
  IgnoredErrorResult rv;
  event->InitCommandEvent(u"command"_ns, true, true,
                          nsGlobalWindowInner::Cast(doc->GetInnerWindow()), 0,
                          /* ctrlKey = */ false, /* altKey = */ false,
                          /* shiftKey = */ false, /* cmdKey = */ false,
                          /* button = */ MouseButton::ePrimary, nullptr, 0, rv);
  if (MOZ_UNLIKELY(rv.Failed())) {
    return;
  }
  aElement.DispatchEvent(*event);
}

static MOZ_CAN_RUN_SCRIPT void ActivateSignal(GSimpleAction* aAction,
                                              GVariant* aParam,
                                              gpointer aUserData) {
  RefPtr element = static_cast<dom::Element*>(aUserData);
  ActivateItem(*element);
}

static MOZ_CAN_RUN_SCRIPT void FireEvent(dom::Element* aTarget,
                                         EventMessage aPopupMessage) {
  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetMouseEvent event(true, aPopupMessage, nullptr, WidgetMouseEvent::eReal);
  EventDispatcher::Dispatch(aTarget, nullptr, &event, nullptr, &status);
}

static MOZ_CAN_RUN_SCRIPT void ChangeStateSignal(GSimpleAction* aAction,
                                                 GVariant* aParam,
                                                 gpointer aUserData) {
  // TODO: Fire events when safe. These run at a bad time for now.
  static constexpr bool kEnabled = false;
  if (!kEnabled) {
    return;
  }
  const bool open = g_variant_get_boolean(aParam);
  RefPtr popup = static_cast<dom::Element*>(aUserData);
  if (open) {
    FireEvent(popup, eXULPopupShowing);
    FireEvent(popup, eXULPopupShown);
  } else {
    FireEvent(popup, eXULPopupHiding);
    FireEvent(popup, eXULPopupHidden);
  }
}

nsPrintfCString Actions::Register(const dom::Element& aMenuItem,
                                  bool aForSubmenu) {
  nsPrintfCString actionName("item-%zu", mNextActionIndex++);
  Maybe<bool> paramValue = aForSubmenu ? Some(false) : GetChecked(aMenuItem);
  RefPtr<GSimpleAction> action;
  if (paramValue) {
    action = dont_AddRef(g_simple_action_new_stateful(
        actionName.get(), nullptr, g_variant_new_boolean(*paramValue)));
  } else {
    action = dont_AddRef(g_simple_action_new(actionName.get(), nullptr));
  }
  if (aForSubmenu) {
    g_signal_connect(action, "change-state", G_CALLBACK(ChangeStateSignal),
                     gpointer(&aMenuItem));
  } else {
    g_signal_connect(action, "activate", G_CALLBACK(ActivateSignal),
                     gpointer(&aMenuItem));
  }
  g_action_map_add_action(G_ACTION_MAP(mGroup.get()), G_ACTION(action.get()));
  return actionName;
}

void Actions::Clear() {
  for (size_t i = 0; i < mNextActionIndex; ++i) {
    g_action_map_remove_action(G_ACTION_MAP(mGroup.get()),
                               nsPrintfCString("item-%zu", i).get());
  }
  mNextActionIndex = 0;
}

class MenuModel final : public nsStubMutationObserver {
  NS_DECL_ISUPPORTS

  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED

 public:
  explicit MenuModel(dom::Element* aElement) : mElement(aElement) {
    mElement->AddMutationObserver(this);
    mGMenu = dont_AddRef(g_menu_new());
    mActions.mGroup = dont_AddRef(g_simple_action_group_new());
  }

  GMenuModel* GetModel() { return G_MENU_MODEL(mGMenu.get()); }
  GActionGroup* GetActionGroup() {
    return G_ACTION_GROUP(mActions.mGroup.get());
  }

  dom::Element* Element() { return mElement; }

  void RecomputeModelIfNeeded();

  bool IsShowing() { return mPoppedUp; }
  void WillShow() {
    mPoppedUp = true;
    RecomputeModelIfNeeded();
  }
  void DidHide() { mPoppedUp = false; }

 private:
  virtual ~MenuModel() { mElement->RemoveMutationObserver(this); }

  void DirtyModel() {
    mDirty = true;
    if (mPoppedUp) {
      RecomputeModelIfNeeded();
    }
  }

  RefPtr<dom::Element> mElement;
  RefPtr<GMenu> mGMenu;
  Actions mActions;
  bool mDirty = true;
  bool mPoppedUp = false;
};

NS_IMPL_ISUPPORTS(MenuModel, nsIMutationObserver)

void MenuModel::ContentRemoved(nsIContent* aChild, nsIContent*) {
  if (NodeIsRelevant(*aChild)) {
    DirtyModel();
  }
}

void MenuModel::ContentInserted(nsIContent* aChild) {
  if (NodeIsRelevant(*aChild)) {
    DirtyModel();
  }
}

void MenuModel::ContentAppended(nsIContent* aChild) {
  if (NodeIsRelevant(*aChild)) {
    DirtyModel();
  }
}

void MenuModel::AttributeChanged(dom::Element* aElement, int32_t aNameSpaceID,
                                 nsAtom* aAttribute, int32_t aModType,
                                 const nsAttrValue* aOldValue) {
  if (NodeIsRelevant(*aElement) &&
      (aAttribute == nsGkAtoms::label || aAttribute == nsGkAtoms::aria_label ||
       aAttribute == nsGkAtoms::disabled || aAttribute == nsGkAtoms::hidden)) {
    DirtyModel();
  }
}

static const dom::Element* GetMenuPopupChild(const dom::Element& aElement) {
  for (const nsIContent* child = aElement.GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (child->IsXULElement(nsGkAtoms::menupopup)) {
      return child->AsElement();
    }
  }
  return nullptr;
}

static void RecomputeModelFor(GMenu* aMenu, Actions& aActions,
                              const dom::Element& aElement) {
  RefPtr<GMenu> sectionMenu;
  auto FlushSectionMenu = [&] {
    if (sectionMenu) {
      g_menu_append_section(aMenu, nullptr, G_MENU_MODEL(sectionMenu.get()));
      sectionMenu = nullptr;
    }
  };

  for (const nsIContent* child = aElement.GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (child->IsXULElement(nsGkAtoms::menuitem) &&
        !IsDisabled(*child->AsElement())) {
      nsAutoString label;
      child->AsElement()->GetAttr(nsGkAtoms::label, label);
      if (label.IsEmpty()) {
        child->AsElement()->GetAttr(nsGkAtoms::aria_label, label);
      }
      nsPrintfCString actionName(
          "menu.%s",
          aActions.Register(*child->AsElement(), /* aForSubmenu = */ false)
              .get());
      g_menu_append(sectionMenu ? sectionMenu.get() : aMenu,
                    NS_ConvertUTF16toUTF8(label).get(), actionName.get());
      continue;
    }
    if (child->IsXULElement(nsGkAtoms::menuseparator)) {
      FlushSectionMenu();
      sectionMenu = dont_AddRef(g_menu_new());
      continue;
    }
    if (child->IsXULElement(nsGkAtoms::menugroup)) {
      FlushSectionMenu();
      sectionMenu = dont_AddRef(g_menu_new());
      RecomputeModelFor(sectionMenu, aActions, *child->AsElement());
      FlushSectionMenu();
      continue;
    }
    if (child->IsXULElement(nsGkAtoms::menu) &&
        !IsDisabled(*child->AsElement())) {
      if (const auto* popup = GetMenuPopupChild(*child->AsElement())) {
        RefPtr<GMenu> submenu = dont_AddRef(g_menu_new());
        RecomputeModelFor(submenu, aActions, *popup);
        nsAutoString label;
        child->AsElement()->GetAttr(nsGkAtoms::label, label);
        RefPtr<GMenuItem> submenuItem = dont_AddRef(g_menu_item_new_submenu(
            NS_ConvertUTF16toUTF8(label).get(), G_MENU_MODEL(submenu.get())));
        nsPrintfCString actionName(
            "menu.%s",
            aActions.Register(*popup, /* aForSubmenu = */ true).get());
        g_menu_item_set_attribute_value(submenuItem.get(), "submenu-action",
                                        g_variant_new_string(actionName.get()));
        g_menu_append_item(sectionMenu ? sectionMenu.get() : aMenu,
                           submenuItem.get());
      }
    }
  }

  FlushSectionMenu();
}

void MenuModel::RecomputeModelIfNeeded() {
  if (!mDirty) {
    return;
  }
  mActions.Clear();
  g_menu_remove_all(mGMenu.get());
  RecomputeModelFor(mGMenu.get(), mActions, *mElement);
}

static GtkMenuPopupAtRect GetPopupAtRectFn() {
  static GtkMenuPopupAtRect sFunc =
      (GtkMenuPopupAtRect)dlsym(RTLD_DEFAULT, "gtk_menu_popup_at_rect");
  return sFunc;
}

bool NativeMenuGtk::CanUse() {
  return StaticPrefs::widget_gtk_native_context_menus() && GetPopupAtRectFn();
}

void NativeMenuGtk::FireEvent(EventMessage aPopupMessage) {
  RefPtr target = Element();
  widget::FireEvent(target, aPopupMessage);
}

#define METHOD_SIGNAL(name_)                                 \
  static MOZ_CAN_RUN_SCRIPT_BOUNDARY void On##name_##Signal( \
      GtkWidget* widget, gpointer user_data) {               \
    RefPtr menu = static_cast<NativeMenuGtk*>(user_data);    \
    return menu->On##name_();                                \
  }

METHOD_SIGNAL(Unmap);

#undef METHOD_SIGNAL

NativeMenuGtk::NativeMenuGtk(dom::Element* aElement)
    : mMenuModel(MakeRefPtr<MenuModel>(aElement)) {
  // Floating, so no need to dont_AddRef.
  mNativeMenu = gtk_menu_new_from_model(mMenuModel->GetModel());
  gtk_widget_insert_action_group(mNativeMenu.get(), "menu",
                                 mMenuModel->GetActionGroup());
  g_signal_connect(mNativeMenu, "unmap", G_CALLBACK(OnUnmapSignal), this);
}

NativeMenuGtk::~NativeMenuGtk() {
  g_signal_handlers_disconnect_by_data(mNativeMenu, this);
}

RefPtr<dom::Element> NativeMenuGtk::Element() { return mMenuModel->Element(); }

void NativeMenuGtk::ShowAsContextMenu(nsPresContext* aPc,
                                      const CSSIntPoint& aPosition) {
  if (mMenuModel->IsShowing()) {
    return;
  }
  RefPtr<nsIWidget> widget = aPc->GetRootWidget();
  if (NS_WARN_IF(!widget)) {
    // XXX Do we need to close menus here?
    return;
  }
  auto* win = static_cast<GdkWindow*>(widget->GetNativeData(NS_NATIVE_WINDOW));
  if (NS_WARN_IF(!win)) {
    return;
  }

  auto* geckoWin = static_cast<nsWindow*>(widget.get());
  // The position needs to be relative to our window.
  auto pos = (aPosition * aPc->CSSToDevPixelScale()) -
             geckoWin->WidgetToScreenOffset();
  auto gdkPos = geckoWin->DevicePixelsToGdkPointRoundDown(
      LayoutDeviceIntPoint::Round(pos));

  mMenuModel->WillShow();
  const GdkRectangle rect = {gdkPos.x, gdkPos.y, 1, 1};
  auto openFn = GetPopupAtRectFn();
  openFn(GTK_MENU(mNativeMenu.get()), win, &rect, GDK_GRAVITY_NORTH_WEST,
         GDK_GRAVITY_NORTH_WEST, GetLastMousePressEvent());

  RefPtr pin{this};
  FireEvent(eXULPopupShown);
}

bool NativeMenuGtk::Close() {
  if (!mMenuModel->IsShowing()) {
    return false;
  }
  gtk_menu_popdown(GTK_MENU(mNativeMenu.get()));
  return true;
}

void NativeMenuGtk::OnUnmap() {
  FireEvent(eXULPopupHiding);

  mMenuModel->DidHide();

  FireEvent(eXULPopupHidden);

  for (NativeMenu::Observer* observer : mObservers.Clone()) {
    observer->OnNativeMenuClosed();
  }
}

void NativeMenuGtk::ActivateItem(dom::Element* aItemElement, Modifiers,
                                 int16_t aButton, ErrorResult&) {
  // TODO: For testing only.
}

void NativeMenuGtk::OpenSubmenu(dom::Element*) {
  // TODO: For testing mostly.
}

void NativeMenuGtk::CloseSubmenu(dom::Element*) {
  // TODO: For testing mostly.
}

}  // namespace mozilla::widget
