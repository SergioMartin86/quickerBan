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
       const auto tileType = _tiles[getIndex(i,j)];
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

    // Allocating static room information
    long pageSize = sysconf (_SC_PAGESIZE);
    _tiles = (uint8_t*)aligned_alloc(pageSize, _height * _width * sizeof(uint8_t));

    // Clearing room 
    for (uint8_t i = 0; i < _height; i++)
     for (uint8_t j = 0; j < _width; j++)
     _tiles[getIndex(i,j)] = itemType::floor;
    
     // Parsing from input
    for (uint8_t i = 0; i < _height; i++)
    {
        const auto& row = rowSequence[i];
        for (uint8_t j = 0; j < row.size(); j++)
        {
            if (row[j] == ' ' || row[j] == '-' || row[j] == '_') _tiles[getIndex(i,j)] = itemType::floor;
            if (row[j] == '.') _tiles[getIndex(i,j)] = itemType::goal;
            if (row[j] == '*' || row[j] == 'B') _tiles[getIndex(i,j)] = itemType::box_on_goal;
            if (row[j] == 'b' || row[j] == '$') _tiles[getIndex(i,j)] = itemType::box;
            if (row[j] == 'P' || row[j] == '+') _tiles[getIndex(i,j)] = itemType::pusher_on_goal;
            if (row[j] == 'p' || row[j] == '@') _tiles[getIndex(i,j)] = itemType::pusher;
            if (row[j] == '#') _tiles[getIndex(i,j)] = itemType::wall;
        }
    }
  }

  private:

  __INLINE__ uint16_t getIndex(const uint8_t i, const uint8_t j) const { return (uint16_t)i * (uint16_t)_width + (uint16_t)j; }

  uint8_t* _tiles = nullptr;
  uint8_t _width = 0;
  uint8_t _height = 0;
  size_t _boxCount = 0;

};

} // namespace quickerBan