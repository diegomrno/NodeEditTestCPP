#include <filesystem>
#include <iostream>
#include <string>

#include "../../../src/module.hpp"
#include "node_editor_wrapper.hpp"

namespace ModuleUI {

  static int it = 1;
  TestCppOtherRandomWindow::TestCppOtherRandomWindow() {
    it++;
    app_window_ = std::make_shared<Cherry::AppWindow>(std::to_string(it), std::to_string(it));

    // app_window_->SetLeftMenubarCallback([this]() { ImGui::Text("GHE;llofsoig"); });
    if (it == 2) {
      app_window_->SetDefaultBehavior(DefaultAppWindowBehaviors::DefaultDocking, "left");
    }
    if (it == 3) {
      app_window_->SetDefaultBehavior(DefaultAppWindowBehaviors::DefaultDocking, "right");
    }
    if (it == 4) {
      app_window_->SetDefaultBehavior(DefaultAppWindowBehaviors::DefaultDocking, "right_up");
    }
    if (it == 5) {
      app_window_->SetDefaultBehavior(DefaultAppWindowBehaviors::DefaultDocking, "left_down");
    }
    // app_window_->SetInternalPaddingY(0.0f);
    // app_window_->SetInternalPaddingX(0.0f);

    app_window_->m_CloseCallback = [=]() {
      Cherry::DeleteAppWindow(app_window_);
      app_window_->SetVisibility(false);
    };
    this->ctx = vxe::get_current_context();
  }  // namespace ModuleUI

  std::shared_ptr<Cherry::AppWindow> &TestCppOtherRandomWindow::get_app_window() {
    return app_window_;
  }

  std::shared_ptr<TestCppOtherRandomWindow> TestCppOtherRandomWindow::create() {
    auto instance = std::shared_ptr<TestCppOtherRandomWindow>(new TestCppOtherRandomWindow());
    instance->setup_render_callback();
    return instance;
  }

  void TestCppOtherRandomWindow::setup_render_callback() {
    auto self = shared_from_this();
    app_window_->SetRenderCallback([self]() {
      if (self) {
        self->render();
      }
    });
  }

  void TestCppOtherRandomWindow::render() {
    if (!i) {
      auto parent = Cherry::GetAppWindowByName("TEST");
      if (parent) {
        app_window_->SetParent(parent);
        app_window_->m_WindowRebuilded = false;
      }
      i = true;
    }

    ImGui::Text("Hello window !");
  }

};  // namespace ModuleUI
