#ifndef GAME_H
#define GAME_H

#include <vector>
#include <string>
#include <memory>
#include "Map.h"
#include "Entity.h"
#include "Skill.h"

// ============================================================================
// Game.h —— 游戏主控类声明
// 负责：
//   - 持有地图、玩家、怪物列表、道具列表
//   - 渲染整个屏幕（地图 + HUD + 战斗日志 + 各类面板）
//   - 处理输入（WASD 移动/攻击 / F 远程瞄准 / T 技能 / I 物品栏 / M 小地图）
//   - 战斗判定、怪物 AI、道具拾取、下楼、经验/升级、技能抽选
//   - 存档/读档（二进制文件 save.dat）
// ============================================================================

// Windows 控制台颜色枚举（对应 SetConsoleTextAttribute 的颜色位）
enum Color : unsigned short {
    COL_DEFAULT = 7,    // 灰白
    COL_WALL = 8,       // 深灰
    COL_FLOOR = 7,      // 灰白
    COL_PLAYER = 11,    // 亮青
    COL_GOBLIN = 10,    // 绿
    COL_SLIME = 2,      // 暗绿
    COL_BAT = 13,       // 紫
    COL_SKELETON = 15,  // 亮白
    COL_BOSS = 12,      // 红（Boss）
    COL_POTION = 12,    // 红
    COL_MANAPOTION = 9, // 蓝（蓝瓶）
    COL_WEAPON = 14,    // 黄
    COL_KEY = 14,       // 黄
    COL_SCROLL = 13,    // 紫（卷轴）
    COL_SHIELD = 11,    // 青（护盾道具）
    COL_CHEST = 14,     // 黄（宝箱）
    COL_STAIRS = 9,     // 蓝
    COL_HUD = 11,       // 青色
    COL_LOG = 8,        // 深灰
    COL_WARN = 12,      // 红
    COL_GOOD = 10,      // 绿
    COL_MP = 9,         // 蓝（法力）
    COL_EXP = 10,       // 绿（经验）
};

// 瞄准模式来源
enum class AimSource {
    Weapon,  // F键远程武器攻击
    Skill,   // 技能远程攻击
};

class Game {
private:
    Map map;
    Player player;
    std::vector<std::unique_ptr<Monster>> monsters;
    std::vector<std::unique_ptr<Item>> items;

    std::vector<std::string> combatLog;  // 战斗日志（最新在尾部）
    bool gameOver;
    bool showMiniMap;
    bool showInventory;
    bool showSkillPanel;     // 技能面板
    bool aimingMode;         // 远程瞄准模式
    AimSource aimSource;     // 瞄准来源（武器或技能）
    int aimSkillIndex;       // 技能瞄准时的技能序号
    LevelType currentLevelType;  // 当前关卡类型
    bool levelEnterPrompt;   // 是否显示关卡进入提示
    std::string message;     // 临时提示信息

    // Windows 控制台句柄
    mutable void* hConsole;  // HANDLE，声明 mutable 以便在 const 方法中调用 Win32 API

    // ---------- 内部辅助 ----------
    void addLog(const std::string& msg);   // 添加日志，最多保留 5 条
    void clearScreen() const;
    void setColor(Color c) const;
    void drawChar(int x, int y, char c, Color col) const;
    void drawText(int x, int y, const std::string& text, Color col) const;
    void clearLine(int x, int y, int len) const;  // 用空格清空一行

    // 在某房间的地板上找一个空闲位置放置实体
    bool findFreeTile(int& x, int& y, bool avoidStairs = true) const;

    // 检查目标位置是否有怪物，返回怪物指针（没有则 nullptr）
    Monster* monsterAt(int x, int y) const;
    // 检查目标位置是否有道具
    Item* itemAt(int x, int y) const;

    // 玩家攻击怪物（近战，含经验奖励）
    void playerAttack(Monster& m);
    // 怪物攻击玩家
    void monsterAttack(Monster& m);

    // 远程射线查找：从玩家出发沿(dx,dy)方向找直线上第一个怪物
    // maxRange=最大射程，outX/outY=命中怪物坐标。返回怪物指针，无则nullptr
    Monster* findFirstMonsterOnLine(int dx, int dy, int maxRange,
                                    int& outX, int& outY) const;

    // 远程攻击执行（武器或技能共用），对命中怪物造成伤害
    void rangedAttack(int dx, int dy, int maxRange, float multiplier,
                      const std::string& attackerName);

    // 使用技能（已选定的技能序号）。若需瞄准则进入aimingMode
    void useSkill(int index);
    // 执行技能效果（瞄准完成后或无需瞄准时调用）
    void executeSkill(const Skill& s);

    // 给予玩家经验并处理升级（含技能抽选面板）
    void grantExp(int amount);
    // 升级技能3选1面板，阻塞等待玩家选择
    void levelUpSkillChoice();

    // 渲染各部分
    void renderMap() const;
    void renderHUD() const;
    void renderLog() const;
    void renderMiniMap() const;
    void renderInventory() const;
    void renderSkillPanel() const;
    void renderAimMode() const;
    void renderMessage() const;
    void renderLevelEnterPrompt() const;
    void renderLevelUpChoices(const std::vector<Skill>& choices) const;

    // 生成新层（根据关卡类型生成不同内容）
    void spawnLevel();
    // 判定下一层关卡类型（随机：15%奖励关,15%Boss关仅level>=3,70%普通）
    LevelType decideLevelType() const;

    // 关卡内具体生成
    void spawnNormalLevel();  // 普通关：怪物+道具+钥匙
    void spawnBonusLevel();   // 奖励关：宝箱，无怪物
    void spawnBossLevel();    // Boss副本：1只Boss

    // 处理一次玩家移动
    void handlePlayerMove(int dx, int dy);

    // 怪物回合
    void monsterTurn();

    // 进入新层时恢复（根据关卡类型）
    void onEnterLevelRestore();

    // 宝箱开启：给予随机奖励
    void openChest(int x, int y);

    // 职业选择（游戏开始时）
    Career chooseCareer();

public:
    Game();
    ~Game();

    // 主循环
    void run();

    // 存档/读档（二进制）
    bool saveGame(const std::string& filename = "save.dat") const;
    bool loadGame(const std::string& filename = "save.dat");
};

#endif // GAME_H
