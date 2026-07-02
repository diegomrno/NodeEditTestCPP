#include "node_editor_wrapper.hpp"

#include <filesystem>
#include <iostream>
#include <string>

#include "../../../src/module.hpp"

namespace ModuleUI {
  TestCppWrapperAppWindow::TestCppWrapperAppWindow() {
    app_window_ = std::make_shared<Cherry::AppWindow>("TEST", "TEST");
    app_window_->SetDockingMode(true);

    app_window_->SetClosable(true);

    app_window_->SetInternalPaddingX(0.0f);
    app_window_->SetInternalPaddingY(0.0f);

    app_window_->SetLeftMenubarCallback([this]() {
      CherryNextComponent.SetProperty("padding_y", "6.0f");
      CherryNextComponent.SetProperty("padding_x", "10.0f");
      if (CherryKit::ButtonImageText("Save", TestCPP::get_path("/resources/icons/icon_save.png"))
              .GetDataAs<bool>("isClicked")) {
        nlohmann::json j;
        j["session_id"] = graph_session_id_;
        auto args = ArgumentValues(j.dump());
        auto ret = ReturnValues();
        vxe::call_input_event("infinitehq.TestCpp", "save_nodegraph", args, ret);
      }

      CherryNextComponent.SetProperty("padding_y", "6.0f");
      CherryNextComponent.SetProperty("padding_x", "10.0f");
      if (CherryKit::ButtonImageText("Refresh", TestCPP::get_path("/resources/icons/icon_refresh.png"))
              .GetDataAs<bool>("isClicked")) {
        nlohmann::json j;
        j["session_id"] = graph_session_id_;
        auto args = ArgumentValues(j.dump());
        auto ret = ReturnValues();
        vxe::call_input_event("infinitehq.TestCpp", "refresh_nodegraph", args, ret);
      }

      CherryNextComponent.SetProperty("padding_y", "6.0f");
      CherryNextComponent.SetProperty("padding_x", "10.0f");
      if (CherryKit::ButtonImageText("Compile", TestCPP::get_path("/resources/icons/icon_refresh.png"))
              .GetDataAs<bool>("isClicked")) {
        nlohmann::json j;
        j["session_id"] = graph_session_id_;
        auto args = ArgumentValues(j.dump());
        auto ret = ReturnValues();
        vxe::call_input_event("infinitehq.TestCpp", "refresh_nodegraph", args, ret);
      }

      CherryNextComponent.SetProperty("padding_y", "6.0f");
      CherryNextComponent.SetProperty("padding_x", "10.0f");
      if (CherryKit::ButtonImageText("Run", TestCPP::get_path("/resources/icons/icon_refresh.png"))
              .GetDataAs<bool>("isClicked")) {
        nlohmann::json j;
        j["session_id"] = graph_session_id_;
        auto args = ArgumentValues(j.dump());
        auto ret = ReturnValues();
        vxe::call_input_event("infinitehq.TestCpp", "refresh_nodegraph", args, ret);
      }
    });

    app_window_->m_CloseCallback = [=]() {
      Cherry::DeleteAppWindow(app_window_);
      app_window_->SetVisibility(false);
    };
    this->ctx = vxe::get_current_context();
  }  // namespace ModuleUI

  std::shared_ptr<Cherry::AppWindow>& TestCppWrapperAppWindow::get_app_window() {
    return app_window_;
  }

  std::shared_ptr<TestCppWrapperAppWindow> TestCppWrapperAppWindow::create() {
    auto instance = std::shared_ptr<TestCppWrapperAppWindow>(new TestCppWrapperAppWindow());
    return instance;
  }

};  // namespace ModuleUI
