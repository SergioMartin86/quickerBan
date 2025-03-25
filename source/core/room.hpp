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

  __INLINE__ void print() const
  {
    for(uint8_t i = 0; i < _height; i++)
    {
     for(uint8_t j = 0; j < _width; j++)
     {
       const auto tileType = _background[getIndex(i,j)];
       if (tileType == itemType::wall) printf("#");
       if (tileType == itemType::pusher) printf("@");
       if (tileType == itemType::pusher_on_goal) printf("+");
       if (tileType == itemType::box) printf("$");
       if (tileType == itemType::box_on_goal) printf("*");
       if (tileType == itemType::goal) printf(".");
       if (tileType == itemType::floor) printf(" ");
     } 
     printf("\n");
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
    createBoxMap();
  }

  private:

  void createBoxMap()
  {
    memset(_boxMap, itemType::floor, _height * _width * sizeof(uint8_t));

    for (size_t i = 0; i < _boxCount; i++)
    {
      uint8_t boxPosX = _state[(i+1) * 2 + 0];
      uint8_t boxPosY = _state[(i+1) * 2 + 1];
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