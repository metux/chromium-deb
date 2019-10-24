// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands.h"

#include "ash/public/cpp/window_properties.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/shell_test_api.test-mojom-test-utils.h"
#include "ash/public/interfaces/shell_test_api.test-mojom.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/mus/change_completion_waiter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using testing::Combine;
using testing::Values;
using testing::WithParamInterface;

namespace {

// WidgetDelegateView which allows the widget to be maximized.
class MaximizableWidgetDelegate : public views::WidgetDelegateView {
 public:
  MaximizableWidgetDelegate() {}
  ~MaximizableWidgetDelegate() override {}

  bool CanMaximize() const override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(MaximizableWidgetDelegate);
};

// Tells Ash to toggle fullscreen as if the user had pressed the hardware
// fullscreen key.
void ToggleFullscreen() {
  ash::mojom::ShellTestApiPtr shell_test_api;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &shell_test_api);
  ash::mojom::ShellTestApiAsyncWaiter waiter(shell_test_api.get());
  waiter.ToggleFullscreen();
  aura::test::WaitForAllChangesToComplete();
}

bool IsInImmersive(aura::Window* window) {
  aura::Window* toplevel =
      features::IsUsingWindowService() ? window->GetRootWindow() : window;
  return toplevel->GetProperty(ash::kImmersiveIsActive);
}

}  // namespace

class AcceleratorCommandsFullscreenBrowserTest
    : public WithParamInterface<ui::WindowShowState>,
      public InProcessBrowserTest {
 public:
  AcceleratorCommandsFullscreenBrowserTest()
      : initial_show_state_(GetParam()) {}
  virtual ~AcceleratorCommandsFullscreenBrowserTest() {}

  // Sets |widget|'s show state to |initial_show_state_|.
  void SetToInitialShowState(views::Widget* widget) {
    if (initial_show_state_ == ui::SHOW_STATE_MAXIMIZED)
      widget->Maximize();
    else
      widget->Restore();
  }

  // Returns true if |widget|'s show state is |initial_show_state_|.
  bool IsInitialShowState(const views::Widget* widget) const {
    if (initial_show_state_ == ui::SHOW_STATE_MAXIMIZED) {
      return widget->IsMaximized();
    } else {
      return !widget->IsMaximized() && !widget->IsFullscreen() &&
             !widget->IsMinimized();
    }
  }

 private:
  ui::WindowShowState initial_show_state_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorCommandsFullscreenBrowserTest);
};

// Test that toggling window fullscreen works properly.
IN_PROC_BROWSER_TEST_P(AcceleratorCommandsFullscreenBrowserTest,
                       ToggleFullscreen) {
  // 1) Browser windows.
  aura::Window* window = browser()->window()->GetNativeWindow();
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  ASSERT_TRUE(browser()->is_type_tabbed());
  ASSERT_TRUE(widget->IsActive());
  SetToInitialShowState(widget);
  EXPECT_TRUE(IsInitialShowState(widget));

  // Wait for Ash to become aware of active widget.
  aura::test::WaitForAllChangesToComplete();

  ToggleFullscreen();
  EXPECT_TRUE(IsInImmersive(window));

  ToggleFullscreen();
  EXPECT_FALSE(IsInImmersive(window));
  EXPECT_TRUE(IsInitialShowState(widget));

  // 2) ToggleFullscreen() should have no effect on windows which cannot be
  // maximized.
  aura::Window* toplevel =
      features::IsUsingWindowService() ? window->GetRootWindow() : window;
  toplevel->SetProperty(aura::client::kResizeBehaviorKey,
                        ws::mojom::kResizeBehaviorNone);
  aura::test::WaitForAllChangesToComplete();
  ToggleFullscreen();
  EXPECT_TRUE(IsInitialShowState(widget));

  // 3) Hosted apps.
  Browser::CreateParams browser_create_params(
      Browser::CreateParams::CreateForApp("Test", true /* trusted_source */,
                                          gfx::Rect(), browser()->profile(),
                                          true));

  Browser* app_host_browser = new Browser(browser_create_params);
  ASSERT_TRUE(app_host_browser->is_app());
  AddBlankTabAndShow(app_host_browser);
  window = app_host_browser->window()->GetNativeWindow();
  widget = views::Widget::GetWidgetForNativeWindow(window);
  ASSERT_TRUE(widget->IsActive());
  SetToInitialShowState(widget);
  EXPECT_TRUE(IsInitialShowState(widget));

  // Wait for Ash to become aware of active widget.
  aura::test::WaitForAllChangesToComplete();

  ToggleFullscreen();
  EXPECT_TRUE(IsInImmersive(window));

  ToggleFullscreen();
  EXPECT_FALSE(IsInImmersive(window));
  EXPECT_TRUE(IsInitialShowState(widget));

  // 4) Popup browser windows.
  browser_create_params =
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(), true);
  Browser* popup_browser = new Browser(browser_create_params);
  ASSERT_TRUE(popup_browser->is_type_popup());
  ASSERT_FALSE(popup_browser->is_app());
  AddBlankTabAndShow(popup_browser);
  window = popup_browser->window()->GetNativeWindow();
  widget = views::Widget::GetWidgetForNativeWindow(window);
  ASSERT_TRUE(widget->IsActive());
  SetToInitialShowState(widget);
  EXPECT_TRUE(IsInitialShowState(widget));

  // Wait for Ash to become aware of active widget.
  aura::test::WaitForAllChangesToComplete();

  ToggleFullscreen();
  EXPECT_TRUE(IsInImmersive(window));

  ToggleFullscreen();
  EXPECT_FALSE(IsInImmersive(window));
  EXPECT_TRUE(IsInitialShowState(widget));

  // 5) Miscellaneous windows (e.g. task manager).
  views::Widget::InitParams params;
  params.delegate = new MaximizableWidgetDelegate();
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  views::Widget misc_widget;
  widget = &misc_widget;
  widget->Init(params);
  widget->Show();
  window = widget->GetNativeWindow();

  ASSERT_TRUE(widget->IsActive());
  SetToInitialShowState(widget);
  EXPECT_TRUE(IsInitialShowState(widget));

  // Wait for Ash to become aware of active widget.
  aura::test::WaitForAllChangesToComplete();

  ToggleFullscreen();
  EXPECT_TRUE(IsInImmersive(window));

  ToggleFullscreen();
  EXPECT_FALSE(IsInImmersive(window));
  EXPECT_TRUE(IsInitialShowState(widget));

  // TODO(pkotwicz|oshima): Make toggling fullscreen restore the window to its
  // show state prior to entering fullscreen.
  EXPECT_FALSE(widget->IsFullscreen());
}

