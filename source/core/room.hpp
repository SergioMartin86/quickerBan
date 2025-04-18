#pragma once

#include <cstdint>
#include <cstdio>
#include <unistd.h>
#include <jaffarCommon/string.hpp>
#include <jaffarCommon/serializers/base.hpp>
#include <jaffarCommon/deserializers/base.hpp>
#include <jaffarCommon/exceptions.hpp>

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
    // Printing
    for(uint8_t i = 0; i < _height; i++)
    {
     for(uint8_t j = 0; j < _width; j++)
     {
       const auto tileType = _tiles[getIndex(i,j)];
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
    _tiles = (uint8_t*)aligned_alloc(pageSize, _height * _width * sizeof(uint8_t));
    _tmp = (uint8_t*)aligned_alloc(pageSize, _height * _width * sizeof(uint8_t));

    // Clearing room 
    for (uint8_t i = 0; i < _height; i++)
     for (uint8_t j = 0; j < _width; j++)
      _tiles[getIndex(i,j)] = itemType::floor;
    
    // Parsing from input
    _boxCount = 0;
    _goalCount = 0;
    for (uint8_t i = 0; i < _height; i++)
    {
        const auto& row = rowSequence[i];
        for (uint8_t j = 0; j < row.size(); j++)
        {
            if (row[j] == ' ' || row[j] == '-' || row[j] == '_') _tiles[getIndex(i,j)] = itemType::floor;
            if (row[j] == '.') { _tiles[getIndex(i,j)] = itemType::goal; _goalCount++; }
            if (row[j] == '*' || row[j] == 'B') { _tiles[getIndex(i,j)] = itemType::box_on_goal; _boxCount++; _goalCount++; }
            if (row[j] == 'b' || row[j] == '$') { _tiles[getIndex(i,j)] = itemType::box; _boxCount++;  }
            if (row[j] == 'P' || row[j] == '+') { _tiles[getIndex(i,j)] = itemType::pusher_on_goal; _goalCount++; }
            if (row[j] == 'p' || row[j] == '@') _tiles[getIndex(i,j)] = itemType::pusher;
            if (row[j] == '#') _tiles[getIndex(i,j)] = itemType::wall;
        }
    }

    // Sanity check: boxes = goals
    if (_boxCount != _goalCount) JAFFAR_THROW_LOGIC("Number of boxes (%lu) is not equal to goals (%lu)", _boxCount, _goalCount);

    // Allocating state space
    _stateSize = 2 * sizeof(uint8_t) * (1 + _boxCount);
    _state = (uint8_t*)aligned_alloc(pageSize, _stateSize);

    // Building background
    for (uint8_t i = 0; i < _height; i++)
    for (uint8_t j = 0; j < _width; j++)
    {
       if (_tiles[getIndex(i,j)] == itemType::floor) _background[getIndex(i,j)] = itemType::floor;
       if (_tiles[getIndex(i,j)] == itemType::goal) _background[getIndex(i,j)] = itemType::goal;
       if (_tiles[getIndex(i,j)] == itemType::pusher) _background[getIndex(i,j)] = itemType::floor;
       if (_tiles[getIndex(i,j)] == itemType::pusher_on_goal) _background[getIndex(i,j)] = itemType::goal;
       if (_tiles[getIndex(i,j)] == itemType::box) _background[getIndex(i,j)] = itemType::floor;
       if (_tiles[getIndex(i,j)] == itemType::box_on_goal) _background[getIndex(i,j)] = itemType::goal;
    }

    // Updating state
    updateState();
  }

  __INLINE__ uint8_t getBoxCount() const { return _boxCount; }
  __INLINE__ bool canMoveUp() const { return canMove(-1, 0); }
  __INLINE__ bool canMoveDown() const { return canMove(1, 0); }
  __INLINE__ bool canMoveLeft() const { return canMove(0, -1); }
  __INLINE__ bool canMoveRight() const { return canMove(0, 1); }

  // Returns true if deadlock, false if ok
  __INLINE__ bool move(const int8_t deltaY, const int8_t deltaX) 
  {
    // Locating pusher's target destination
    const auto pusherPosY = _state[0];
    const auto pusherPosX = _state[1];
    const auto pusherIdx = getIndex(pusherPosY, pusherPosX);

    // Removing current pusher from map
    _tiles[pusherIdx] = _background[pusherIdx];

    // Getting destination index
    const auto destPosY = pusherPosY + deltaY;
    const auto destPosX = pusherPosX + deltaX;
    const auto index1 = getIndex(destPosY, destPosX);

    // Reset moved box flag
    _movedBox = false;

    // Flag to report deadlock
    bool isDeadlock = false;

    // Checking if there is a box there already
    const auto tile1Type = _tiles[index1];

    // Move the pusher now
    _tiles[index1] = itemType::pusher;

    // Now handle the box push, if one
    if (tile1Type == itemType::box || tile1Type == itemType::box_on_goal)
    {
       // Setting flag
       _movedBox = true;

       // Moving box
       const auto dest2PosY = destPosY + deltaY;
       const auto dest2PosX = destPosX + deltaX;
       const auto index2 = getIndex(dest2PosY, dest2PosX);

      // Check if box is moving to a goal
      const bool isBoxOnGoal = _background[index2] == itemType::goal;

       // Checking deadlock
       if (isBoxOnGoal == false) isDeadlock = checkBoxDeadlock(dest2PosY, dest2PosX);

       // Moving box
       if (isBoxOnGoal) _tiles[index2] = itemType::box_on_goal;
       else _tiles[index2] = itemType::box;
    }

    // Updating state
    updateState();

    return isDeadlock;
  }
  
  // Checking if the recently moved box that is not in a goal position has provoked a deadlock
  __INLINE__ bool checkBoxDeadlock(const uint8_t y, const uint8_t x)
  {
    // Check 1: If the box is stuck between two walls
    //  x#
    //  #
    if (_background[getIndex(y+1, x)] == itemType::wall && _background[getIndex(y, x+1)] == itemType::wall) return true;

    // #x
    //  #
    if (_background[getIndex(y+1, x)] == itemType::wall && _background[getIndex(y, x-1)] == itemType::wall) return true;

    // # 
    // x#
    if (_background[getIndex(y-1, x)] == itemType::wall && _background[getIndex(y, x+1)] == itemType::wall) return true;

    //  #
    // #x
    if (_background[getIndex(y-1, x)] == itemType::wall && _background[getIndex(y, x-1)] == itemType::wall) return true;

    // Check 2: If the box is bunched up in a square
    // x$
    // $$
    if (
            (_tiles[getIndex(y+1, x+0)] == itemType::box || _tiles[getIndex(y+1, x+0)] == itemType::box_on_goal || _tiles[getIndex(y+1, x+0)] == itemType::wall)
         && (_tiles[getIndex(y+0, x+1)] == itemType::box || _tiles[getIndex(y+0, x+1)] == itemType::box_on_goal || _tiles[getIndex(y+0, x+1)] == itemType::wall)
         && (_tiles[getIndex(y+1, x+1)] == itemType::box || _tiles[getIndex(y+1, x+1)] == itemType::box_on_goal || _tiles[getIndex(y+1, x+1)] == itemType::wall)
    ) return true;

    // $x
    // $$
    if (
         (_tiles[getIndex(y+1, x+0)] == itemType::box || _tiles[getIndex(y+1, x+0)] == itemType::box_on_goal || _tiles[getIndex(y+1, x+0)] == itemType::wall)
      && (_tiles[getIndex(y+0, x-1)] == itemType::box || _tiles[getIndex(y+0, x-1)] == itemType::box_on_goal || _tiles[getIndex(y+0, x-1)] == itemType::wall)
      && (_tiles[getIndex(y+1, x-1)] == itemType::box || _tiles[getIndex(y+1, x-1)] == itemType::box_on_goal || _tiles[getIndex(y+1, x-1)] == itemType::wall)
        ) return true;

    // $$
    // x$
    if (
           (_tiles[getIndex(y-1, x+0)] == itemType::box || _tiles[getIndex(y-1, x+0)] == itemType::box_on_goal || _tiles[getIndex(y-1, x+0)] == itemType::wall)
        && (_tiles[getIndex(y+0, x+1)] == itemType::box || _tiles[getIndex(y+0, x+1)] == itemType::box_on_goal || _tiles[getIndex(y+0, x+1)] == itemType::wall)
        && (_tiles[getIndex(y-1, x+1)] == itemType::box || _tiles[getIndex(y-1, x+1)] == itemType::box_on_goal || _tiles[getIndex(y-1, x+1)] == itemType::wall)
     ) return true;

    // $$
    // $x
    if (
           (_tiles[getIndex(y-1, x+0)] == itemType::box || _tiles[getIndex(y-1, x+0)] == itemType::box_on_goal || _tiles[getIndex(y-1, x+0)] == itemType::wall)
        && (_tiles[getIndex(y+0, x-1)] == itemType::box || _tiles[getIndex(y+0, x-1)] == itemType::box_on_goal || _tiles[getIndex(y+0, x-1)] == itemType::wall)
        && (_tiles[getIndex(y-1, x-1)] == itemType::box || _tiles[getIndex(y-1, x-1)] == itemType::box_on_goal || _tiles[getIndex(y-1, x-1)] == itemType::wall)
       ) return true;

    return false;
  }

  __INLINE__ bool getMovedBox() const { return _movedBox; }
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

  __INLINE__ uint32_t getTotalDistanceToGoal()
  {
    memcpy(_tmp, _tiles, sizeof(uint8_t) * _height * _width);
    uint32_t totalDistance = 0;

    // Getting pusher position
    const auto pusherPosY = _state[0];
    const auto pusherPosX = _state[1];
    const auto pusherIdx = getIndex(pusherPosY, pusherPosX); 
    if (_background[pusherIdx] == itemType::goal) _tmp[pusherIdx] = itemType::goal;

    // For each of the boxes
    for (size_t box = 0; box < _boxCount; box++) 
    {
      auto boxPosY = _state[(box+1) * 2 + 0];
      auto boxPosX = _state[(box+1) * 2 + 1];
      const auto boxIdx = getIndex(boxPosY, boxPosX); 
      
      // If box is on goal continue
      if (_background[boxIdx] == itemType::goal)  continue;

      // Storing index of the closest goal
      uint16_t shortestGoalIndex = 0;
      uint32_t shortestGoalDistance = _height + _width;

      // Look for the closest free goal
      for (size_t i = 0; i < _height; i++)
      for (size_t j = 0; j < _width; j++)
      {
        // Getting index
        const auto curIndex = getIndex(i, j);
        if (_tmp[curIndex] == itemType::goal) 
        {
          // Getting distance
          const uint32_t curDistance = std::abs((int)boxPosY - (int)i) + std::abs((int)boxPosX - (int)j);
          if (curDistance < shortestGoalDistance)
          {
            shortestGoalDistance = curDistance;
            shortestGoalIndex = curIndex;
          }
        }
      }

      if (shortestGoalIndex == 0) JAFFAR_THROW_RUNTIME("Could not find a goal for the box");

      // Replacing shortest goal so it's not used again
      _tmp[shortestGoalIndex] = itemType::box_on_goal;

      // Adding distance
      totalDistance += shortestGoalDistance;
    }

    return totalDistance;
  }

  __INLINE__ uint8_t* getState() const { return _state; }
  
  __INLINE__ void loadState(jaffarCommon::deserializer::Base &deserializer)
  {
    deserializer.pop(_state, _stateSize);
    updateTiles();
  }

  __INLINE__ void saveState(jaffarCommon::serializer::Base &serializer) const
  {
    serializer.push(_state, _stateSize);
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

    // Getting the tile index for th enext position
    const auto nextTileIndex = getIndex(nextTilePosY, nextTilePosX);

    // Checking for wall immediately close
    if (_background[nextTileIndex] == itemType::wall) return false;

    // Checking for box
    const auto nextTileType = _tiles[nextTileIndex];
    if (nextTileType == itemType::box || nextTileType == itemType::box_on_goal)
    {
        const auto nextTile2PosY = pusherPosY+(2 * deltaY);
        const auto nextTile2PosX = pusherPosX+(2 * deltaX);
        const auto nextTile2Index = getIndex(nextTile2PosY, nextTile2PosX);

        // If the other one is wall, then cannot move
        if (_background[nextTile2Index] == itemType::wall) return false;

        // If the other one is box, then cannot move
        if (_tiles[nextTile2Index] == itemType::box || _tiles[nextTile2Index] == itemType::box_on_goal) return false;
    }

    // No restrictions
    return true;
  }
  
  __INLINE__ void updateTiles()
  {
    memcpy(_tiles, _background, _height * _width * sizeof(uint8_t));

    // Locating pusher's target destination
    const auto pusherPosY = _state[0];
    const auto pusherPosX = _state[1];
    const auto pusherIdx = getIndex(pusherPosY, pusherPosX);
    if (_background[pusherIdx] == itemType::goal) _tiles[pusherIdx] = itemType::pusher_on_goal;
    else _tiles[pusherIdx] = itemType::pusher;

    // Placing boxes
    for (size_t i = 0; i < _boxCount; i++)
    {
      auto boxPosY = _state[(i+1) * 2 + 0];
      auto boxPosX = _state[(i+1) * 2 + 1];
      const auto boxIdx = getIndex(boxPosY, boxPosX); 
      if (_background[boxIdx] == itemType::goal) _tiles[boxIdx] = itemType::box_on_goal;
      else _tiles[boxIdx] = itemType::box;
    }
  }


  __INLINE__ void updateState()
  {
    size_t currentPos = 1;
    for (uint8_t i = 0; i < _height; i++)
    for (uint8_t j = 0; j < _width; j++)
    {
       if (_tiles[getIndex(i,j)] == itemType::pusher)
       { 
           _state[0] = i;
           _state[1] = j;
       }
       
       if (_tiles[getIndex(i,j)] == itemType::box)
       {
            _state[currentPos * 2 + 0] = i;
            _state[currentPos * 2 + 1] = j;
            currentPos++;
       }

       if (_tiles[getIndex(i,j)] == itemType::box_on_goal)
       {
            _state[currentPos * 2 + 0] = i;
            _state[currentPos * 2 + 1] = j;
            currentPos++;
       }
    }
  }

  __INLINE__ uint16_t getIndex(const uint8_t i, const uint8_t j) const { return (uint16_t)i * (uint16_t)_width + (uint16_t)j; }

  uint8_t* _background = nullptr;
  uint8_t* _tiles = nullptr;
  
  uint8_t* _tmp = nullptr; // for temporary calculations

  uint8_t _width = 0;
  uint8_t _height = 0;

  uint8_t* _state = nullptr;
  size_t _stateSize;
  size_t _boxCount = 0;
  size_t _goalCount = 0;

  bool _movedBox = false;

};

} // namespace quickerBan