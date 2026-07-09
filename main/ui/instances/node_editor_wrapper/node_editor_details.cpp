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
  static constexpr const char *kFlowType = "flow";

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

  static std::vector<std::string> FetchAssignableTypes(const std::string &context_id) {
    std::vector<std::string> types;

    nlohmann::json j;
    j["context_id"] = context_id;
    auto ret = CallIe("get_all_pin_formats", j.dump());
    const auto &rj = ret.get_json();
    if (!rj.contains("pin_formats") || !rj["pin_formats"].is_array())
      return types;

    for (const auto &pf_json : rj["pin_formats"]) {
      std::string type = pf_json.value("type", "");
      if (type.empty() || type == kFlowType)
        continue;
      types.push_back(type);
    }

    return types;
  }

  static int it = 1;
  DetailsWindow::DetailsWindow(const std::string &parent_name, const std::string &id, const std::string &st) {
    it++;
    app_window_ = std::make_shared<Cherry::AppWindow>("Details", "Details");
    id_ = id;
    parent_name_ = parent_name;
    storage_path_ = st;
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

  std::shared_ptr<DetailsWindow>
  DetailsWindow::create(const std::string &parent_name, const std::string &id, const std::string &st) {
    auto instance = std::shared_ptr<DetailsWindow>(new DetailsWindow(parent_name, id, st));
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

    if (!available_types_loaded_) {
      available_types_ = FetchAssignableTypes(kContextId);
      available_types_loaded_ = true;
    }

    if (!drawer_session_) {
      ImGui::TextDisabled("(no session)");
      return;
    }

    if (drawer_session_->selected_var.empty()) {
      ImGui::TextDisabled("(select a variable or a function)");
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

    std::string name = var.name;
    std::string def_val = var.default_value;

    static std::vector<std::string> types;
    static bool i = false;
    if (!i) {
      for (const auto &type : available_types_) {
        types.push_back(type);
      }
      i = true;
    }
    static bool te = false;

    CherryKit::TableSimple(
        "Details",
        { CherryKit::KeyValParent(
              "General",
              { CherryKit::KeyValString("Name", &name),
                CherryKit::KeyValComboString(CherryID("TYPEID"), "Type", &types),
                CherryKit::KeyValString("Default value", &def_val) }),
          CherryKit::KeyValParent("Todo", { CherryKit::KeyValBool("Read only", &te) }) });

    var.type = CherryApp.GetComponent(CherryID("TYPEID")).GetData("selected_string");
    var.name = name;
    var.default_value = def_val;

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