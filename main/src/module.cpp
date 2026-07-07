#include "module.hpp"

// runtime pointer
#ifndef CTestCPP
std::weak_ptr<TestCPP::Context> CTestCPP;
#endif

std::shared_ptr<TestCPP::Context> TestCPP::create_context() {
  auto ctx = std::make_shared<TestCPP::Context>();

  set_current_context(ctx);

  return ctx;
}

void TestCPP::destroy_context(std::shared_ptr<TestCPP::Context> ctx) {
  set_current_context(nullptr);
}

void TestCPP::set_current_context(std::shared_ptr<TestCPP::Context> ctx) {
  CTestCPP = ctx;
}

std::shared_ptr<TestCPP::Context> TestCPP::get_current_context() {
  return CTestCPP.lock();
}

std::string TestCPP::get_path(const std::string &path) {
  return get_current_context()->interface->cook_path(path);
}

bool TestCPP::is_cpp_sketch(const std::string &path) {
  std::string fullpath = path + "/cpp_sketch.ng";
  std::string graph = fullpath + "/graph.nodegraph";
  return fs::exists(fullpath) && fs::exists(graph);
}

static std::vector<std::shared_ptr<ModuleUI::TestCppWrapperAppWindow>> s_instances;
void TestCPP::open_cpp_sketch(const std::string &path) {
  std::string id;
  fs::path full_path(path);
  std::string graph_path = path + "/cpp_sketch.ng";
  std::string storage_path = path + "/storage.json";

  int session_id = ++i_session;
  TestCPP::set_session_link(std::to_string(session_id), "none");
  TestCPP::set_session_variables(std::to_string(session_id), std::make_shared<DrawerSession>());

  auto inst = ModuleUI::TestCppWrapperAppWindow::create(full_path.filename(), std::to_string(i_session), storage_path);
  Cherry::AddAppWindow(inst->get_app_window());
  s_instances.push_back(inst);

  {
    auto i = ModuleUI::DetailsWindow::create(full_path.filename(), std::to_string(i_session), storage_path);
    Cherry::AddAppWindow(i->get_app_window());
  }
  {
    auto i = ModuleUI::DrawerWindow::create(full_path.filename(), std::to_string(i_session), storage_path);
    Cherry::AddAppWindow(i->get_app_window());
  }

  {
    auto i = ModuleUI::ComponentsWindow::create(full_path.filename(), std::to_string(i_session), storage_path);
    Cherry::AddAppWindow(i->get_app_window());
  }

  {
    nlohmann::json j;
    j["path"] = graph_path;
    j["disable_native_saving_system"] = true;
    j["graph_title"] = "C++ Simple Program";
    j["parent_appwindow"] = full_path.filename();
    j["logo_path"] = TestCPP::get_path("resources/icons/icon_magnifying_glass.png");

    auto ret = ReturnValues();
    auto args = ArgumentValues(j.dump());
    vxe::call_input_event("infinitehq.nodeedit", "open_graph", args, ret);

    id = ret.get_json()["session_id"];
    std::cout << id << std::endl;
    TestCPP::set_session_link(std::to_string(i_session), id);

    inst->set_instance_id(id);
  }

  {
    // nlohmann::json j;
    // j["session_id"] = id;
    // auto args = ArgumentValues(j.dump());
    // auto ret = ReturnValues();
    // vxe::call_input_event("infinitehq.nodeedit", "refresh_nodegraph", args, ret);
  }
}

void TestCPP::set_session_link(const std::string &session_id, const std::string &graph_id) {
  auto ctx = get_current_context();
  if (!ctx)
    return;
  ctx->session_links[session_id] = graph_id;
}

std::string TestCPP::get_session_link(const std::string &session_id) {
  auto ctx = get_current_context();
  if (!ctx)
    return "";
  auto it = ctx->session_links.find(session_id);
  if (it == ctx->session_links.end())
    return "";
  return it->second;
}

