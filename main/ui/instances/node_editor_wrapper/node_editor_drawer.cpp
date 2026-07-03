#include <filesystem>
#include <iostream>
#include <string>

#include "../../../src/module.hpp"
#include "node_editor_wrapper.hpp"

namespace ModuleUI {

  static int it = 1;
  DrawerWindow::DrawerWindow() {
    it++;

    app_window_ = std::make_shared<Cherry::AppWindow>("Drawe", "Drawer");

    app_window_->SetDefaultBehavior(DefaultAppWindowBehaviors::DefaultDocking, "left");

    app_window_->m_CloseCallback = [=]() {
      Cherry::DeleteAppWindow(app_window_);
      app_window_->SetVisibility(false);
    };
    this->ctx = vxe::get_current_context();
  }  // namespace ModuleUI

  std::shared_ptr<Cherry::AppWindow> &DrawerWindow::get_app_window() {
    return app_window_;
  }

  std::shared_ptr<DrawerWindow> DrawerWindow::create() {
    auto instance = std::shared_ptr<DrawerWindow>(new DrawerWindow());
    instance->setup_render_callback();
    return instance;
  }

  void DrawerWindow::setup_render_callback() {
    auto self = shared_from_this();
    app_window_->SetRenderCallback([self]() {
      if (self) {
        self->render();
      }
    });
  }

  void DrawerWindow::render() {
    if (!i) {
      auto parent = Cherry::GetAppWindowByName("TEST");
      if (parent) {
        app_window_->SetParent(parent);
        app_window_->m_WindowRebuilded = false;
      }
      i = true;
    }

    if (CherryKit::ButtonText("Add sample var").GetDataAs<bool>("isClicked")) {
      auto call = [](const char *action, const std::string &json) {
        auto args = ArgumentValues(json);
        auto ret = ReturnValues();
        vxe::call_input_event("infinitehq.nodeedit", action, args, ret);
      };

      std::string session_id = gs_id_;

      auto json_escape = [](std::string s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
          if (c == '\\' || c == '"')
            out += '\\';
          out += c;
        }
        return out;
      };

      std::string session_id_escaped = json_escape(session_id);

      call(
          "setup_schema_to_graph_ext",
          R"({
    "session_id": ")" +
              session_id_escaped + R"(",
    "context_name": "testcpp",
    "id": "some_int_variable",
    "type": "simple",
    "label": "My new int",
    "label_color": "#FFFFFF",
    "status": "active",
    "border_color":        "#ebd934",
    "background_color":        "#ebd93433",
    "input_pins": [
    ],
    "output_pins": [
      { "id": "value", "name": "", "type": "int" }
    ],
    "spawnable": true,
    "spawn_possibility": {
      "category": "Utils",
      "proper_description": "",
      "proper_logo": "resources/icons/edit.png",
      "proper_name": "Sample var",
      "schema_id": "some_int_variable"
    }
  })");
    }
  }

};  // namespace ModuleUI
