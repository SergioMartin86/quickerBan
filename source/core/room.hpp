#pragma once

#include <cstdint>
#include <cstdio>
#include <jaffarCommon/string.hpp>

#define _ROOM_MAX_WIDTH 256
#define _ROOM_MAX_HEIGHT 256

namespace quickerBan {

class Room
{
  public:

  enum itemType
  {
    floor,
    wall,
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
       const auto tileType = _tiles[i][j];
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

    _height = rowSequence.size();
    for (uint8_t i = 0; i < _height; i++)
    {
        const auto& row = rowSequence[i];
        if (row.size() > _width) _width = (uint8_t) row.size();
        for (uint8_t j = 0; j < _width; j++)
        {
            if (row[j] == ' ' || row[j] == '-' || row[j] == '_') _tiles[i][j] = itemType::floor;
            if (row[j] == '.') _tiles[i][j] = itemType::goal;
            if (row[j] == '*' || row[j] == 'B') _tiles[i][j] = itemType::box_on_goal;
            if (row[j] == 'b' || row[j] == '$') _tiles[i][j] = itemType::box;
            if (row[j] == 'P' || row[j] == '+') _tiles[i][j] = itemType::pusher_on_goal;
            if (row[j] == 'p' || row[j] == '@') _tiles[i][j] = itemType::pusher;
            if (row[j] == '#') _tiles[i][j] = itemType::wall;
        }
    }
  }

  private:

  uint8_t _tiles[_ROOM_MAX_HEIGHT][_ROOM_MAX_WIDTH];
  uint8_t _width = 0;
  uint8_t _height = 0;

};

} // namespace quickerBan