#include "./src/module.hpp"

// main.cpp
class infinitehq_testcpp : public ModuleInterface {
 public:
  std::shared_ptr<TestCPP::Context> ctx;

  void execute() override {
    ctx = TestCPP::create_context();

    auto m = ModuleInterface::get_editor_module_by_name(this->name());
    TestCPP::get_current_context()->m_interface = m;

    this->add_content_browser_item_identifier(ItemIdentifierInterface(
        TestCPP::is_cpp_sketch,
        "infinitehq.testcpp:cpp_sketch",
        "Cpp Sketch",
        "#B1FF31",
        TestCPP::get_path("resources/icons/cb_i.png")));

    this->add_content_browser_item_handler(ItemHandlerInterface(
        "infinitehq.testcpp:cpp_sketch",
        TestCPP::open_cpp_sketch,
        "Edit sketch",
        "Edit this sketch",
        TestCPP::get_path("resources/icons/edit.png")));

    this->add_content_browser_item_creator(
        ItemCreatorInterface(TestCPP::create_nodegraph, "Create C++ Nodegraph", "Create nodegraph"));

    TestCPP::setup_graph_ctx();
  }

  void destroy() override {
    this->reset_module();

    TestCPP::destroy_context(ctx);
    ctx.reset();
  }
};

#ifdef _WIN32
extern "C" __declspec(dllexport) ModuleInterface *create_em() {
  return new infinitehq_testcpp();
}
#else
extern "C" ModuleInterface *create_em() {
  return new infinitehq_testcpp();
}
#endif
