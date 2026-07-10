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

    if (drawer_session_->selected_var.empty() && drawer_session_->selected_function.empty()) {
      ImGui::TextDisabled("(select a variable or a function)");
      return;
    }
    // Function details
    if (!drawer_session_->selected_function.empty()) {
      auto it = std::find_if(
          drawer_session_->functions.begin(), drawer_session_->functions.end(), [&](const TestCPP::Function &v) {
            return v.id == drawer_session_->selected_function;
          });

      if (it == drawer_session_->functions.end()) {
        ImGui::TextDisabled("(function not found)");
        return;
      }

      TestCPP::Function &f = *it;
      std::string name = f.name;

      std::vector<Cherry::Component> inputRows;
      std::string functionId = f.id;
      for (int index = 0; index < (int)f.inputs.size(); ++index) {
        inputRows.push_back(
            CherryKit::KeyValCustom(
                f.inputs[index].name.empty() ? "(unnamed)" : f.inputs[index].name, [&, functionId, index]() {
                  auto it = std::find_if(
                      drawer_session_->functions.begin(), drawer_session_->functions.end(), [&](const TestCPP::Function &f) {
                        return f.id == functionId;
                      });

                  if (it == drawer_session_->functions.end())
                    return;

                  TestCPP::Function &foo = *it;

                  ImGui::PushID(("fn_in_" + std::to_string(index)).c_str());

                  char nameBuf[128];
                  snprintf(nameBuf, sizeof(nameBuf), "%s", foo.inputs[index].name.c_str());
                  ImGui::SetNextItemWidth(140.0f);
                  if (ImGui::InputText("##pin_name", nameBuf, sizeof(nameBuf)))
                    foo.inputs[index].name = nameBuf;

                  ImGui::SameLine();

                  int selectedType = 0;
                  for (int t = 0; t < (int)available_types_.size(); ++t) {
                    if (available_types_[t] == foo.inputs[index].type) {
                      selectedType = t;
                      break;
                    }
                  }

                  const char *preview = available_types_.empty() ? "" : available_types_[selectedType].c_str();
                  ImGui::SetNextItemWidth(110.0f);
                  if (ImGui::BeginCombo("##pin_type", preview)) {
                    for (int t = 0; t < (int)available_types_.size(); ++t) {
                      bool selected = (selectedType == t);
                      if (ImGui::Selectable(available_types_[t].c_str(), selected)) {
                        foo.inputs[index].type = available_types_[t];
                        // switching type invalidates a stale default written for a different type
                        foo.inputs[index].default_value = TestCPP::DefaultLiteralValueForType(available_types_[t]);
                      }
                      if (selected)
                        ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                  }

                  ImGui::SameLine();

                  char defBuf[128];
                  snprintf(defBuf, sizeof(defBuf), "%s", foo.inputs[index].default_value.c_str());
                  ImGui::SetNextItemWidth(90.0f);
                  if (ImGui::InputTextWithHint("##pin_default", "default", defBuf, sizeof(defBuf)))
                    foo.inputs[index].default_value = defBuf;

                  ImGui::SameLine();
                  if (ImGui::SmallButton("X")) {
                    foo.inputs.erase(foo.inputs.begin() + index);
                    ImGui::PopID();
                    return;
                  }

                  ImGui::PopID();
                }));
      }

      inputRows.push_back(CherryKit::KeyValCustom("", [&]() {
        if (CherryKit::ButtonText("+ Add Input").GetDataAs<bool>("isClicked")) {
          std::string defaultType = available_types_.empty() ? "" : available_types_[0];
          TestCPP::FunctionPin pin;
          pin.type = defaultType;
          pin.default_value = TestCPP::DefaultLiteralValueForType(defaultType);
          f.inputs.push_back(std::move(pin));
        }
      }));

      std::vector<Cherry::Component> outputRows;
      for (int index = 0; index < (int)f.outputs.size(); ++index) {
        outputRows.push_back(
            CherryKit::KeyValCustom(
                f.outputs[index].name.empty() ? "(unnamed)" : f.outputs[index].name, [&, functionId, index]() {
                  auto it = std::find_if(
                      drawer_session_->functions.begin(), drawer_session_->functions.end(), [&](const TestCPP::Function &f) {
                        return f.id == functionId;
                      });

                  if (it == drawer_session_->functions.end())
                    return;

                  TestCPP::Function &foo = *it;
                  ImGui::PushID(("fn_out_" + std::to_string(index)).c_str());

                  char nameBuf[128];
                  snprintf(nameBuf, sizeof(nameBuf), "%s", foo.outputs[index].name.c_str());
                  ImGui::SetNextItemWidth(140.0f);
                  if (ImGui::InputText("##pin_name", nameBuf, sizeof(nameBuf)))
                    foo.outputs[index].name = nameBuf;

                  ImGui::SameLine();

                  int selectedType = 0;
                  for (int t = 0; t < (int)available_types_.size(); ++t) {
                    if (available_types_[t] == foo.outputs[index].type) {
                      selectedType = t;
                      break;
                    }
                  }

                  const char *preview = available_types_.empty() ? "" : available_types_[selectedType].c_str();
                  ImGui::SetNextItemWidth(110.0f);
                  if (ImGui::BeginCombo("##pin_type", preview)) {
                    for (int t = 0; t < (int)available_types_.size(); ++t) {
                      bool selected = (selectedType == t);
                      if (ImGui::Selectable(available_types_[t].c_str(), selected)) {
                        foo.outputs[index].type = available_types_[t];
                        foo.outputs[index].default_value = TestCPP::DefaultLiteralValueForType(available_types_[t]);
                      }
                      if (selected)
                        ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                  }

                  ImGui::SameLine();

                  char defBuf[128];
                  snprintf(defBuf, sizeof(defBuf), "%s", foo.outputs[index].default_value.c_str());
                  ImGui::SetNextItemWidth(90.0f);
                  if (ImGui::InputTextWithHint("##pin_default", "default", defBuf, sizeof(defBuf)))
                    foo.outputs[index].default_value = defBuf;

                  ImGui::SameLine();
                  if (ImGui::SmallButton("X")) {
                    foo.outputs.erase(foo.outputs.begin() + index);
                    ImGui::PopID();
                    return;
                  }

                  ImGui::PopID();
                }));
      }

      outputRows.push_back(CherryKit::KeyValCustom("", [&]() {
        if (CherryKit::ButtonText("+ Add Output").GetDataAs<bool>("isClicked")) {
          std::string defaultType = available_types_.empty() ? "" : available_types_[0];
          TestCPP::FunctionPin pin;
          pin.type = defaultType;
          pin.default_value = TestCPP::DefaultLiteralValueForType(defaultType);
          f.outputs.push_back(std::move(pin));
        }
      }));

      CherryKit::TableSimple(
          "Function Details",
          {
              CherryKit::KeyValParent(
                  "General",
                  {
                      CherryKit::KeyValString("Name", &name),
                  }),

              CherryKit::KeyValParent("Inputs", inputRows),

              CherryKit::KeyValParent("Outputs", outputRows),

              CherryKit::KeyValParent(
                  "Actions",
                  {
                      CherryKit::KeyValCustom(
                          "",
                          [&]() {
                            if (CherryKit::ButtonText("Delete Function").GetDataAs<bool>("isClicked")) {
                              std::string deleted_id = f.id;

                              drawer_session_->functions.erase(
                                  std::remove_if(
                                      drawer_session_->functions.begin(),
                                      drawer_session_->functions.end(),
                                      [&](const TestCPP::Function &v) { return v.id == deleted_id; }),
                                  drawer_session_->functions.end());

                              if (drawer_session_->selected_function == deleted_id)
                                drawer_session_->selected_function.clear();
                            }
                          }),
                  }),
          });

      f.name = name;
    }

    // Variable details
    if (!drawer_session_->selected_var.empty()) {
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
          "Details Variables",
          { CherryKit::KeyValParent(
                "General var",
                { CherryKit::KeyValString("Name", &name),
                  CherryKit::KeyValComboString(CherryID("TYPEID"), "Type", &types),
                  CherryKit::KeyValString("Default value", &def_val) }),
            CherryKit::KeyValParent("TEST", { CherryKit::KeyValBool("Read only", &te) }) });

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
  }

};  // namespace ModuleUI