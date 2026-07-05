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

  static constexpr const char *kContextId = "testcpp";
  static constexpr const char *kVarIdPrefix = "v_";
  static constexpr const char *kVarGetPrefix = "varget_";
  static constexpr const char *kVarSetPrefix = "varset_";

  static bool StartsWith(const std::string &s, const std::string &prefix) {
    return s.rfind(prefix, 0) == 0;
  }

  static std::string GenerateUniqueVariableId() {
    static std::atomic<uint64_t> s_counter{ 0 };
    uint64_t counter = s_counter.fetch_add(1, std::memory_order_relaxed);
    uint64_t ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

    std::ostringstream oss;
    oss << kVarIdPrefix << ns << "_" << counter;
    return oss.str();
  }

  static std::string DefaultValueForType(const std::string &type) {
    if (type == "bool")
      return "false";
    if (type == "int")
      return "0";
    if (type == "float")
      return "0.0";
    return "";
  }

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
      if (type.empty() || type == "flow")
        continue;
      types.push_back(type);
    }

    return types;
  }

  static std::vector<TestCPP::Variable> LoadVariablesFromFile(const std::string &storage_path) {
    std::vector<TestCPP::Variable> result;
    if (storage_path.empty())
      return result;

    std::filesystem::path p(storage_path);

    try {
      if (!std::filesystem::exists(p)) {
        if (p.has_parent_path())
          std::filesystem::create_directories(p.parent_path());

        nlohmann::json seed;
        seed["vars"] = nlohmann::json::array();

        std::ofstream out(p);
        if (out.is_open())
          out << seed.dump(2);

        return result;
      }

      std::ifstream in(p);
      if (!in.is_open()) {
        std::cerr << "[TestCPP] Could not open '" << storage_path << "' for reading." << std::endl;
        return result;
      }

      nlohmann::json j;
      in >> j;

      if (j.contains("vars") && j["vars"].is_array()) {
        for (const auto &vj : j["vars"]) {
          result.push_back(vj.get<TestCPP::Variable>());
        }
      }
    } catch (const std::exception &e) {
      std::cerr << "[TestCPP] Failed to load variables from '" << storage_path << "': " << e.what() << std::endl;
      result.clear();
    }

    return result;
  }

  static void SaveVariablesToFile(const std::string &storage_path, const std::vector<TestCPP::Variable> &vars) {
    if (storage_path.empty())
      return;

    std::filesystem::path p(storage_path);

    try {
      if (p.has_parent_path())
        std::filesystem::create_directories(p.parent_path());

      nlohmann::json j;
      j["vars"] = nlohmann::json::array();
      for (const auto &v : vars)
        j["vars"].push_back(v);

      std::ofstream out(p);
      if (!out.is_open()) {
        std::cerr << "[TestCPP] Could not open '" << storage_path << "' for writing." << std::endl;
        return;
      }
      out << j.dump(2);
    } catch (const std::exception &e) {
      std::cerr << "[TestCPP] Failed to save variables to '" << storage_path << "': " << e.what() << std::endl;
    }
  }

  namespace {
    struct ExistingVarEntry {
      std::string schema_id;
      std::string var_id;
      std::string label;
      std::string type;
      std::string default_value;
    };
  }  // namespace

  static void CollectExistingVarSchemas(
      const nlohmann::json &schemas_json,
      std::unordered_map<std::string, ExistingVarEntry> &getters,
      std::unordered_map<std::string, ExistingVarEntry> &setters) {
    for (const auto &schema_json : schemas_json) {
      std::string schema_id = schema_json.value("id", "");

      bool is_getter = StartsWith(schema_id, kVarGetPrefix);
      bool is_setter = StartsWith(schema_id, kVarSetPrefix);
      if (!is_getter && !is_setter)
        continue;

      ExistingVarEntry entry;
      entry.schema_id = schema_id;
      entry.var_id = is_getter ? schema_id.substr(std::string(kVarGetPrefix).size())
                               : schema_id.substr(std::string(kVarSetPrefix).size());
      entry.label = schema_json.value("label", "");

      if (is_getter) {
        if (schema_json.contains("output_pins") && schema_json["output_pins"].is_array() &&
            !schema_json["output_pins"].empty()) {
          entry.type = schema_json["output_pins"][0].value("type", "");
          entry.default_value = schema_json["output_pins"][0].value("default_value", "");
        }
        getters[entry.var_id] = std::move(entry);
      } else {
        if (schema_json.contains("input_pins") && schema_json["input_pins"].is_array()) {
          for (const auto &pin : schema_json["input_pins"]) {
            if (pin.value("id", "") == "value") {
              entry.type = pin.value("type", "");
              entry.default_value = pin.value("default_value", "");
              break;
            }
          }
        }
        setters[entry.var_id] = std::move(entry);
      }
    }
  }

  static void CreateGetterSchema(
      const std::string &session_id,
      const TestCPP::Variable &var,
      std::unordered_map<std::string, TestCPP::PinFormatInfo> &pin_format_cache) {
    const std::string getter_id = std::string(kVarGetPrefix) + var.id;

    const TestCPP::PinFormatInfo &pf = GetOrFetchPinFormat(pin_format_cache, kContextId, var.type);

    nlohmann::json sj;
    sj["session_id"] = session_id;
    sj["context_name"] = kContextId;
    sj["id"] = getter_id;
    sj["type"] = "simple";
    sj["label"] = var.name;
    sj["border_color"] = pf.color;
    sj["background_color"] = pf.color + "33";  // 33 is for opacity
    sj["status"] = "active";
    sj["input_pins"] = nlohmann::json::array();
    sj["output_pins"] = nlohmann::json::array(
        { { { "id", "value" }, { "name", "" }, { "type", var.type }, { "default_value", var.default_value } } });
    sj["spawnable"] = true;
    sj["spawn_possibility"] = {
      { "category", "Variables" },          { "proper_description", "" }, { "proper_logo", "resources/icons/edit.png" },
      { "proper_name", "Get " + var.name }, { "schema_id", getter_id },
    };

    CallIe("setup_schema_to_graph_ext", sj.dump());
  }

  static void CreateSetterSchema(
      const std::string &session_id,
      const TestCPP::Variable &var,
      std::unordered_map<std::string, TestCPP::PinFormatInfo> &pin_format_cache) {
    const std::string setter_id = std::string(kVarSetPrefix) + var.id;

    const TestCPP::PinFormatInfo &pf = GetOrFetchPinFormat(pin_format_cache, kContextId, var.type);

    nlohmann::json sj;
    sj["session_id"] = session_id;
    sj["context_name"] = kContextId;
    sj["id"] = setter_id;
    sj["type"] = "blueprint";
    sj["label"] = "Set " + var.name;
    sj["header_color"] = pf.color;
    sj["label_color"] = "#DEDEDE";
    sj["status"] = "active";
    sj["input_pins"] = nlohmann::json::array(
        {
            { { "id", "flow" }, { "name", "" }, { "type", "flow" } },
            { { "id", "value" }, { "name", var.name }, { "type", var.type }, { "default_value", var.default_value } },
        });
    sj["output_pins"] = nlohmann::json::array({ { { "id", "flow" }, { "name", "" }, { "type", "flow" } } });
    sj["spawnable"] = true;
    sj["spawn_possibility"] = {
      { "category", "Variables" },          { "proper_description", "" }, { "proper_logo", "resources/icons/edit.png" },
      { "proper_name", "Set " + var.name }, { "schema_id", setter_id },
    };

    CallIe("setup_schema_to_graph_ext", sj.dump());
  }

  static void DeleteVarSchema(const std::string &session_id, const std::string &schema_id) {
    nlohmann::json dj;
    dj["session_id"] = session_id;
    dj["schema_id"] = schema_id;
    CallIe("delete_graph_ext_schema", dj.dump());
  }

  static bool SyncVariablesToGraph(
      const std::string &session_id,
      const std::shared_ptr<TestCPP::DrawerSession> &session,
      std::unordered_map<std::string, TestCPP::PinFormatInfo> &pin_format_cache) {
    if (session_id.empty() || !session)
      return false;

    nlohmann::json j;
    j["session_id"] = session_id;
    auto ret = CallIe("get_all_ext_schemas", j.dump());
    const auto &rj = ret.get_json();

    std::unordered_map<std::string, ExistingVarEntry> existing_getters;
    std::unordered_map<std::string, ExistingVarEntry> existing_setters;
    if (rj.contains("schemas") && rj["schemas"].is_array()) {
      CollectExistingVarSchemas(rj["schemas"], existing_getters, existing_setters);
    }

    std::vector<std::string> assignable_types = FetchAssignableTypes(kContextId);

    std::unordered_map<std::string, bool> wanted_ids;
    bool changed = false;

    for (auto &var : session->vars) {
      if (!assignable_types.empty() &&
          std::find(assignable_types.begin(), assignable_types.end(), var.type) == assignable_types.end()) {
        var.type = assignable_types.front();
      }

      wanted_ids[var.id] = true;

      auto git = existing_getters.find(var.id);
      bool getter_matches = git != existing_getters.end() && git->second.label == var.name && git->second.type == var.type &&
                            git->second.default_value == var.default_value;

      auto sit = existing_setters.find(var.id);
      bool setter_matches = sit != existing_setters.end() && sit->second.label == ("Set " + var.name) &&
                            sit->second.type == var.type && sit->second.default_value == var.default_value;

      if (!getter_matches) {
        if (git != existing_getters.end())
          DeleteVarSchema(session_id, git->second.schema_id);
        CreateGetterSchema(session_id, var, pin_format_cache);
        changed = true;
      }
      if (!setter_matches) {
        if (sit != existing_setters.end())
          DeleteVarSchema(session_id, sit->second.schema_id);
        CreateSetterSchema(session_id, var, pin_format_cache);
        changed = true;
      }
    }

    for (const auto &[var_id, entry] : existing_getters) {
      if (wanted_ids.find(var_id) == wanted_ids.end()) {
        DeleteVarSchema(session_id, entry.schema_id);
        changed = true;
      }
    }
    for (const auto &[var_id, entry] : existing_setters) {
      if (wanted_ids.find(var_id) == wanted_ids.end()) {
        DeleteVarSchema(session_id, entry.schema_id);
        changed = true;
      }
    }

    if (changed) {
      nlohmann::json sj;
      sj["session_id"] = session_id;
      CallIe("save_nodegraph", sj.dump());
    }

    return changed;
  }

  static bool DrawCirclePlusButton(const ImVec2 &center, float radius) {
    ImVec2 min(center.x - radius, center.y - radius);

    ImGui::SetCursorScreenPos(min);
    ImGui::InvisibleButton("##circle_plus_btn", ImVec2(radius * 2.0f, radius * 2.0f));
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();

    ImU32 col = ImGui::GetColorU32(hovered ? ImGuiCol_Text : ImGuiCol_TextDisabled);
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    draw_list->AddCircle(center, radius, col, 16, 1.4f);

    float cross = radius * 0.55f;
    draw_list->AddLine(ImVec2(center.x - cross, center.y), ImVec2(center.x + cross, center.y), col, 1.4f);
    draw_list->AddLine(ImVec2(center.x, center.y - cross), ImVec2(center.x, center.y + cross), col, 1.4f);

    return clicked;
  }

  static bool VariablesCategoryHeader(const char *label, bool *p_open) {
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
      return false;

    ImGuiStyle &style = ImGui::GetStyle();
    ImGuiID id = window->GetID(label);

    const float full_width = ImGui::GetContentRegionAvail().x;
    const float line_height = ImGui::GetFrameHeight() * 1.3f;
    const float button_radius = line_height * 0.24f;

    ImVec2 pos = window->DC.CursorPos;
    ImRect header_bb(pos, ImVec2(pos.x + full_width, pos.y + line_height));

    bool add_clicked = false;

    ImGui::PushID(label);
    ImGui::ItemSize(ImVec2(full_width, line_height));
    if (!ImGui::ItemAdd(header_bb, id)) {
      ImGui::PopID();
      return false;
    }

    bool hovered, held;

    ImRect click_bb = header_bb;
    click_bb.Max.x -= (button_radius * 2.0f + style.ItemSpacing.x * 2.0f);
    bool pressed = ImGui::ButtonBehavior(click_bb, id, &hovered, &held);
    if (pressed)
      *p_open = !(*p_open);

    ImU32 bg_col = ImGui::GetColorU32(ImVec4(0.08f, 0.09f, 0.10f, 1.0f));
    ImGui::GetWindowDrawList()->AddRectFilled(header_bb.Min, header_bb.Max, bg_col, style.FrameRounding);
    if (hovered)
      ImGui::GetWindowDrawList()->AddRectFilled(
          header_bb.Min, header_bb.Max, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.04f)), style.FrameRounding);

    ImVec2 arrow_pos =
        ImVec2(header_bb.Min.x + style.FramePadding.x, header_bb.Min.y + (line_height - ImGui::GetTextLineHeight()) * 0.5f);
    ImGui::RenderArrow(
        ImGui::GetWindowDrawList(), arrow_pos, ImGui::GetColorU32(ImGuiCol_Text), *p_open ? ImGuiDir_Down : ImGuiDir_Right);

    std::string upper_label = label;
    std::transform(upper_label.begin(), upper_label.end(), upper_label.begin(), ::toupper);
    ImVec2 text_pos =
        ImVec2(arrow_pos.x + line_height * 0.7f, header_bb.Min.y + (line_height - ImGui::GetTextLineHeight()) * 0.5f);
    ImGui::GetWindowDrawList()->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), upper_label.c_str());

    ImVec2 circle_center =
        ImVec2(header_bb.Max.x - button_radius - style.ItemSpacing.x, header_bb.Min.y + line_height * 0.5f);
    if (DrawCirclePlusButton(circle_center, button_radius))
      add_clicked = true;

    ImGui::PopID();
    return add_clicked;
  }

  static void VariableRow(
      TestCPP::Variable &var,
      const std::shared_ptr<TestCPP::DrawerSession> &session,
      std::unordered_map<std::string, TestCPP::PinFormatInfo> &pin_format_cache) {
    ImGui::PushID(var.id.c_str());

    bool is_selected = (session->selected_var == var.id);
    const TestCPP::PinFormatInfo &pf = GetOrFetchPinFormat(pin_format_cache, kContextId, var.type);
    ImVec4 type_color = ParseHexColor(pf.color);

    const float row_height = ImGui::GetFrameHeight();

    ImVec2 row_start = ImGui::GetCursorScreenPos();
    float full_width = ImGui::GetContentRegionAvail().x;

    if (ImGui::Selectable("##row", is_selected, ImGuiSelectableFlags_AllowItemOverlap, ImVec2(full_width, row_height))) {
      session->selected_var = var.id;
    }
    ImGui::SetCursorScreenPos(row_start);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(var.name.c_str());

    const float type_w = ImGui::CalcTextSize(pf.name.c_str()).x;
    const float pill_w = row_height * 0.55f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;

    float right_block_w = pill_w + spacing + type_w;
    float cursor_x = row_start.x + full_width - right_block_w;
    ImGui::SetCursorScreenPos(ImVec2(cursor_x, row_start.y));

    ImVec2 pill_pos = ImGui::GetCursorScreenPos();
    float pill_h = row_height * 0.32f;
    ImVec2 pill_min = ImVec2(pill_pos.x, pill_pos.y + (row_height - pill_h) * 0.5f);
    ImVec2 pill_max = ImVec2(pill_min.x + pill_w, pill_min.y + pill_h);
    ImGui::GetWindowDrawList()->AddRectFilled(pill_min, pill_max, ImGui::ColorConvertFloat4ToU32(type_color), pill_h * 0.5f);
    ImGui::Dummy(ImVec2(pill_w, row_height));
    ImGui::SameLine(0.0f, spacing);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(pf.name.c_str());

    ImGui::PopID();
  }

  static void ShowVariablesPanel(const std::shared_ptr<TestCPP::DrawerSession> &session) {
    if (!session)
      return;

    static bool variables_open = true;

    ImGui::PushID("VariablesPanel");

    bool add_clicked = VariablesCategoryHeader("Variables", &variables_open);

    if (add_clicked) {
      TestCPP::Variable new_var;
      new_var.id = GenerateUniqueVariableId();
      new_var.name = "NewVar_" + std::to_string(session->vars.size());
      new_var.type = "bool";
      new_var.default_value = DefaultValueForType(new_var.type);

      session->vars.push_back(new_var);
      session->selected_var = new_var.id;
    }

    if (variables_open) {
      if (session->vars.empty()) {
        ImGui::TextDisabled("(aucune variable)");
      } else {
        for (auto &var : session->vars) {
          VariableRow(var, session, session->pin_format_cache);
        }
      }
    }

    ImGui::PopID();
  }

  static int it = 1;
  DrawerWindow::DrawerWindow(const std::string &parent_name, const std::string &id, const std::string &st) {
    it++;
    storage_path_ = st;
    parent_name_ = parent_name;
    id_ = id;
    app_window_ = std::make_shared<Cherry::AppWindow>("Drawe", "Drawer");

    app_window_->SetDefaultBehavior(DefaultAppWindowBehaviors::DefaultDocking, "left");

    app_window_->m_CloseCallback = [=]() {
      Cherry::DeleteAppWindow(app_window_);
      app_window_->SetVisibility(false);
    };
    this->ctx = vxe::get_current_context();
  }

  std::shared_ptr<Cherry::AppWindow> &DrawerWindow::get_app_window() {
    return app_window_;
  }

  std::shared_ptr<DrawerWindow>
  DrawerWindow::create(const std::string &parent_name, const std::string &id, const std::string &st) {
    auto instance = std::shared_ptr<DrawerWindow>(new DrawerWindow(parent_name, id, st));
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

    if (drawer_session_ && !vars_loaded_from_file_) {
      drawer_session_->vars = LoadVariablesFromFile(storage_path_);
      vars_loaded_from_file_ = true;
    }

    if (gs_id_loaded && vars_loaded_from_file_ && !initial_sync_done_ && drawer_session_) {
      SyncVariablesToGraph(gs_id_, drawer_session_, drawer_session_->pin_format_cache);
      initial_sync_done_ = true;
    }

    if (TestCPP::get_session_need_refresh(id_)) {
      TestCPP::set_session_need_refresh(id_, false);
      if (drawer_session_) {
        drawer_session_->vars = LoadVariablesFromFile(storage_path_);

        if (!drawer_session_->selected_var.empty()) {
          bool still_exists =
              std::any_of(drawer_session_->vars.begin(), drawer_session_->vars.end(), [&](const TestCPP::Variable &v) {
                return v.id == drawer_session_->selected_var;
              });
          if (!still_exists)
            drawer_session_->selected_var.clear();
        }

        SyncVariablesToGraph(gs_id_, drawer_session_, drawer_session_->pin_format_cache);
      }
    }

    if (TestCPP::get_session_need_save(id_)) {
      TestCPP::set_session_need_save(id_, false);
      if (drawer_session_) {
        SaveVariablesToFile(storage_path_, drawer_session_->vars);
        SyncVariablesToGraph(gs_id_, drawer_session_, drawer_session_->pin_format_cache);
      }
    }

    if (drawer_session_) {
      CherryStyle::RemoveMarginX(32.0f);
      ShowVariablesPanel(drawer_session_);
    }
  }

};  // namespace ModuleUI