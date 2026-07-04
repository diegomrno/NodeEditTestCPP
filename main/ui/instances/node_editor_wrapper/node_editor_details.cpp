#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "../../../src/module.hpp"
#include "node_editor_wrapper.hpp"

namespace ModuleUI {

  static constexpr const char *kContextId = "testcpp";

  static const std::vector<std::string> kVariableTypes = { "bool", "int", "float", "string" };

  static ReturnValues CallIe(const char *action, const std::string &json) {
    auto args = ArgumentValues(json);
    auto ret = ReturnValues();
    vxe::call_input_event("infinitehq.nodeedit", action, args, ret);
    return ret;
  }

  static ImVec4 ParseHexColor(const std::string &hex, const ImVec4 &fallback = ImVec4(0.55f, 0.55f, 0.55f, 1.0f)) {
    std::string h = hex;
    if (!h.empty() && h[0] == '#')
      h = h.substr(1);
    if (h.size() != 6 && h.size() != 8)
      return fallback;

    try {
      auto hex_to_float = [](const std::string &s) { return static_cast<float>(std::stoul(s, nullptr, 16)) / 255.0f; };
      float r = hex_to_float(h.substr(0, 2));
      float g = hex_to_float(h.substr(2, 2));
      float b = hex_to_float(h.substr(4, 2));
      float a = h.size() == 8 ? hex_to_float(h.substr(6, 2)) : 1.0f;
      return ImVec4(r, g, b, a);
    } catch (...) {
      return fallback;
    }
  }

  static const TestCPP::PinFormatInfo &GetOrFetchPinFormat(
      std::unordered_map<std::string, TestCPP::PinFormatInfo> &cache,
      const std::string &context_id,
      const std::string &type) {
    auto it = cache.find(type);
    if (it != cache.end())
      return it->second;

    TestCPP::PinFormatInfo info;
    info.name = type;
    info.color = "#8C8C8C";

    nlohmann::json j;
    j["context_id"] = context_id;
    j["type"] = type;
    auto ret = CallIe("get_pin_format", j.dump());
    const auto &rj = ret.get_json();
    if (!rj.empty() && rj.contains("name")) {
      info.name = rj.value("name", type);
      info.color = rj.value("color", info.color);
    }

    auto [inserted_it, ok] = cache.emplace(type, std::move(info));
    return inserted_it->second;
  }

  static int it = 1;
  DetailsWindow::DetailsWindow(const std::string &parent_name, const std::string &id) {
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
  }

  std::shared_ptr<Cherry::AppWindow> &DetailsWindow::get_app_window() {
    return app_window_;
  }

  std::shared_ptr<DetailsWindow> DetailsWindow::create(const std::string &parent_name, const std::string &id) {
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

    if (!drawer_session_) {
      auto gsv = TestCPP::get_session_variables(id_);
      if (gsv) {
        drawer_session_ = gsv;
      }
    }

    if (!drawer_session_) {
      ImGui::TextDisabled("(no session)");
      return;
    }

    if (drawer_session_->selected_var.empty()) {
      ImGui::TextDisabled("(select a variable)");
      return;
    }

    auto it = std::find_if(drawer_session_->vars.begin(), drawer_session_->vars.end(), [&](const TestCPP::Variable &v) {
      return v.id == drawer_session_->selected_var;
    });

    if (it == drawer_session_->vars.end()) {
      ImGui::TextDisabled("(variable not found)");
      return;
    }

    TestCPP::Variable &var = *it;

    CherryKit::TitleOne(var.name);

    {
      char buf[256];
      std::snprintf(buf, sizeof(buf), "%s", var.name.c_str());
      if (ImGui::InputText("Name", buf, sizeof(buf))) {
        var.name = buf;
      }
    }

    {
      const TestCPP::PinFormatInfo &current_pf =
          GetOrFetchPinFormat(drawer_session_->pin_format_cache, kContextId, var.type);

      if (ImGui::BeginCombo("Type", current_pf.name.c_str())) {
        for (const auto &type : kVariableTypes) {
          const TestCPP::PinFormatInfo &pf = GetOrFetchPinFormat(drawer_session_->pin_format_cache, kContextId, type);
          bool is_selected = (type == var.type);

          ImGui::PushStyleColor(ImGuiCol_Text, ParseHexColor(pf.color));
          bool clicked = ImGui::Selectable(pf.name.c_str(), is_selected);
          ImGui::PopStyleColor();

          if (clicked) {
            var.type = type;
          }
          if (is_selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
    }

    {
      char buf[256];
      std::snprintf(buf, sizeof(buf), "%s", var.value.c_str());
      if (ImGui::InputText("Value", buf, sizeof(buf))) {
        var.value = buf;
      }
    }

    ImGui::Spacing();
    if (CherryKit::ButtonText("Delete variable").GetDataAs<bool>("isClicked")) {
      std::string deleted_id = var.id;
      drawer_session_->vars.erase(
          std::remove_if(
              drawer_session_->vars.begin(),
              drawer_session_->vars.end(),
              [&](const TestCPP::Variable &v) { return v.id == deleted_id; }),
          drawer_session_->vars.end());

      if (drawer_session_->selected_var == deleted_id) {
        drawer_session_->selected_var.clear();
      }
    }
  }

};  // namespace ModuleUI