void TestCPP::set_session_variables(
    const std::string &session_id,
    const std::shared_ptr<TestCPP::DrawerSession> &variables) {
  auto ctx = get_current_context();
  if (!ctx)
    return;
  ctx->session_variables[session_id] = variables;
}

void TestCPP::set_session_need_refresh(const std::string &id, bool v) {
  auto ctx = get_current_context();
  if (!ctx)
    return;
  ctx->session_need_refresh[id] = v;
}

bool TestCPP::get_session_need_refresh(const std::string &id) {
  auto ctx = get_current_context();
  if (!ctx)
    return false;
  auto it = ctx->session_need_refresh.find(id);
  if (it == ctx->session_need_refresh.end())
    return false;
  return it->second;
}

void TestCPP::set_session_need_save(const std::string &id, bool v) {
  auto ctx = get_current_context();
  if (!ctx)
    return;
  ctx->session_need_save[id] = v;
}

bool TestCPP::get_session_need_save(const std::string &id) {
  auto ctx = get_current_context();
  if (!ctx)
    return "";
  auto it = ctx->session_need_save.find(id);
  if (it == ctx->session_need_save.end())
    return "";
  return it->second;
}

std::shared_ptr<TestCPP::DrawerSession> TestCPP::get_session_variables(const std::string &session_id) {
  auto ctx = get_current_context();
  if (!ctx)
    return nullptr;
  auto it = ctx->session_variables.find(session_id);
  if (it == ctx->session_variables.end())
    return nullptr;
  return it->second;
}

