#pragma once

// Base controller class
// by eien86

#include <cstdint>
#include <jaffarCommon/exceptions.hpp>
#include <jaffarCommon/json.hpp>
#include <string>
#include <sstream>

namespace jaffar
{

enum InputKey_t {
  UP      = 0,
  DOWN    = 1,
  LEFT    = 2,
  RIGHT   = 3,
};

typedef uint8_t port_t;

struct input_t
{
  InputKey_t key;
};

class InputParser
{
public:

  enum controller_t { none, joypad };

  InputParser(const nlohmann::json &config)
  {
  }

  inline input_t parseInputString(const std::string& s) const
  {
    return parseInputString(s[0]);
  };

  inline input_t parseInputString(const char c) const
  {
    // Storage for the input
    input_t input;

    // Parsing input
    if (c == 'U' || c == 'u') input.key = UP;
    if (c == 'L' || c == 'l') input.key = LEFT;
    if (c == 'D' || c == 'd') input.key = DOWN;
    if (c == 'R' || c == 'r') input.key = RIGHT;
    
    // Returning input
    return input;
  };

  input_t _input;
}; // class InputParser

} // namespace jaffar