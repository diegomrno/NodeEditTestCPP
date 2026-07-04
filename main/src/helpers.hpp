#pragma once
#include <vxcore/include/vortex.h>
#include <vxcore/include/vortex_internals.h>

#include <string>
#include <unordered_map>
#include <vector>
#include <vxgui/editor/main/editor.hpp>
#ifndef TESTCPP_HELPERS
#define TESTCPP_HELPERS
namespace TestCPP {
  struct Variable {
    std::string name;
    std::string type;
    std::string value;
    std::string default_value;
    std::string id;
  };

  struct PinFormatInfo {
    std::string name;
    std::string color;
  };

  struct DrawerSession {
    std::vector<Variable> vars;
    std::string selected_var;
    std::unordered_map<std::string, PinFormatInfo> pin_format_cache;
  };
}  // namespace TestCPP
#endif  // TESTCPP_HELPERS