#pragma once

#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/exceptions.hpp>
#include <jaffarCommon/file.hpp>
#include <jaffarCommon/json.hpp>
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

    // Running a single frame
    // retro_run();
  }

  inline jaffarCommon::hash::hash_t getStateHash() const
  {
    MetroHash128 hash;
    
    // //  Getting RAM pointer and size
    // hash.Update(_memoryAreas.wram, _memorySizes.wram);
    // hash.Update(_memoryAreas.vram, _memorySizes.vram);

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
    _room.print();
  }

  size_t getEmulatorStateSize()
  {
    // return (size_t)_emu->saveState(nullptr, 0, nullptr);
    return 0;
  }

  void enableStateBlock(const std::string& block) 
  {
    // enableStateBlockImpl(block);
  }

  void disableStateBlock(const std::string& block)
  {
    //  disableStateBlockImpl(block);
    // _stateSize = getEmulatorStateSize();
  }

  void setWorkRamSerializationSize(const size_t size)
  {
    // setWorkRamSerializationSizeImpl(size);
    // _stateSize = getEmulatorStateSize();
  }

  inline size_t getStateSize() const 
  {
    // return _stateSize;
    return 0;
  }

  inline jaffar::InputParser *getInputParser() const { return _inputParser.get(); }
  
  void serializeState(jaffarCommon::serializer::Base& s) const
  {
    // VFile* vf;
    // _emu->saveState(nullptr, 0, (char*)_dummyStateData);
    // s.push(_dummyStateData, _stateSize);
  }

  void deserializeState(jaffarCommon::deserializer::Base& d) 
  {
    // d.pop(_dummyStateData, _stateSize);
    // _emu->loadState((char*)_dummyStateData, _stateSize);
  }

  std::string getCoreName() const { return "QuickerBan"; }

  private:

  size_t _stateSize;
  std::unique_ptr<jaffar::InputParser> _inputParser;
  std::string _inputRoomFilePath;
  quickerBan::Room _room;
};

} // namespace jaffar