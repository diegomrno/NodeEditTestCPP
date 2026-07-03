#include <filesystem>
#include <iostream>
#include <string>

#include "../../../src/module.hpp"
#include "node_editor_wrapper.hpp"

namespace ModuleUI {

  static int it = 1;
  DetailsWindow::DetailsWindow(const std::string& parent_name, const std::string& id) {
    it++;
    app_window_ = std::make_shared<Cherry::AppWindow>("Details", "Details");

    id_ = id;
    parent_name_ = parent_name;
    app_window_->SetDefaultBehavior(DefaultAppWindowBehaviors::DefaultDocking, "right");

    app_window_->m_CloseCallback = [=]() {
      Cherry::DeleteAppWindow(app_window_);
      app_window_->SetVisibility(false);
    };
    this->ctx = vxe::get_current_context();
  }  // namespace ModuleUI

  std::shared_ptr<Cherry::AppWindow>& DetailsWindow::get_app_window() {
    return app_window_;
  }

  std::shared_ptr<DetailsWindow> DetailsWindow::create(const std::string& parent_name, const std::string& id) {
    auto instance = std::shared_ptr<DetailsWindow>(new DetailsWindow(parent_name, id));
    instance->setup_render_callback();
    return instance;
  }

  void DetailsWindow::setup_render_callback() {
    auto self = shared_from_this();
    app_window_->SetRenderCallback([self]() {
      if (self) {
        self->render();
      }
    });
  }

  void DetailsWindow::render() {
    if (!i) {
      auto parent = Cherry::GetAppWindowByName(parent_name_);
      if (parent) {
        app_window_->SetParent(parent);
        app_window_->m_WindowRebuilded = false;
      }
      i = true;
    }

    ImGui::Text("Hello window !");
  }

};  // namespace ModuleUI
