#pragma once

#include <cstdint>
#include <cstdio>
#include <unistd.h>
#include <jaffarCommon/string.hpp>

namespace quickerBan {

class Room
{
  public:

  enum itemType
  {
    wall = 0,
    floor,
    pusher,
    pusher_on_goal,
    box,
    box_on_goal,
    goal
  };

  Room() = default;
  ~Room() = default;

  __INLINE__ void printMap() const
  {
    uint8_t* tempMap = (uint8_t*)malloc(sizeof(uint8_t) * _width * _height);
    memcpy(tempMap, _background, sizeof(uint8_t) * _width * _height);

    // Placing pusher
    auto pusherPosY = _state[0];
    auto pusherPosX = _state[1];
    auto index = getIndex(pusherPosY, pusherPosX);
    if (_background[index] == itemType::goal) tempMap[index] = itemType::pusher_on_goal;
    else  tempMap[index] = itemType::pusher;

    // Placing boxes
    for(uint8_t i = 0; i < _boxCount; i++)
    {
      auto boxPosY = _state[(i+1) * 2 + 0];
      auto boxPosX = _state[(i+1) * 2 + 1];
      auto index = getIndex(boxPosY, boxPosX);
      if (_background[index] == itemType::goal) tempMap[index] = itemType::box_on_goal;
      else  tempMap[index] = itemType::box;
    }

    // Printing
    for(uint8_t i = 0; i < _height; i++)
    {
     for(uint8_t j = 0; j < _width; j++)
     {
       const auto tileType = tempMap[getIndex(i,j)];
       if (tileType == itemType::wall) jaffarCommon::logger::log("#");
       if (tileType == itemType::pusher) jaffarCommon::logger::log("@");
       if (tileType == itemType::pusher_on_goal) jaffarCommon::logger::log("+");
       if (tileType == itemType::box) jaffarCommon::logger::log("$");
       if (tileType == itemType::box_on_goal) jaffarCommon::logger::log("*");
       if (tileType == itemType::goal) jaffarCommon::logger::log(".");
       if (tileType == itemType::floor) jaffarCommon::logger::log(" ");
     } 
     jaffarCommon::logger::log("\n");
    }

    // Freeing up
    free(tempMap);
  }

  __INLINE__ void parse(const std::string& roomString)
  {
    const auto rowSequence = jaffarCommon::string::split(roomString, '\n');

    // Getting room size
    _height = rowSequence.size();
    for (uint8_t i = 0; i < _height; i++)
    {
        const auto& row = rowSequence[i];
        if (row.size() > _width) _width = (uint8_t) row.size();
    }

    // Allocating static and dynamic room information
    long pageSize = sysconf (_SC_PAGESIZE);
    _background = (uint8_t*)aligned_alloc(pageSize, _height * _width * sizeof(uint8_t));
    _boxMap = (uint8_t*)aligned_alloc(pageSize, _height * _width * sizeof(uint8_t));
    uint8_t* tiles = (uint8_t*)aligned_alloc(pageSize, _height * _width * sizeof(uint8_t));

    // Clearing room 
    for (uint8_t i = 0; i < _height; i++)
     for (uint8_t j = 0; j < _width; j++)
      tiles[getIndex(i,j)] = itemType::floor;
    
    // Parsing from input
    _boxCount = 0;
    _goalCount = 0;
    for (uint8_t i = 0; i < _height; i++)
    {
        const auto& row = rowSequence[i];
        for (uint8_t j = 0; j < row.size(); j++)
        {
            if (row[j] == ' ' || row[j] == '-' || row[j] == '_') tiles[getIndex(i,j)] = itemType::floor;
            if (row[j] == '.') { tiles[getIndex(i,j)] = itemType::goal; _goalCount++; }
            if (row[j] == '*' || row[j] == 'B') { tiles[getIndex(i,j)] = itemType::box_on_goal; _boxCount++; _goalCount++; }
            if (row[j] == 'b' || row[j] == '$') { tiles[getIndex(i,j)] = itemType::box; _boxCount++;  }
            if (row[j] == 'P' || row[j] == '+') { tiles[getIndex(i,j)] = itemType::pusher_on_goal; _goalCount++; }
            if (row[j] == 'p' || row[j] == '@') tiles[getIndex(i,j)] = itemType::pusher;
            if (row[j] == '#') tiles[getIndex(i,j)] = itemType::wall;
        }
    }

    // Sanity check: boxes = goals
    if (_boxCount != _goalCount) JAFFAR_THROW_LOGIC("Number of boxes (%lu) is not equal to goals (%lu)", _boxCount, _goalCount);

    // Allocating state space
    _stateSize = 2 * sizeof(uint8_t) * (1 + _boxCount);
    _state = (uint8_t*)aligned_alloc(pageSize, _stateSize);

    // Building background
    size_t currentBox = 1;
    for (uint8_t i = 0; i < _height; i++)
    for (uint8_t j = 0; j < _width; j++)
    {
       if (tiles[getIndex(i,j)] == itemType::floor)
       { 
           _background[getIndex(i,j)] = itemType::floor;
       }

       if (tiles[getIndex(i,j)] == itemType::goal)
       { 
           _background[getIndex(i,j)] = itemType::goal;
       }

       if (tiles[getIndex(i,j)] == itemType::pusher)
       { 
           _state[0] = i;
           _state[1] = j;
           _background[getIndex(i,j)] = itemType::floor;
       }
       
       if (tiles[getIndex(i,j)] == itemType::pusher_on_goal)
       {
            _state[0] = i;
            _state[1] = j;
            _background[getIndex(i,j)] = itemType::goal;
       }

       if (tiles[getIndex(i,j)] == itemType::box)
       {
            _state[currentBox * 2 + 0] = i;
            _state[currentBox * 2 + 1] = j;
            _background[getIndex(i,j)] = itemType::floor;
            currentBox++;
       }

       if (tiles[getIndex(i,j)] == itemType::box_on_goal)
       {
            _state[currentBox * 2 + 0] = i;
            _state[currentBox * 2 + 1] = j;
            _background[getIndex(i,j)] = itemType::goal;
            currentBox++;
       }
    }

    free(tiles);
    updateBoxMap();
  }