INSTANTIATE_TEST_CASE_P(InitiallyRestored,
                        AcceleratorCommandsFullscreenBrowserTest,
                        Values(ui::SHOW_STATE_NORMAL));
INSTANTIATE_TEST_CASE_P(InitiallyMaximized,
                        AcceleratorCommandsFullscreenBrowserTest,
                        Values(ui::SHOW_STATE_MAXIMIZED));

class AcceleratorCommandsPlatformAppFullscreenBrowserTest
    : public WithParamInterface<ui::WindowShowState>,
      public extensions::PlatformAppBrowserTest {
 public:
  AcceleratorCommandsPlatformAppFullscreenBrowserTest()
      : initial_show_state_(GetParam()) {}
  virtual ~AcceleratorCommandsPlatformAppFullscreenBrowserTest() {}

  // Sets |app_window|'s show state to |initial_show_state_|.
  void SetToInitialShowState(extensions::AppWindow* app_window) {
    if (initial_show_state_ == ui::SHOW_STATE_MAXIMIZED)
      app_window->Maximize();
    else
      app_window->Restore();
  }

  // Returns true if |app_window|'s show state is |initial_show_state_|.
  bool IsInitialShowState(extensions::AppWindow* app_window) const {
    if (initial_show_state_ == ui::SHOW_STATE_MAXIMIZED)
      return app_window->GetBaseWindow()->IsMaximized();
    else
      return ui::BaseWindow::IsRestored(*app_window->GetBaseWindow());
  }

 private:
  ui::WindowShowState initial_show_state_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorCommandsPlatformAppFullscreenBrowserTest);
};

// Test the behavior of platform apps when ToggleFullscreen() is called.
IN_PROC_BROWSER_TEST_P(AcceleratorCommandsPlatformAppFullscreenBrowserTest,
                       ToggleFullscreen) {
  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("minimal", "Launched");

  {
    // Test that ToggleFullscreen() toggles a platform's app's fullscreen
    // state and that it additionally puts the app into immersive fullscreen
    // if put_all_windows_in_immersive() returns true.
    extensions::AppWindow::CreateParams params;
    params.frame = extensions::AppWindow::FRAME_CHROME;
    extensions::AppWindow* app_window =
        CreateAppWindowFromParams(browser()->profile(), extension, params);
    extensions::NativeAppWindow* native_app_window =
        app_window->GetBaseWindow();
    SetToInitialShowState(app_window);
    ASSERT_TRUE(app_window->GetBaseWindow()->IsActive());
    EXPECT_TRUE(IsInitialShowState(app_window));

    // Wait for Ash to become aware of active widget.
    aura::test::WaitForAllChangesToComplete();

    ToggleFullscreen();
    EXPECT_TRUE(native_app_window->IsFullscreen());
    EXPECT_TRUE(IsInImmersive(native_app_window->GetNativeWindow()));

    ToggleFullscreen();
    EXPECT_FALSE(native_app_window->IsFullscreen());
    EXPECT_TRUE(IsInitialShowState(app_window));

    CloseAppWindow(app_window);
  }

  {
    // Repeat the test, but make sure that frameless platform apps are never put
    // into immersive fullscreen.
    extensions::AppWindow::CreateParams params;
    params.frame = extensions::AppWindow::FRAME_NONE;
    extensions::AppWindow* app_window =
        CreateAppWindowFromParams(browser()->profile(), extension, params);
    extensions::NativeAppWindow* native_app_window =
        app_window->GetBaseWindow();
    ASSERT_TRUE(app_window->GetBaseWindow()->IsActive());
    SetToInitialShowState(app_window);
    EXPECT_TRUE(IsInitialShowState(app_window));

    // Wait for Ash to become aware of active widget.
    aura::test::WaitForAllChangesToComplete();

    ToggleFullscreen();
    EXPECT_TRUE(native_app_window->IsFullscreen());
    EXPECT_FALSE(IsInImmersive(native_app_window->GetNativeWindow()));

    ToggleFullscreen();
    EXPECT_FALSE(native_app_window->IsFullscreen());
    EXPECT_TRUE(IsInitialShowState(app_window));

    CloseAppWindow(app_window);
  }
}

INSTANTIATE_TEST_CASE_P(InitiallyRestored,
                        AcceleratorCommandsPlatformAppFullscreenBrowserTest,
                        Values(ui::SHOW_STATE_NORMAL));
INSTANTIATE_TEST_CASE_P(InitiallyMaximized,
                        AcceleratorCommandsPlatformAppFullscreenBrowserTest,
                        Values(ui::SHOW_STATE_MAXIMIZED));