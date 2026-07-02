#include <vxcore/include/vortex.h>
#include <vxcore/include/vortex_internals.h>

#include <atomic>
#include <format>
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

namespace TestCPP {
  struct Context {
    std::shared_ptr<ModuleInterface> interface;
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

}  // namespace TestCPP

#endif  // SAMPLE_MODULE_HPP