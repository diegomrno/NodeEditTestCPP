#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "../../../src/module.hpp"
#include "node_editor_wrapper.hpp"

namespace ModuleUI {
  ComponentsWindow::ComponentsWindow(const std::string &parent_name, const std::string &id, const std::string &st) {
    storage_path_ = st;
    parent_name_ = parent_name;
    id_ = id;
    app_window_ = std::make_shared<Cherry::AppWindow>("Components", "Components");

    app_window_->SetDefaultBehavior(DefaultAppWindowBehaviors::DefaultDocking, "left_up");

    app_window_->m_CloseCallback = [=]() {
      Cherry::DeleteAppWindow(app_window_);
      app_window_->SetVisibility(false);
    };
    this->ctx = vxe::get_current_context();
  }

  std::shared_ptr<Cherry::AppWindow> &ComponentsWindow::get_app_window() {
    return app_window_;
  }

  std::shared_ptr<ComponentsWindow>
  ComponentsWindow::create(const std::string &parent_name, const std::string &id, const std::string &st) {
    auto instance = std::shared_ptr<ComponentsWindow>(new ComponentsWindow(parent_name, id, st));
    instance->setup_render_callback();
    return instance;
  }

  void ComponentsWindow::setup_render_callback() {
    auto self = shared_from_this();
    app_window_->SetRenderCallback([self]() {
      if (self) {
        self->render();
      }
    });
  }

  void ComponentsWindow::render() {
    if (!i) {
      auto parent = Cherry::GetAppWindowByName(parent_name_);
      if (parent) {
        app_window_->SetParent(parent);
        app_window_->m_WindowRebuilded = false;
      }
      i = true;
    }

    if (!gs_id_loaded) {
      auto gs_id = TestCPP::get_session_link(id_);
      if (gs_id != "none") {
        gs_id_ = gs_id;
        gs_id_loaded = true;
      }
    }

    if (!drawer_session_) {
      auto gsv = TestCPP::get_session_variables(id_);
      if (gsv) {
        drawer_session_ = gsv;
      }
    }
    CherryKit::TitleFive("Hello");
  }

};  // namespace ModuleUI