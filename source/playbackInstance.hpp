#pragma once

#include "emuInstance.hpp"
#include <string>
#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/exceptions.hpp>

#define _INVERSE_FRAME_RATE 66667

struct stepData_t
{
  jaffar::input_t inputData;
  char input;
  uint8_t *stateData;
  uint8_t *videoBuffer;
  jaffarCommon::hash::hash_t hash;
};

class PlaybackInstance
{
  public:

  // Initializes the playback module instance
  PlaybackInstance(jaffar::EmuInstance *emu, const std::string &sequence, const std::string& cycleType) :
   _emu(emu)
  {
    // Getting full state size
    _fullStateSize = _emu->getStateSize();  

    // Allocating temporary state data 
    uint8_t* stateData = (uint8_t*)malloc(_fullStateSize);

    // Getting input decoder
    auto inputParser = _emu->getInputParser();

    // Building sequence information
    for (size_t i = 0; i < sequence.size(); i++)
    {
      // Adding new step
      stepData_t step;
      step.input = sequence[i];
      step.inputData = inputParser->parseInputString(step.input);

      // Serializing state
      jaffarCommon::serializer::Contiguous s(stateData, _fullStateSize);
      _emu->serializeState(s);
      step.hash = _emu->getStateHash();

      // Saving step data
      step.stateData = (uint8_t *)malloc(_fullStateSize);
      memcpy(step.stateData, stateData, _fullStateSize);

      // Adding the step into the sequence
      _stepSequence.push_back(step);

      // We advance depending on cycle type
      if (cycleType == "Simple")
      {
        _emu->advanceState(step.inputData );
      }

      if (cycleType == "Rerecord")
      {
        _emu->advanceState(step.inputData );
        jaffarCommon::deserializer::Contiguous d(stateData, _fullStateSize);
        _emu->deserializeState(d);
        _emu->advanceState(step.inputData );
      }
    }

    // Adding last step with no input
    stepData_t step;
    step.input = '.';
    step.inputData = _stepSequence.rbegin()->inputData;
    step.stateData = (uint8_t *)malloc(_fullStateSize);
    jaffarCommon::serializer::Contiguous s(step.stateData, _fullStateSize);

    _emu->serializeState(s);
    step.hash = _emu->getStateHash();

    // Adding the step into the sequence
    _stepSequence.push_back(step);

    // Freeing memory
    free(stateData);
  }

  size_t getSequenceLength() const
  {
    return _stepSequence.size();
  }

  const char getInputString(const size_t stepId) const
  {
    // Checking the required step id does not exceed contents of the sequence
    if (stepId > _stepSequence.size()) JAFFAR_THROW_RUNTIME("[Error] Attempting to render a step larger than the step sequence");

    // Getting step information
    const auto &step = _stepSequence[stepId];

    // Returning step input
    return step.input;
  }

  const jaffar::input_t getInputData(const size_t stepId) const
  {
    // Checking the required step id does not exceed contents of the sequence
    if (stepId > _stepSequence.size()) JAFFAR_THROW_RUNTIME("[Error] Attempting to render a step larger than the step sequence");

    // Getting step information
    const auto &step = _stepSequence[stepId];

    // Returning step input
    return step.inputData;
  }

  const uint8_t *getStateData(const size_t stepId) const
  {
    // Checking the required step id does not exceed contents of the sequence
    if (stepId > _stepSequence.size()) JAFFAR_THROW_RUNTIME("[Error] Attempting to render a step larger than the step sequence");

    // Getting step information
    const auto &step = _stepSequence[stepId];

    // Returning step input
    return step.stateData;
  }

  const jaffarCommon::hash::hash_t getStateHash(const size_t stepId) const
  {
    // Checking the required step id does not exceed contents of the sequence
    if (stepId > _stepSequence.size()) JAFFAR_THROW_RUNTIME("[Error] Attempting to render a step larger than the step sequence");

    // Getting step information
    const auto &step = _stepSequence[stepId];

    // Returning step input
    return step.hash;
  }

  private:
  
  // Internal sequence information
  std::vector<stepData_t> _stepSequence;

  // Pointer to the contained emulator instance
  jaffar::EmuInstance *const _emu;

  // Full size of the game state
  size_t _fullStateSize;
};
