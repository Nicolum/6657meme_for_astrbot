#include "Map.h"
#include "Entity.h"   // MAP_WIDTH/HEIGHT
#include <cstdlib>    // rand()
#include <algorithm>  // min/max

// ============================================================================
// Map.cpp —— 地图生成实现
// 算法思路：
//   1. 初始化整张图为墙壁 '#'
//   2. 随机生成若干个不重叠的房间，挖成地板 '.'
//   3. 用 L 形走廊连接相邻房间（按生成顺序）
//   4. 在最后一个房间放置下行楼梯 '>'
// ============================================================================

Map::Map(int w, int h) : width(w), height(h), stairsX(0), stairsY(0) {
    generate();
}

// 在 (x1,y1) 到 (x2,y2) 之间挖一条水平/垂直走廊
static void carveCorridor(std::vector<std::string>& grid, int x1, int y1, int x2, int y2) {
    // 先水平后垂直（L 形）
    int startX = std::min(x1, x2);
    int endX = std::max(x1, x2);
    for (int x = startX; x <= endX; ++x) {
        grid[y1][x] = '.';
    }
    int startY = std::min(y1, y2);
    int endY = std::max(y1, y2);
    for (int y = startY; y <= endY; ++y) {
        grid[y][x2] = '.';
    }
}

// 检查两个矩形（房间）是否重叠，留 1 格间隙
static bool roomsOverlap(const Room& a, const Room& b) {
    return !(a.x + a.w + 1 < b.x ||
             b.x + b.w + 1 < a.x ||
             a.y + a.h + 1 < b.y ||
             b.y + b.h + 1 < a.y);
}

void Map::generate() {
    // 1. 全部初始化为墙
    grid.assign(height, std::string(width, '#'));
    rooms.clear();

    // 2. 尝试生成房间（目标 6~10 个）
    int maxTries = 80;
    int targetRooms = 6 + rand() % 5;  // 6~10
    for (int t = 0; t < maxTries && (int)rooms.size() < targetRooms; ++t) {
        int rw = 4 + rand() % 7;   // 宽 4~10
        int rh = 3 + rand() % 5;   // 高 3~7
        int rx = 1 + rand() % (width - rw - 2);
        int ry = 1 + rand() % (height - rh - 2);

        Room r{rx, ry, rw, rh};
        // 检查与已有房间是否重叠
        bool ok = true;
        for (const auto& exist : rooms) {
            if (roomsOverlap(r, exist)) { ok = false; break; }
        }
        if (!ok) continue;

        // 挖出房间
        for (int y = ry; y < ry + rh; ++y) {
            for (int x = rx; x < rx + rw; ++x) {
                grid[y][x] = '.';
            }
        }
        rooms.push_back(r);
    }

    // 3. 连接所有房间：按顺序，相邻房间用 L 形走廊相连
    for (size_t i = 1; i < rooms.size(); ++i) {
        const Room& prev = rooms[i - 1];
        const Room& cur = rooms[i];
        carveCorridor(grid, prev.cx(), prev.cy(), cur.cx(), cur.cy());
    }

    // 4. 在最后一个房间中心放置楼梯
    if (!rooms.empty()) {
        const Room& last = rooms.back();
        stairsX = last.cx();
        stairsY = last.cy();
        grid[stairsY][stairsX] = '>';
    } else {
        // 极端情况：没有生成任何房间，放一个默认楼梯
        stairsX = width / 2;
        stairsY = height / 2;
        grid[stairsY][stairsX] = '>';
    }
}

char Map::at(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) return '#';
    return grid[y][x];
}

void Map::set(int x, int y, char c) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    grid[y][x] = c;
}

bool Map::isWalkable(int x, int y) const {
    char c = at(x, y);
    return c == '.' || c == '>';
}