void TestCPP::setup_graph_ctx() {
  auto call = [](const char *action, const std::string &json) {
    auto args = ArgumentValues(json);
    auto ret = ReturnValues();
    vxe::call_input_event("infinitehq.nodeedit", action, args, ret);
  };

  std::string foo_logo_path = TestCPP::get_path("resources/base/foo.png");
  std::string event_logo_path = TestCPP::get_path("resources/base/event.png");
  std::string if_logo_path = TestCPP::get_path("resources/base/if.png");

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

  std::string foo_logo_path_escaped = json_escape(foo_logo_path);
  std::string event_logo_path_escaped = json_escape(event_logo_path);
  std::string if_logo_path_escaped = json_escape(if_logo_path);

  call("create_node_context", R"({
    "name": "testcpp"
  })");

  // setup pins
  call("setup_pin_format", R"({
    "context_name": "testcpp",
    "type":         "flow",
    "name":         "Flow",
    "color":        "#d4d4d4",
    "shape":        "flow",
    "description":  "Flow pin"
  })");

  call("setup_pin_format", R"({
    "context_name": "testcpp",
    "type":         "bool",
    "name":         "Boolean",
    "color":        "#eb3461",
    "shape":        "circle",
    "description":  "Simple boolean"
  })");

  call("setup_pin_format", R"({
    "context_name": "testcpp",
    "type":         "int",
    "name":         "Integer",
    "color":        "#ebd934",
    "shape":        "circle",
    "description":  "Simple integer"
  })");

  call("setup_pin_format", R"({
    "context_name": "testcpp",
    "type":         "float",
    "name":         "Floating number",
    "color":        "#c0eb34",
    "shape":        "circle",
    "description":  "Simple floating number"
  })");

  call("setup_pin_format", R"({
    "context_name": "testcpp",
    "type":         "string",
    "name":         "String",
    "color":        "#d034eb",
    "shape":        "circle",
    "description":  "Simple string"
  })");

  // setup schemas
  call(
      "setup_schema",
      R"({
    "context_name": "testcpp",
    "id": "console_output",
    "type": "blueprint",
    "status": "active",
    "second_label": "Print message to console",
    "second_label_color": "#8a88f2",
    "label": "Console output",
    "label_color": "#ABABAB",
    "header_color": "#3a4bab",
    "header_logo_path": ")" +
          foo_logo_path_escaped + R"(",
    "input_pins": [
      { "id": "input_flow", "name": "", "type": "flow" },
      { "id": "string", "name": "Message", "type": "string" }
    ],
    "output_pins": [
      { "id": "output_flow", "name": "", "type": "flow" }
    ],
    "spawnable": true,
    "spawn_possibility": {
      "category": "Functions",
      "proper_description": "Console output node",
      "proper_logo": "resources/icons/edit.png",
      "proper_name": "Console output",
      "schema_id": "console_output"
    }
  })");

  call(
      "setup_schema",
      R"({
    "context_name": "testcpp",
    "id": "sleep",
    "type": "blueprint",
    "status": "active",
    "second_label": "Sleep for a while",
    "second_label_color": "#8a88f2",
    "label": "Sleep",
    "label_color": "#ABABAB",
    "header_color": "#3a4bab",
    "header_logo_path": ")" +
          foo_logo_path_escaped + R"(",
    "input_pins": [
      { "id": "input_flow", "name": "", "type": "flow" },
      { "id": "int", "name": "Seconds", "type": "int" }
    ],
    "output_pins": [
      { "id": "output_flow", "name": "", "type": "flow" }
    ],
    "spawnable": true,
    "spawn_possibility": {
      "category": "Functions",
      "proper_description": "Sleep for a while",
      "proper_logo": "resources/icons/edit.png",
      "proper_name": "Sleep",
      "schema_id": "sleep"
    }
  })");

  call(
      "setup_schema",
      R"({
    "context_name": "testcpp",
    "id": "close",
    "type": "blueprint",
    "status": "active",
    "second_label": "Stop and close the program",
    "second_label_color": "#8a88f2",
    "label": "Stop",
    "label_color": "#ABABAB",
    "header_color": "#3a4bab",
    "header_logo_path": ")" +
          foo_logo_path_escaped + R"(",
    "input_pins": [
      { "id": "input_flow", "name": "", "type": "flow" }
    ],
    "output_pins": [
    ],
    "spawnable": true,
    "spawn_possibility": {
      "category": "Functions",
      "proper_description": "Close and stop the program",
      "proper_logo": "resources/icons/edit.png",
      "proper_name": "Close",
      "schema_id": "close"
    }
  })");

  call(
      "setup_schema",
      R"({
    "context_name": "testcpp",
    "id": "on_setup",
    "type": "blueprint",
    "status": "active",
    "second_label": "Start when the program begins",
    "second_label_color": "#cf8a8a",
    "label": "OnSetup",
    "label_color": "#ABABAB",
    "header_color": "#ab3a45",
    "header_logo_path": ")" +
          event_logo_path_escaped + R"(",
    "input_pins": [
    ],
    "output_pins": [
      { "id": "output_flow", "name": "", "type": "flow" }
    ],
    "spawnable": true,
    "spawn_possibility": {
      "category": "Events",
      "proper_description": "Triggered when the program debute",
      "proper_logo": "resources/icons/edit.png",
      "proper_name": "On begin",
      "schema_id": "on_setup"
    }
  })");

  call(
      "setup_schema",
      R"({
    "context_name": "testcpp",
    "id": "on_loop",
    "type": "blueprint",
    "status": "active",
    "second_label": "Triggered every ticks",
    "second_label_color": "#cf8a8a",
    "label": "OnLoop",
    "label_color": "#ABABAB",
    "header_color": "#ab3a45",
    "header_logo_path": ")" +
          event_logo_path_escaped + R"(",
    "input_pins": [
    ],
    "output_pins": [
      { "id": "output_flow", "name": "", "type": "flow" }
    ],
    "spawnable": true,
    "spawn_possibility": {
      "category": "Events",
      "proper_description": "Triggered every ticks",
      "proper_logo": "resources/icons/edit.png",
      "proper_name": "On Loop",
      "schema_id": "on_loop"
    }
  })");

  call(
      "setup_schema",
      R"({
    "context_name": "testcpp",
    "id": "branch",
    "type": "blueprint",
    "status": "active",
    "label": "Branch",
    "label_color": "#9A9A9A",
    "header_color": "#595959",
    "header_logo_path": ")" +
          if_logo_path_escaped + R"(",
    "input_pins": [
      { "id": "input_flow", "name": "", "type": "flow" },
      { "id": "bool", "name": "Condition", "type": "bool" }
    ],
    "output_pins": [
      { "id": "output_true", "name": "True", "type": "flow" },
      { "id": "output_false", "name": "False", "type": "flow" }
    ],
    "spawnable": true,
    "spawn_possibility": {
      "category": "Flow",
      "proper_description": "Simple if then else condition",
      "proper_logo": "resources/icons/edit.png",
      "proper_name": "Branch",
      "schema_id": "branch"
    }
  })");

  call(
      "setup_schema",
      R"({
    "context_name": "testcpp",
    "id": "is_int_higher_than_int",
    "type": "simple",
    "label": ">",
    "status": "active",
    "input_pins": [
      { "id": "int1", "name": "", "type": "int" },
      { "id": "int2", "name": "", "type": "int" }
    ],
    "output_pins": [
      { "id": "bool", "name": "", "type": "bool" }
    ],
    "spawnable": true,
    "spawn_possibility": {
      "category": "Utils",
      "proper_description": "",
      "proper_logo": "resources/icons/edit.png",
      "proper_name": "Is Int higher than Int",
      "schema_id": "is_int_higher_than_int"
    }
  })");

  auto setup_simple_math = [&](const std::string &id,
                               const std::string &label,
                               const std::vector<std::pair<std::string, std::string>> &inputs,
                               const std::string &out_type,
                               const std::string &proper_name) {
    std::string in_json;
    for (auto &[pin_id, pin_type] : inputs) {
      in_json += R"({ "id": ")" + pin_id + R"(", "name": "", "type": ")" + pin_type + R"(" },)";
    }
    if (!in_json.empty())
      in_json.pop_back();

    call(
        "setup_schema",
        R"({
    "context_name": "testcpp",
    "id": ")" +
            id + R"(",
    "type": "simple",
    "label": ")" +
            label + R"(",
    "status": "active",
    "input_pins": [ )" +
            in_json + R"( ],
    "output_pins": [
      { "id": "out", "name": "", "type": ")" +
            out_type + R"(" }
    ],
    "spawnable": true,
    "spawn_possibility": {
      "category": "Utils",
      "proper_description": "",
      "proper_logo": "resources/icons/edit.png",
      "proper_name": ")" +
            proper_name + R"(",
      "schema_id": ")" +
            id + R"("
    }
  })");
  };

  setup_simple_math("add_int", "+", { { "a", "int" }, { "b", "int" } }, "int", "Add (Int)");
  setup_simple_math("subtract_int", "-", { { "a", "int" }, { "b", "int" } }, "int", "Subtract (Int)");
  setup_simple_math("multiply_int", "*", { { "a", "int" }, { "b", "int" } }, "int", "Multiply (Int)");
  setup_simple_math("divide_int", "/", { { "a", "int" }, { "b", "int" } }, "int", "Divide (Int)");
  setup_simple_math("min_int", "Min", { { "a", "int" }, { "b", "int" } }, "int", "Min (Int)");
  setup_simple_math("max_int", "Max", { { "a", "int" }, { "b", "int" } }, "int", "Max (Int)");
  setup_simple_math("clamp_int", "Clamp", { { "value", "int" }, { "min", "int" }, { "max", "int" } }, "int", "Clamp (Int)");
  setup_simple_math("abs_int", "Abs", { { "a", "int" } }, "int", "Abs (Int)");
  setup_simple_math("random_int", "Random", { { "min", "int" }, { "max", "int" } }, "int", "Random Int");

  setup_simple_math("add_float", "+", { { "a", "float" }, { "b", "float" } }, "float", "Add (Float)");
  setup_simple_math("subtract_float", "-", { { "a", "float" }, { "b", "float" } }, "float", "Subtract (Float)");
  setup_simple_math("multiply_float", "*", { { "a", "float" }, { "b", "float" } }, "float", "Multiply (Float)");
  setup_simple_math("divide_float", "/", { { "a", "float" }, { "b", "float" } }, "float", "Divide (Float)");
  setup_simple_math("min_float", "Min", { { "a", "float" }, { "b", "float" } }, "float", "Min (Float)");
  setup_simple_math("max_float", "Max", { { "a", "float" }, { "b", "float" } }, "float", "Max (Float)");
  setup_simple_math(
      "clamp_float", "Clamp", { { "value", "float" }, { "min", "float" }, { "max", "float" } }, "float", "Clamp (Float)");
  setup_simple_math("abs_float", "Abs", { { "a", "float" } }, "float", "Abs (Float)");
  setup_simple_math("round_float", "Round", { { "a", "float" } }, "int", "Round");
  setup_simple_math("floor_float", "Floor", { { "a", "float" } }, "int", "Floor");
  setup_simple_math("ceil_float", "Ceil", { { "a", "float" } }, "int", "Ceil");
  setup_simple_math("random_float", "Random", { { "min", "float" }, { "max", "float" } }, "float", "Random Float");

  setup_simple_math("int_to_string", "To String", { { "a", "int" } }, "string", "Int To String");
  setup_simple_math("float_to_string", "To String", { { "a", "float" } }, "string", "Float To String");
  setup_simple_math("bool_to_string", "To String", { { "a", "bool" } }, "string", "Bool To String");
  setup_simple_math("string_to_int", "To Int", { { "a", "string" } }, "int", "String To Int");
  setup_simple_math("string_to_float", "To Float", { { "a", "string" } }, "float", "String To Float");
}

