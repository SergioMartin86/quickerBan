#pragma once

#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/exceptions.hpp>
#include <jaffarCommon/file.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/serializers/base.hpp>
#include <jaffarCommon/deserializers/base.hpp>
#include <jaffarCommon/serializers/contiguous.hpp>
#include <jaffarCommon/deserializers/contiguous.hpp>
#include "inputParser.hpp"
#include "room.hpp"

namespace jaffar
{

class EmuInstance;

class EmuInstance
{
  public:

  EmuInstance(const nlohmann::json &config)
  {
    _inputRoomFilePath = jaffarCommon::json::getString(config, "Input Room File");
    // _biosFilePath = jaffarCommon::json::getString(config, "Bios File Path");
    // _inputParser = std::make_unique<jaffar::InputParser>(config);
  }

  ~EmuInstance() = default;
 
  void advanceState(const jaffar::input_t &input)
  {
    // Setting input
    auto inputValue = input.key;

    _isDeadlock = true;
    if (inputValue == InputKey_t::UP) _isDeadlock = _room.move(-1, 0);
    if (inputValue == InputKey_t::DOWN) _isDeadlock = _room.move(1, 0);
    if (inputValue == InputKey_t::RIGHT) _isDeadlock = _room.move(0, 1);
    if (inputValue == InputKey_t::LEFT) _isDeadlock = _room.move(0, -1);
  }

  inline jaffarCommon::hash::hash_t getStateHash() const
  {
    MetroHash128 hash;
    
    //  Getting RAM pointer and size
    hash.Update(_room.getState(), _room.getStateSize());

    jaffarCommon::hash::hash_t result;
    hash.Finalize(reinterpret_cast<uint8_t *>(&result));
    return result;
  }

  void initialize()
  {
    // Reading from input file
    std::string inputRoomData;
    bool        status = jaffarCommon::file::loadStringFromFile(inputRoomData, _inputRoomFilePath.c_str());
    if (status == false) JAFFAR_THROW_LOGIC("Could not find/read from input sok file: %s\n", _inputRoomFilePath.c_str());

    _room.parse(inputRoomData);

    _stateSize = _room.getStateSize();
  }

  void printInfo()
  {
     _room.printMap();

    //  // Getting state
    //  jaffarCommon::logger::log("[] Possible Moves: { ");
    //  if (_room.canMoveUp()) jaffarCommon::logger::log("U");
    //  if (_room.canMoveDown()) jaffarCommon::logger::log("D");
    //  if (_room.canMoveLeft()) jaffarCommon::logger::log("L");
    //  if (_room.canMoveRight()) jaffarCommon::logger::log("R");
    //  jaffarCommon::logger::log(" }\n");
  }

  inline bool canMoveUp() const {return _room.canMoveUp(); }
  inline bool canMoveDown() const {return _room.canMoveDown(); }
  inline bool canMoveLeft() const {return _room.canMoveLeft(); }
  inline bool canMoveRight() const {return _room.canMoveRight(); }
  inline size_t getBoxesOnGoal() const {return _room.getBoxesOnGoal(); }
  inline size_t getGoalCount() const {return _room.getGoalCount(); }
  inline bool getMovedBox() const { return _room.getMovedBox(); }
  inline bool getIsDeadlock() const { return _isDeadlock; }
  inline uint32_t getTotalDistance() { return _room.getTotalDistanceToGoal(); }

  inline uint8_t* getState() const 
  {
    return _room.getState();
  }

  inline size_t getStateSize() const 
  {
    return _stateSize;
  }

  inline jaffar::InputParser *getInputParser() const { return _inputParser.get(); }
  
  void serializeState(jaffarCommon::serializer::Base& s) const
  {
    _room.saveState(s);
  }

  void deserializeState(jaffarCommon::deserializer::Base& d) 
  {
    _room.loadState(d);
  }

  std::string getCoreName() const { return "QuickerBan"; }

  private:

  size_t _stateSize;
  std::unique_ptr<jaffar::InputParser> _inputParser;
  std::string _inputRoomFilePath;
  quickerBan::Room _room;
  bool _isDeadlock = false;
};

} // namespace jaffar