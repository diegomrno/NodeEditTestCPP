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
  auto inst = ModuleUI::TestCppWrapperAppWindow::create();
  Cherry::AddAppWindow(inst->get_app_window());
  s_instances.push_back(inst);

  {
    // auto i = ModuleUI::TestCppOtherRandomWindow::create();
    // Cherry::AddAppWindow(i->get_app_window());
  }

  {
    nlohmann::json j;
    j["path"] = graph_path;
    j["disable_native_saving_system"] = true;
    j["graph_title"] = "C++ Simple Program";
    j["parent_appwindow"] = "TEST";
    j["logo_path"] = TestCPP::get_path("resources/icons/icon_magnifying_glass.png");

    auto ret = ReturnValues();
    auto args = ArgumentValues(j.dump());
    vxe::call_input_event("infinitehq.nodeedit", "open_graph", args, ret);

    id = ret.get_json()["session_id"];
    inst->set_instance_id(id);
  }

  {
    nlohmann::json j;
    j["session_id"] = id;
    auto args = ArgumentValues(j.dump());
    auto ret = ReturnValues();
    vxe::call_input_event("infinitehq.nodeedit", "refresh_nodegraph", args, ret);
  }
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
}
