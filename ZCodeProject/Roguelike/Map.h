#ifndef MAP_H
#define MAP_H

#include <vector>
#include <string>
#include "Entity.h"   // MAP_WIDTH / MAP_HEIGHT 常量

// ============================================================================
// Map.h —— 地图类声明
// 负责地牢的随机生成：房间 + 走廊 + 楼梯
// 地图字符约定：
//   '#' 墙壁
//   '.' 地板（可通行）
//   '>' 下行楼梯
//   ' ' 未探索区域（实际上我们用墙填充外围，内部全部生成）
// ============================================================================

// 房间结构体
struct Room {
    int x, y;   // 房间左上角坐标
    int w, h;   // 宽、高
    int cx() const { return x + w / 2; }  // 中心 x
    int cy() const { return y + h / 2; }  // 中心 y
};

class Map {
private:
    int width, height;
    std::vector<std::string> grid;  // 二维字符网格
    std::vector<Room> rooms;        // 本层所有房间
    int stairsX, stairsY;           // 楼梯坐标

public:
    Map(int w = MAP_WIDTH, int h = MAP_HEIGHT);

    // 重新生成一层地牢
    void generate();

    // 访问地图
    char at(int x, int y) const;          // 越界返回 '#'
    void set(int x, int y, char c);
    bool isWalkable(int x, int y) const;   // 是否可通行（'.' 或 '>'）

    // 房间访问（用于放置实体）
    const std::vector<Room>& getRooms() const { return rooms; }
    const Room& getRoom(size_t i) const { return rooms[i]; }
    size_t roomCount() const { return rooms.size(); }

    // 楼梯
    int getStairsX() const { return stairsX; }
    int getStairsY() const { return stairsY; }
    void setStairs(int x, int y) { stairsX = x; stairsY = y; }  // 读档用

    int getWidth() const { return width; }
    int getHeight() const { return height; }
};

#endif // MAP_H