std::vector<TestCPP::Variable> TestCPP::load_variables_from_file(const std::string &storage_path) {
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
    if (!in.is_open())
      return result;

    nlohmann::json j;
    in >> j;

    if (j.contains("vars") && j["vars"].is_array())
      for (const auto &vj : j["vars"])
        result.push_back(vj.get<TestCPP::Variable>());
  } catch (const std::exception &) {
    result.clear();
  }

  return result;
}

bool writeFile(const fs::path &filePath, const std::string &content) {
  std::ofstream ofs(filePath, std::ios::out | std::ios::trunc | std::ios::binary);
  if (!ofs.is_open()) {
    // TODO: err
    return false;
  }

  ofs << content;
  if (!ofs.good()) {
    // TODO: err
    return false;
  }

  ofs.close();
  if (ofs.fail()) {
    // TODO: err
    return false;
  }

  return true;
}

bool createDirectorySafely(const fs::path &dirPath) {
  std::error_code ec;

  const auto status = fs::symlink_status(dirPath, ec);
  if (ec && ec != std::errc::no_such_file_or_directory) {
    // TODO: err
    return false;
  }

  if (fs::exists(status)) {
    if (fs::is_symlink(status)) {
      // TODO: err
      return false;
    }
    if (!fs::is_directory(status)) {
      // TODO: err
      return false;
    }
    return true;
  }

  fs::create_directories(dirPath, ec);
  if (ec) {
    // TODO: err
    return false;
  }

  return true;
}

void TestCPP::create_nodegraph(const std::string &path) {
  if (path.empty()) {
    // TODO: err
    return;
  }

  std::error_code ec;

  const fs::path rootPath = fs::weakly_canonical(fs::absolute(fs::path(path), ec), ec);
  if (ec) {
    // TODO: err
    return;
  }
  if (!createDirectorySafely(rootPath)) {
    return;
  }

  const fs::path ngDir = rootPath / "cpp_sketch.ng";
  if (!createDirectorySafely(ngDir)) {
    return;
  }

  const fs::path nodegraphFile = ngDir / "graph.nodegraph";
  static const std::string nodegraphContent = R"({"context_id": "testcpp", "graph": {}})";

  if (!writeFile(nodegraphFile, nodegraphContent)) {
    return;
  }

  const fs::path storageFile = rootPath / "storage.json";
  static const std::string storageContent = "{}";

  if (!writeFile(storageFile, storageContent)) {
    return;
  }
}