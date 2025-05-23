#include "argparse/argparse.hpp"
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/serializers/contiguous.hpp>
#include <jaffarCommon/deserializers/contiguous.hpp>
#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/string.hpp>
#include <jaffarCommon/timing.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/file.hpp>
#include "emuInstance.hpp"
#include <chrono>
#include <sstream>
#include <vector>
#include <string>


int main(int argc, char *argv[])
{
  // Parsing command line arguments
  argparse::ArgumentParser program("tester", "1.0");

  program.add_argument("scriptFile")
    .help("Path to the test script file to run.")
    .required();

  program.add_argument("sequenceFile")
    .help("Path to the input sequence file (.sol) to reproduce.")
    .required();

  program.add_argument("--cycleType")
    .help("Specifies the emulation actions to be performed per each input. Possible values: 'Simple': performs only advance state, 'Rerecord': performs load/advance/save, and 'Full': performs load/advance/save/advance.")
    .default_value(std::string("Simple"));

  program.add_argument("--hashOutputFile")
    .help("Path to write the hash output to.")
    .default_value(std::string(""));

  program.add_argument("--warmup")
  .help("Warms up the CPU before running for reduced variation in performance results")
  .default_value(false)
  .implicit_value(true);

  // Try to parse arguments
  try { program.parse_args(argc, argv); } catch (const std::runtime_error &err) { JAFFAR_THROW_LOGIC("%s\n%s", err.what(), program.help().str().c_str()); }

  // Getting test script file path
  const auto scriptFilePath = program.get<std::string>("scriptFile");

  // Getting path where to save the hash output (if any)
  const auto hashOutputFile = program.get<std::string>("--hashOutputFile");

  // Getting cycle type
  const auto cycleType = program.get<std::string>("--cycleType");

  bool cycleTypeRecognized = false;
  if (cycleType == "Simple") cycleTypeRecognized = true;
  if (cycleType == "Rerecord") cycleTypeRecognized = true;
  if (cycleTypeRecognized == false) JAFFAR_THROW_LOGIC("Unrecognized cycle type: %s\n", cycleType.c_str());

  // Getting warmup setting
  const auto useWarmUp = program.get<bool>("--warmup");

  // Loading script file
  std::string configJsRaw;
  if (jaffarCommon::file::loadStringFromFile(configJsRaw, scriptFilePath) == false) JAFFAR_THROW_LOGIC("Could not find/read script file: %s\n", scriptFilePath.c_str());

  // Parsing script
  const auto configJs = nlohmann::json::parse(configJsRaw);

  // Getting sequence file path
  std::string sequenceFilePath = program.get<std::string>("sequenceFile");

  // Creating emulator instance
  auto e = jaffar::EmuInstance(configJs);

  // Initializing emulator instance
  e.initialize();
  
  // Getting full state size
  const auto stateSize = e.getStateSize();

  // Loading sequence file
  std::string sequenceRaw;
  if (jaffarCommon::file::loadStringFromFile(sequenceRaw, sequenceFilePath) == false) JAFFAR_THROW_LOGIC("[ERROR] Could not find or read from input sequence file: %s\n", sequenceFilePath.c_str());

  // Getting sequence lenght
  const auto sequenceLength = sequenceRaw.size();

  // Getting input parser from the emulator
  const auto inputParser = e.getInputParser();

  // Getting decoded emulator input for each entry in the sequence
  std::vector<jaffar::input_t> decodedSequence;
  for (const auto &input : sequenceRaw) decodedSequence.push_back(inputParser->parseInputString(input));

  // Getting emulation core name
  std::string emulationCoreName = e.getCoreName();

  // Printing test information
  printf("[] -----------------------------------------\n");
  printf("[] Running Script:                         '%s'\n", scriptFilePath.c_str());
  printf("[] Cycle Type:                             '%s'\n", cycleType.c_str());
  printf("[] Emulation Core:                         '%s'\n", emulationCoreName.c_str());
  printf("[] Sequence File:                          '%s'\n", sequenceFilePath.c_str());
  printf("[] Sequence Length:                        %lu\n", sequenceLength);
  
  // If warmup is enabled, run it now. This helps in reducing variation in performance results due to CPU throttling
  if (useWarmUp)
  {
    printf("[] ********** Warming Up **********\n");

    auto tw = jaffarCommon::timing::now();
    double waitedTime = 0.0;
    #pragma omp parallel
    while(waitedTime < 2.0) waitedTime = jaffarCommon::timing::timeDeltaSeconds(jaffarCommon::timing::now(), tw);
  }

  printf("[] ********** Running Test **********\n");

  fflush(stdout);

  // Serializing initial state
  auto currentState = (uint8_t *)malloc(stateSize);
  {
    jaffarCommon::serializer::Contiguous cs(currentState);
    e.serializeState(cs);
  }

  // Check whether to perform each action
  bool doPreAdvance = cycleType == "Rerecord";
  bool doDeserialize = cycleType == "Rerecord";
  bool doSerialize = cycleType == "Rerecord";

  // Actually running the sequence
  auto t0 = std::chrono::high_resolution_clock::now();
  for (const auto &input : decodedSequence)
  {
    // e.printInfo();
    
    if (doPreAdvance == true) e.advanceState(input);
    
    if (doDeserialize == true)
    {
      jaffarCommon::deserializer::Contiguous d(currentState, stateSize);
      e.deserializeState(d);
    } 
    
    e.advanceState(input);

    if (doSerialize == true)
    {
      auto s = jaffarCommon::serializer::Contiguous(currentState, stateSize);
      e.serializeState(s);
    } 
  }
  auto tf = std::chrono::high_resolution_clock::now();

  // Calculating running time
  auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>(tf - t0).count();
  double elapsedTimeSeconds = (double)dt * 1.0e-9;

  // Calculating final state hash
  auto result = e.getStateHash();

  // Creating hash string
  char hashStringBuffer[256];
  sprintf(hashStringBuffer, "0x%lX%lX", result.first, result.second);

  // Printing Info
  e.printInfo();

  // Printing time information
  printf("[] Elapsed time:                           %3.3fs\n", (double)dt * 1.0e-9);
  printf("[] Performance:                            %.3f inputs / s\n", (double)sequenceLength / elapsedTimeSeconds);
  printf("[] Final State Hash:                       %s\n", hashStringBuffer);
  
  // If saving hash, do it now
  if (hashOutputFile != "") jaffarCommon::file::saveStringToFile(std::string(hashStringBuffer), hashOutputFile.c_str());

  // If reached this point, everything ran ok
  return 0;
}
