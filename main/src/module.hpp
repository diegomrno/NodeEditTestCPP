#include <vxcore/include/vortex.h>
#include <vxcore/include/vortex_internals.h>

#include <atomic>
#include <format>
#include <memory>
#include <unordered_map>
#include <vxgui/editor/main/editor.hpp>

#include "../ui/instances/node_editor_wrapper/node_editor_wrapper.hpp"
#include "./helpers.hpp"

#ifndef SAMPLE_MODULE_HPP
#define SAMPLE_MODULE_HPP

#ifndef TESTCPP_API
#define TESTCPP_API
#endif

// TODO :
// Graph context extensions
// Vortex Events APi

static int i_session = 0;

namespace TestCPP {
  struct Context {
    std::shared_ptr<ModuleInterface> interface;
    std::unordered_map<std::string, std::string> session_links;  // second = graph id
    std::unordered_map<std::string, bool> session_need_save;
    std::unordered_map<std::string, bool> session_need_refresh;
    std::unordered_map<std::string, std::shared_ptr<DrawerSession>> session_variables;  // second = drawer
    // TODO structure to hold variables and current edititng states
  };
}  // namespace TestCPP
// context pointer
#ifndef CTestCPP
extern TESTCPP_API std::weak_ptr<TestCPP::Context> CTestCPP;
#endif

namespace TestCPP {
  // Context
  TESTCPP_API std::shared_ptr<TestCPP::Context> create_context();
  TESTCPP_API void destroy_context(std::shared_ptr<TestCPP::Context> ctx);
  TESTCPP_API void set_current_context(std::shared_ptr<TestCPP::Context> ctx);
  TESTCPP_API std::shared_ptr<TestCPP::Context> get_current_context();

  TESTCPP_API std::string get_path(const std::string &path);

  TESTCPP_API bool is_cpp_sketch(const std::string &path);
  TESTCPP_API void open_cpp_sketch(const std::string &path);

  TESTCPP_API void setup_graph_ctx();
  TESTCPP_API void set_session_link(const std::string &id, const std::string &graph_id);
  TESTCPP_API std::string get_session_link(const std::string &id);
  TESTCPP_API void set_session_variables(const std::string &id, const std::shared_ptr<DrawerSession> &variables);
  TESTCPP_API std::shared_ptr<DrawerSession> get_session_variables(const std::string &id);
  TESTCPP_API void set_session_need_refresh(const std::string &id, bool v);
  TESTCPP_API bool get_session_need_refresh(const std::string &id);
  TESTCPP_API void set_session_need_save(const std::string &id, bool v);
  TESTCPP_API bool get_session_need_save(const std::string &id);
}  // namespace TestCPP

#endif  // SAMPLE_MODULE_HPP