  __INLINE__ uint8_t getBoxCount() const { return _boxCount; }
  __INLINE__ bool canMoveUp() const { return canMove(-1, 0); }
  __INLINE__ bool canMoveDown() const { return canMove(1, 0); }
  __INLINE__ bool canMoveLeft() const { return canMove(0, -1); }
  __INLINE__ bool canMoveRight() const { return canMove(0, 1); }

  __INLINE__ bool move(const int8_t deltaY, const int8_t deltaX) 
  {
    // Locating pusher's target destination
    auto& pusherPosY = _state[0];
    auto& pusherPosX = _state[1];
    const auto destPosY = pusherPosY + deltaY;
    const auto destPosX = pusherPosX + deltaX;

    // Moving pusher
    pusherPosY = destPosY;
    pusherPosX = destPosX;

    // Getting destination index
    const auto index = getIndex(pusherPosY, pusherPosX);

    // Checking if there is a box there already
    if (_boxMap[index] == itemType::box)
    {
       // Move the box in the map
       _boxMap[index] = itemType::floor;

       // And putting it in the next place
       const auto dest2PosY = pusherPosY + deltaY;
       const auto dest2PosX = pusherPosX + deltaX;
       const auto index2 = getIndex(dest2PosY, dest2PosX);
       _boxMap[index2] = itemType::box;

       // Looking for box in the state vector and updating it there
       for (size_t i = 0; i < _boxCount; i++)
       {
        const auto boxPosY = (i+1) * 2 + 0;
        const auto boxPosX = (i+1) * 2 + 1;
        if (_state[boxPosY] == destPosY && _state[boxPosX] == destPosX)
        {
           _state[boxPosY] = dest2PosY;
           _state[boxPosX] = dest2PosX;
           break;
        }
       }
    }

    return true;
  }
  
  __INLINE__ size_t getBoxesOnGoal() const
   {
    size_t boxesOnGoal = 0;
    for(uint8_t i = 0; i < _boxCount; i++)
    {
      auto boxPosY = _state[(i+1) * 2 + 0];
      auto boxPosX = _state[(i+1) * 2 + 1];
      auto index = getIndex(boxPosY, boxPosX);
      if (_background[index] == itemType::goal) boxesOnGoal++;
    }
    return boxesOnGoal;
   }

  __INLINE__ size_t getGoalCount() const
  {
    return _goalCount;
  }

  __INLINE__ uint8_t* getState() const { return _state; }
  
  __INLINE__ void loadState(const uint8_t* state)
  {
    
    memcpy(_state, state, _stateSize);
    updateBoxMap();
  }

  __INLINE__ void saveState(uint8_t* state) const
  {
    memcpy(state, _state, _stateSize);
  }

  __INLINE__ size_t getStateSize() const { return _stateSize; }

  private:

  __INLINE__ bool canMove(const int8_t deltaY, const int8_t deltaX) const 
  {
    // Locating pusher
    const auto pusherPosY = _state[0];
    const auto pusherPosX = _state[1];

    // Getting index of destination square
    const auto nextTilePosY = pusherPosY+(1 * deltaY);
    const auto nextTilePosX = pusherPosX+(1 * deltaX);
    const auto nextTileIndex = getIndex(nextTilePosY, nextTilePosX);

    // Checking for wall immediately close
    if (_background[nextTileIndex] == itemType::wall) return false;

    // Checking for box
    if (_boxMap[nextTileIndex] == itemType::box)
    {
        const auto nextTile2PosY = pusherPosY+(2 * deltaY);
        const auto nextTile2PosX = pusherPosX+(2 * deltaX);
        const auto nextTile2Index = getIndex(nextTile2PosY, nextTile2PosX);

        // If the other one is wall, then cannot move
        if (_background[nextTile2Index] == itemType::wall) return false;

        // If the other one is box, then cannot move
        if (_boxMap[nextTile2Index] == itemType::box) return false;
    }

    // No restrictions
    return true;
  }

  __INLINE__ void updateBoxMap()
  {
    memset(_boxMap, itemType::floor, _height * _width * sizeof(uint8_t));

    for (size_t i = 0; i < _boxCount; i++)
    {
      uint8_t boxPosY = _state[(i+1) * 2 + 0];
      uint8_t boxPosX = _state[(i+1) * 2 + 1];
      _boxMap[getIndex(boxPosY,boxPosX)] = itemType::box;
    }
  }
  __INLINE__ uint16_t getIndex(const uint8_t i, const uint8_t j) const { return (uint16_t)i * (uint16_t)_width + (uint16_t)j; }

  uint8_t* _background = nullptr;
  uint8_t* _boxMap = nullptr;
  uint8_t _width = 0;
  uint8_t _height = 0;

  uint8_t* _state = nullptr;
  size_t _stateSize;
  size_t _boxCount = 0;
  size_t _goalCount = 0;

};

} // namespace quickerBan