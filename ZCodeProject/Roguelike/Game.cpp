#include "Game.h"

#include <windows.h>
#include <conio.h>    // _kbhit, _getch
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

// ============================================================================
// Game.cpp —— 游戏主控实现
// ============================================================================

// ---------- 存档文件魔数，用于校验文件有效性 ----------
static const char SAVE_MAGIC[4] = {'R','G','L','K'};
static const int SAVE_VERSION = 2;  // v2: 新增职业/法力/护盾/经验/技能/武器/关卡类型

// ---------- 辅助：把方向键的二次读取码映射为 wasd ----------
// 功能键(F5/F7等)以0x00前缀发送扫描码，方向键以0xE0前缀发送。
// 为避免F5扫描码0x74与字母't'(116)冲突，将F5/F7映射到不与字母冲突的特殊值。
enum SpecialKey : int {
    KEY_F5 = 1000,  // 存档
    KEY_F7 = 1001,  // 读档
};
static int normalizeArrowKey(int ch) {
    if (ch == 0xE0) {
        // 方向键前缀
        ch = _getch();
        switch (ch) {
            case 72: return 'w';  // Up
            case 80: return 's';  // Down
            case 75: return 'a';  // Left
            case 77: return 'd';  // Right
        }
        return 0;  // 其它扩展键忽略
    }
    if (ch == 0) {
        // 功能键前缀(0x00)：读取扫描码
        ch = _getch();
        switch (ch) {
            case 0x74: return KEY_F5;  // F5
            case 0x76: return KEY_F7;  // F7
        }
        return 0;  // 其它功能键忽略
    }
    return ch;
}

Game::Game()
    : player(1, 1, Career::Warrior), gameOver(false), showMiniMap(false),
      showInventory(false), showSkillPanel(false), aimingMode(false),
      aimSource(AimSource::Weapon), aimSkillIndex(-1),
      currentLevelType(LevelType::Normal), levelEnterPrompt(false),
      hConsole(nullptr) {
    // 初始化随机数种子
    srand((unsigned)time(nullptr));

    // 获取 Windows 标准输出句柄
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    // 隐藏光标，避免闪烁
    CONSOLE_CURSOR_INFO ci;
    ci.bVisible = FALSE;
    ci.dwSize = 1;
    SetConsoleCursorInfo(hConsole, &ci);

    // 设置控制台窗口大小，确保能容下整个画面
    system("mode con cols=80 lines=32");

    // 职业选择
    Career c = chooseCareer();
    // 用 placement new 不现实，改为直接重新构造 player（Player 是成员，用赋值）
    player = Player(1, 1, c);

    // 根据职业赋予初始技能
    switch (c) {
        case Career::Warrior:
            if (const Skill* s = getSkillTemplate(1)) player.addSkill(*s);  // 重击
            break;
        case Career::Mage:
            if (const Skill* s = getSkillTemplate(5)) player.addSkill(*s);  // 火球术
            break;
        case Career::Ranger:
            if (const Skill* s = getSkillTemplate(9)) player.addSkill(*s);  // 精准射击
            break;
    }

    // 生成第 1 层（第一层强制为普通关）
    currentLevelType = LevelType::Normal;
    spawnLevel();
}

Game::~Game() {
    if (hConsole) {
        CONSOLE_CURSOR_INFO ci;
        ci.bVisible = TRUE;
        ci.dwSize = 10;
        SetConsoleCursorInfo(hConsole, &ci);
    }
}

// -------------------- 职业选择 --------------------
Career Game::chooseCareer() {
    // 清屏后显示职业选择菜单
    system("cls");
    setColor(COL_HUD);
    std::cout << "========================================\n";
    std::cout << "      选择你的职业\n";
    std::cout << "========================================\n";
    setColor(COL_GOOD);
    std::cout << "[1] 战士\n";
    setColor(COL_DEFAULT);
    std::cout << "    HP40/MP10  攻6防3  近战铁剑\n";
    std::cout << "    初始技能：重击(近战2倍伤害)\n";
    std::cout << "    定位：正面硬刚，高生存\n\n";

    setColor(COL_MANAPOTION);
    std::cout << "[2] 法师\n";
    setColor(COL_DEFAULT);
    std::cout << "    HP24/MP30  攻4防1  远程法杖\n";
    std::cout << "    初始技能：火球术(远程直线1.8倍)\n";
    std::cout << "    定位：远程爆发，脆皮高蓝\n\n";

    setColor(COL_HUD);
    std::cout << "[3] 游侠\n";
    setColor(COL_DEFAULT);
    std::cout << "    HP30/MP18  攻5防2  远程弓\n";
    std::cout << "    初始技能：精准射击(远程直线1.6倍)\n";
    std::cout << "    定位：均衡远程，灵活机动\n\n";

    setColor(COL_WARN);
    std::cout << "按 1/2/3 选择职业: ";
    setColor(COL_DEFAULT);

    while (true) {
        int ch = _getch();
        if (ch == '1') return Career::Warrior;
        if (ch == '2') return Career::Mage;
        if (ch == '3') return Career::Ranger;
    }
}

// -------------------- 日志与清屏 --------------------
void Game::addLog(const std::string& msg) {
    combatLog.push_back(msg);
    while (combatLog.size() > 5) {
        combatLog.erase(combatLog.begin());
    }
}

void Game::clearScreen() const {
    COORD coord{0, 0};
    SetConsoleCursorPosition(hConsole, coord);
}

void Game::setColor(Color c) const {
    SetConsoleTextAttribute(hConsole, (WORD)c);
}

void Game::drawChar(int x, int y, char c, Color col) const {
    COORD coord;
    coord.X = (SHORT)x;
    coord.Y = (SHORT)y;
    SetConsoleCursorPosition(hConsole, coord);
    setColor(col);
    putchar(c);
}

void Game::drawText(int x, int y, const std::string& text, Color col) const {
    COORD coord;
    coord.X = (SHORT)x;
    coord.Y = (SHORT)y;
    SetConsoleCursorPosition(hConsole, coord);
    setColor(col);
    std::cout << text;
}

void Game::clearLine(int x, int y, int len) const {
    COORD coord;
    coord.X = (SHORT)x;
    coord.Y = (SHORT)y;
    SetConsoleCursorPosition(hConsole, coord);
    setColor(COL_DEFAULT);
    for (int i = 0; i < len; ++i) putchar(' ');
}

// -------------------- 寻找空闲位置 --------------------
bool Game::findFreeTile(int& x, int& y, bool avoidStairs) const {
    for (int t = 0; t < 100; ++t) {
        if (map.roomCount() == 0) return false;
        size_t ri = rand() % map.roomCount();
        const Room& r = map.getRoom(ri);
        x = r.x + rand() % r.w;
        y = r.y + rand() % r.h;
        if (!map.isWalkable(x, y)) continue;
        if (x == player.getX() && y == player.getY()) continue;
        if (monsterAt(x, y)) continue;
        if (itemAt(x, y)) continue;
        if (avoidStairs && x == map.getStairsX() && y == map.getStairsY()) continue;
        return true;
    }
    return false;
}

Monster* Game::monsterAt(int x, int y) const {
    for (auto& m : monsters) {
        if (m->isAlive() && m->getX() == x && m->getY() == y) return m.get();
    }
    return nullptr;
}

Item* Game::itemAt(int x, int y) const {
    for (auto& it : items) {
        if (it->getX() == x && it->getY() == y) return it.get();
    }
    return nullptr;
}

// -------------------- 战斗 --------------------
void Game::playerAttack(Monster& m) {
    int dmg = m.takeDamage(player.getEffectiveAttack());
    std::ostringstream oss;
    oss << "你攻击" << m.getName() << "，造成 " << dmg << " 伤害";
    addLog(oss.str());
    if (!m.isAlive()) {
        std::ostringstream oss2;
        oss2 << m.getName() << "被击败！获得 " << m.getExpReward() << " 经验";
        addLog(oss2.str());
        // Boss击杀后楼梯解锁（移除钥匙需求，由下楼逻辑判断关卡类型处理）
        grantExp(m.getExpReward());
    }
}

void Game::monsterAttack(Monster& m) {
    int atk = m.getAttack();
    // Boss蓄力重击：2倍伤害
    if (m.getType() == MonsterType::Boss && m.isCharging()) {
        atk *= 2;
        addLog(m.getName() + "释放蓄力重击！");
        m.setCharging(false);
    }
    int dmg = player.takeDamage(atk);
    std::ostringstream oss;
    oss << m.getName() << "攻击你，造成 " << dmg << " 伤害";
    addLog(oss.str());
}

// -------------------- 远程射线攻击 --------------------
Monster* Game::findFirstMonsterOnLine(int dx, int dy, int maxRange,
                                      int& outX, int& outY) const {
    int cx = player.getX();
    int cy = player.getY();
    for (int step = 1; step <= maxRange; ++step) {
        int nx = cx + dx * step;
        int ny = cy + dy * step;
        // 遇到墙则射线停止
        if (!map.isWalkable(nx, ny)) return nullptr;
        Monster* m = monsterAt(nx, ny);
        if (m) {
            outX = nx;
            outY = ny;
            return m;
        }
    }
    return nullptr;
}

void Game::rangedAttack(int dx, int dy, int maxRange, float multiplier,
                        const std::string& attackerName) {
    int hitX, hitY;
    Monster* m = findFirstMonsterOnLine(dx, dy, maxRange, hitX, hitY);
    if (!m) {
        addLog(attackerName + "射偏了，未命中任何目标");
        return;
    }
    int dmg = m->takeDamage((int)(player.getEffectiveAttack() * multiplier));
    std::ostringstream oss;
    oss << attackerName << "命中" << m->getName() << "，造成 " << dmg << " 伤害";
    addLog(oss.str());
    if (!m->isAlive()) {
        std::ostringstream oss2;
        oss2 << m->getName() << "被击败！获得 " << m->getExpReward() << " 经验";
        addLog(oss2.str());
        grantExp(m->getExpReward());
    }
}

// -------------------- 技能 --------------------
void Game::useSkill(int index) {
    auto& skills = player.getSkillsMut();
    if (index < 0 || index >= (int)skills.size()) return;
    Skill& s = skills[index];
    // 检查冷却
    if (s.cooldownLeft > 0) {
        message = s.name + " 还在冷却中（剩余" + std::to_string(s.cooldownLeft) + "回合）";
        return;
    }
    // 检查MP
    if (!player.spendMP(s.mpCost)) {
        message = "法力不足，无法施放 " + s.name;
        return;
    }
    // 远程直线技能需要瞄准
    if (s.effect == SkillEffect::DamageLine) {
        // 进入瞄准模式
        aimingMode = true;
        aimSource = AimSource::Skill;
        aimSkillIndex = index;
        message = "选择" + s.name + "的施放方向 (WASD)，ESC取消";
        return;  // 等待方向输入
    }
    // 无需瞄准，直接执行
    executeSkill(s);
    // 扣冷却
    s.cooldownLeft = s.cooldown;
    // 消耗一回合
    player.tickBuff();
    player.tickSkillCooldowns();
    monsterTurn();
}

void Game::executeSkill(const Skill& s) {
    // 获取玩家技能列表中的实际引用（因为参数是值传递的副本）
    // 这里通过 id 查找
    std::ostringstream oss;
    float mult = s.power / 10.0f;  // power存的是倍率*10
    switch (s.effect) {
        case SkillEffect::DamageMelee: {
            // 攻击相邻的怪物：取玩家朝向？这里简化为攻击所有相邻怪物中最近的一个
            // 实际上近战技能应该有方向。简化：攻击相邻4格中第一个有怪物的
            int dirs[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
            bool hit = false;
            for (auto& d : dirs) {
                Monster* m = monsterAt(player.getX() + d[0], player.getY() + d[1]);
                if (m) {
                    int dmg = m->takeDamage((int)(player.getEffectiveAttack() * mult));
                    oss << s.name << "命中" << m->getName() << "，造成 " << dmg << " 伤害";
                    addLog(oss.str());
                    // 吸血效果（id=12）
                    if (s.id == 12) {
                        player.heal(dmg / 2);
                        addLog("吸取了 " + std::to_string(dmg / 2) + " 点生命");
                    }
                    if (!m->isAlive()) {
                        addLog(m->getName() + "被击败！获得 " + std::to_string(m->getExpReward()) + " 经验");
                        grantExp(m->getExpReward());
                    }
                    hit = true;
                    break;
                }
            }
            if (!hit) addLog(s.name + "未命中任何目标（需靠近敌人）");
            break;
        }
        case SkillEffect::DamageLine:
            // 瞄准模式下已在 useSkill 中处理进入瞄准，这里不该被调用
            // 但若直接调用，默认向上射
            break;
        case SkillEffect::AOE: {
            // 攻击周围8格所有怪物
            int dirs[8][2] = {{-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}};
            int killed = 0;
            int totalDmg = 0;
            for (auto& d : dirs) {
                Monster* m = monsterAt(player.getX() + d[0], player.getY() + d[1]);
                if (m) {
                    int dmg = m->takeDamage((int)(player.getEffectiveAttack() * mult));
                    totalDmg += dmg;
                    if (!m->isAlive()) {
                        ++killed;
                        grantExp(m->getExpReward());
                    }
                }
            }
            oss << s.name << "造成范围伤害，总计 " << totalDmg;
            if (killed > 0) oss << "，击杀 " << killed;
            addLog(oss.str());
            break;
        }
        case SkillEffect::Heal:
            player.heal(s.power);
            addLog(s.name + "：恢复 " + std::to_string(s.power) + " 点生命");
            break;
        case SkillEffect::Shield:
            player.addShield(s.power);
            addLog(s.name + "：获得 " + std::to_string(s.power) + " 点护盾");
            break;
        case SkillEffect::BuffAtk:
            player.applyBuffAtk(s.power, 6);
            oss << s.name << "：攻击力+" << s.power << "，持续6回合";
            addLog(oss.str());
            break;
    }
}

// -------------------- 经验与升级 --------------------
void Game::grantExp(int amount) {
    bool leveled = player.grantExp(amount);
    if (leveled) {
        // 可能连续升级，每次升级都触发技能选择
        while (true) {
            addLog("=== 升级！当前等级 " + std::to_string(player.getPlayerLevel()) + " ===");
            // 升级面板
            levelUpSkillChoice();
            // 检查是否还能继续升（grantExp在Player内部已处理多级，这里只做一次面板）
            // 由于Player::grantExp已一次性处理完所有连续升级，这里只弹一次面板
            break;
        }
    }
}

void Game::levelUpSkillChoice() {
    auto choices = rollSkillChoices(player.getSkills(), player.getCareer());
    if (choices.empty()) {
        addLog("没有可学的新技能，仅提升属性");
        return;
    }
    // 渲染选择面板并等待输入
    bool chosen = false;
    while (!chosen) {
        clearScreen();
        renderMap();
        renderHUD();
        renderLog();
        renderLevelUpChoices(choices);
        int ch = _getch();
        if (ch >= '1' && ch <= '3') {
            int idx = ch - '1';
            if (idx < (int)choices.size()) {
                const Skill& pick = choices[idx];
                // 检查是否已学
                auto& skills = player.getSkillsMut();
                bool found = false;
                for (auto& ps : skills) {
                    if (ps.id == pick.id) {
                        // 升级已有技能：提升威力或降低消耗
                        ps.learnLevel++;
                        ps.power += 3;  // 每级+3威力(即+0.3倍率或+3固定值)
                        if (ps.mpCost > 1) ps.mpCost -= 1;  // 每级-1消耗
                        addLog(pick.name + " 升级至 " + std::to_string(ps.learnLevel) + " 级！");
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    // 学习新技能
                    Skill ns = pick;
                    ns.learnLevel = 1;
                    ns.cooldownLeft = 0;
                    player.addSkill(ns);
                    addLog("学会了新技能：" + ns.name + "！");
                }
                chosen = true;
            }
        }
    }
}

// -------------------- 生成新层 --------------------
LevelType Game::decideLevelType() const {
    // 第1层强制普通
    if (player.getLevel() <= 1) return LevelType::Normal;
    int r = rand() % 100;
    // 奖励关15%，Boss关15%（仅level>=3），其余普通
    if (r < 15) return LevelType::Bonus;
    if (r < 30 && player.getLevel() >= 3) return LevelType::Boss;
    return LevelType::Normal;
}

void Game::spawnLevel() {
    // 重新生成地图
    map.generate();
    monsters.clear();
    items.clear();

    // 把玩家放到第一个房间中心
    if (map.roomCount() > 0) {
        const Room& first = map.getRoom(0);
        player.setPos(first.cx(), first.cy());
    } else {
        player.setPos(1, 1);
    }

    // 根据关卡类型生成内容
    switch (currentLevelType) {
        case LevelType::Normal: spawnNormalLevel(); break;
        case LevelType::Bonus:  spawnBonusLevel(); break;
        case LevelType::Boss:   spawnBossLevel(); break;
    }

    // 进入新层恢复
    onEnterLevelRestore();

    // 关卡类型提示
    levelEnterPrompt = true;
}

void Game::spawnNormalLevel() {
    // 生成 5~8 个怪物
    int monsterCount = 5 + rand() % 4;
    for (int i = 0; i < monsterCount; ++i) {
        int mx, my;
        if (!findFreeTile(mx, my)) break;
        MonsterType t;
        int r = rand() % 4;
        if (player.getLevel() >= 3 && rand() % 100 < 25) {
            t = MonsterType::Skeleton;
        } else {
            t = (MonsterType)r;
        }
        monsters.push_back(std::make_unique<Monster>(mx, my, t, player.getLevel()));
        // 深层怪物加强：同时设置maxHp
        if (player.getLevel() > 1) {
            int bonus = player.getLevel() - 1;
            monsters.back()->setHP(monsters.back()->getHP() + bonus);
            monsters.back()->setMaxHP(monsters.back()->getMaxHP() + bonus);
        }
    }

    // 生成道具：3~5 个（比原来多，缓解补给不足）
    int itemCount = 3 + rand() % 3;
    for (int i = 0; i < itemCount; ++i) {
        int ix, iy;
        if (!findFreeTile(ix, iy)) break;
        int r = rand() % 6;  // 6种道具加权
        if (r == 0) {
            int val = 8 + rand() % 8;
            items.push_back(std::make_unique<Item>(ix, iy, ItemType::Potion, val));
        } else if (r == 1) {
            int val = 6 + rand() % 6;
            items.push_back(std::make_unique<Item>(ix, iy, ItemType::ManaPotion, val));
        } else if (r == 2) {
            // 武器：根据层数决定武器类型和加成
            int atkBonus = 1 + rand() % 3 + player.getLevel() / 3;
            WeaponKind wk;
            std::string wname;
            int wr = rand() % 4;
            switch (wr) {
                case 0: wk = WeaponKind::Melee; wname = "暴风之剑" + std::to_string(atkBonus); break;
                case 1: wk = WeaponKind::Spear; wname = "十字镐" + std::to_string(atkBonus); break;
                case 2: wk = WeaponKind::Bow;   wname = "反曲之弓" + std::to_string(atkBonus); break;
                default: wk = WeaponKind::Staff; wname = "无用大棒" + std::to_string(atkBonus); break;
            }
            items.push_back(std::make_unique<Item>(ix, iy, ItemType::Weapon, atkBonus, wk, wname));
        } else if (r == 3) {
            // 卷轴：提升maxHP 3~6
            int val = 3 + rand() % 4;
            items.push_back(std::make_unique<Item>(ix, iy, ItemType::Scroll, val));
        } else if (r == 4) {
            // 护盾道具：+5~10护盾
            int val = 5 + rand() % 6;
            items.push_back(std::make_unique<Item>(ix, iy, ItemType::Shield, val));
        } else {
            items.push_back(std::make_unique<Item>(ix, iy, ItemType::Key, 1));
        }
    }

    // 保证至少1把钥匙
    bool hasKey = false;
    for (auto& it : items) {
        if (it->getType() == ItemType::Key) { hasKey = true; break; }
    }
    if (!hasKey) {
        int ix, iy;
        if (findFreeTile(ix, iy)) {
            items.push_back(std::make_unique<Item>(ix, iy, ItemType::Key, 1));
        }
    }

    addLog("你进入了第 " + std::to_string(player.getLevel()) + " 层地牢 [普通关]");
}

void Game::spawnBonusLevel() {
    // 无怪物，放置 4~6 个宝箱
    int chestCount = 4 + rand() % 3;
    for (int i = 0; i < chestCount; ++i) {
        int ix, iy;
        if (!findFreeTile(ix, iy)) break;
        items.push_back(std::make_unique<Item>(ix, iy, ItemType::Chest, 0));
    }
    addLog("你进入了第 " + std::to_string(player.getLevel()) + " 层 [奖励关！无怪物，拾取宝箱]");
}

void Game::spawnBossLevel() {
    // 1只Boss
    int bx, by;
    if (findFreeTile(bx, by)) {
        monsters.push_back(std::make_unique<Monster>(bx, by, MonsterType::Boss, player.getLevel()));
    }
    addLog("你进入了第 " + std::to_string(player.getLevel()) + " 层 [Boss副本！击败巨龙！]");
}

void Game::onEnterLevelRestore() {
    switch (currentLevelType) {
        case LevelType::Normal:
            // 普通关：恢复30%最大HP + 20%最大MP
            player.heal(player.getMaxHP() * 3 / 10);
            player.restoreMP(player.getMaxMP() * 2 / 10);
            break;
        case LevelType::Bonus:
            // 奖励关：回满HP/MP，护盾+5
            player.heal(player.getMaxHP());
            player.restoreMP(player.getMaxMP());
            player.addShield(5);
            break;
        case LevelType::Boss:
            // Boss关：不恢复
            break;
    }
}

void Game::openChest(int x, int y) {
    // 宝箱随机奖励
    int r = rand() % 7;
    std::ostringstream oss;
    switch (r) {
        case 0: {
            int val = 12 + rand() % 8;
            player.heal(val);
            oss << "宝箱开出大血瓶，恢复 " << val << " HP";
            break;
        }
        case 1: {
            int val = 10 + rand() % 8;
            player.restoreMP(val);
            oss << "宝箱开出大蓝瓶，恢复 " << val << " MP";
            break;
        }
        case 2: {
            int val = 4 + rand() % 4;
            player.increaseMaxHP(val);
            oss << "宝箱开出生命卷轴，最大HP+" << val;
            break;
        }
        case 3: {
            int val = 8 + rand() % 6;
            player.addShield(val);
            oss << "宝箱开出护盾，护盾+" << val;
            break;
        }
        case 4: {
            int atkBonus = 2 + rand() % 3 + player.getLevel() / 2;
            WeaponKind wk;
            std::string wname;
            int wr = rand() % 4;
            switch (wr) {
                case 0: wk = WeaponKind::Melee; wname = "无尽之刃" + std::to_string(atkBonus); break;
                case 1: wk = WeaponKind::Spear; wname = "破舰者" + std::to_string(atkBonus); break;
                case 2: wk = WeaponKind::Bow;   wname = "路安娜的飓风" + std::to_string(atkBonus); break;
                default: wk = WeaponKind::Staff; wname = "大天使之杖" + std::to_string(atkBonus); break;
            }
            player.equipWeapon(wname, wk, atkBonus);
            oss << "宝箱开出" << wname;
            break;
        }
        case 5: {
            player.addKey();
            oss << "宝箱出钥匙x1";
            break;
        }
        default: {
            int val = 15 + rand() % 10;
            player.grantExp(val);
            oss << "宝箱出经验+" << val;
            break;
        }
    }
    addLog(oss.str());
    message = oss.str();
}

// -------------------- 玩家移动 --------------------
void Game::handlePlayerMove(int dx, int dy) {
    int nx = player.getX() + dx;
    int ny = player.getY() + dy;

    // 1. 检查是否撞到怪物 -> 近战攻击
    Monster* m = monsterAt(nx, ny);
    if (m) {
        playerAttack(*m);
        return;
    }

    // 2. 检查可通行
    if (!map.isWalkable(nx, ny)) return;

    // 3. 检查楼梯
    if (nx == map.getStairsX() && ny == map.getStairsY()) {
        // 普通关需钥匙，奖励关/Boss关无需钥匙（Boss关需先击败Boss）
        if (currentLevelType == LevelType::Normal) {
            if (player.getKeys() > 0) {
                player.useKey();
                addLog("用钥匙打开楼梯门，向下一层进发！");
                player.nextLevel();
                currentLevelType = decideLevelType();
                spawnLevel();
                return;
            } else {
                message = "楼梯被锁住了，需要钥匙(K)才能开门！";
                return;
            }
        } else if (currentLevelType == LevelType::Boss) {
            // Boss关：必须击败Boss才能下楼
            bool bossAlive = false;
            for (auto& mon : monsters) {
                if (mon->getType() == MonsterType::Boss && mon->isAlive()) {
                    bossAlive = true; break;
                }
            }
            if (bossAlive) {
                message = "Boss还活着，无法下楼！";
                return;
            }
            addLog("击败Boss，楼梯已解锁！向下一层进发！");
            player.nextLevel();
            currentLevelType = decideLevelType();
            spawnLevel();
            return;
        } else {
            // 奖励关：直接下楼
            addLog("离开奖励关，向下一层进发！");
            player.nextLevel();
            currentLevelType = decideLevelType();
            spawnLevel();
            return;
        }
    }

    // 4. 移动
    player.setPos(nx, ny);

    // 5. 拾取道具
    Item* it = itemAt(nx, ny);
    if (it) {
        // 宝箱直接开启
        if (it->getType() == ItemType::Chest) {
            openChest(nx, ny);
        } else if (it->getType() == ItemType::Key) {
            player.addKey();
            addLog("拾取了钥匙");
            message = "拾取了钥匙";
        } else {
            // 其它道具加入物品栏
            std::ostringstream oss;
            std::string displayName = it->getItemName().empty() ? it->getName() : it->getItemName();
            player.addItem(it->getType(), displayName, it->getValue(), it->getWeaponKind());
            switch (it->getType()) {
                case ItemType::Potion:
                    oss << "拾取了血瓶(恢复" << it->getValue() << "HP)";
                    break;
                case ItemType::ManaPotion:
                    oss << "拾取了蓝瓶(恢复" << it->getValue() << "MP)";
                    break;
                case ItemType::Weapon:
                    oss << "拾取了" << displayName << "(攻击+" << it->getValue() << ")";
                    break;
                case ItemType::Scroll:
                    oss << "拾取了生命卷轴(maxHP+" << it->getValue() << ")";
                    break;
                case ItemType::Shield:
                    oss << "拾取了护盾(护盾+" << it->getValue() << ")";
                    break;
                default:
                    oss << "拾取了" << displayName;
                    break;
            }
            addLog(oss.str());
            message = oss.str();
        }
        // 从地图移除
        items.erase(std::remove_if(items.begin(), items.end(),
                                   [&](const std::unique_ptr<Item>& p) {
                                       return p->getX() == nx && p->getY() == ny;
                                   }),
                    items.end());
    }
}

// -------------------- 怪物回合 --------------------
void Game::monsterTurn() {
    for (auto& m : monsters) {
        if (!m->isAlive()) continue;

        // Boss蓄力判定：相邻玩家时有30%概率蓄力（不移动，下回合重击）
        if (m->getType() == MonsterType::Boss && !m->isCharging()) {
            int dist = std::abs(m->getX() - player.getX()) + std::abs(m->getY() - player.getY());
            if (dist <= 1 && rand() % 100 < 30) {
                m->setCharging(true);
                addLog(m->getName() + "开始蓄力，下回合将重击！");
                continue;  // 本回合不行动
            }
        }

        int dx, dy;
        m->decideMove(player.getX(), player.getY(), dx, dy);
        if (dx == 0 && dy == 0) continue;

        int nx = m->getX() + dx;
        int ny = m->getY() + dy;

        if (nx == player.getX() && ny == player.getY()) {
            monsterAttack(*m);
            continue;
        }
        if (!map.isWalkable(nx, ny)) continue;
        if (monsterAt(nx, ny)) continue;
        m->setPos(nx, ny);
    }

    // 清理死亡怪物
    monsters.erase(std::remove_if(monsters.begin(), monsters.end(),
                                  [](const std::unique_ptr<Monster>& p) { return !p->isAlive(); }),
                   monsters.end());
}

// -------------------- 渲染 --------------------
void Game::renderMap() const {
    for (int y = 0; y < map.getHeight(); ++y) {
        for (int x = 0; x < map.getWidth(); ++x) {
            char c = map.at(x, y);
            Color col = COL_DEFAULT;
            if (c == '#') col = COL_WALL;
            else if (c == '.') col = COL_FLOOR;
            else if (c == '>') col = COL_STAIRS;

            if (player.getX() == x && player.getY() == y) {
                c = player.getSymbol();
                col = COL_PLAYER;
            } else if (Monster* m = monsterAt(x, y)) {
                c = m->getSymbol();
                switch (m->getType()) {
                    case MonsterType::Goblin:   col = COL_GOBLIN; break;
                    case MonsterType::Slime:    col = COL_SLIME; break;
                    case MonsterType::Bat:      col = COL_BAT; break;
                    case MonsterType::Skeleton: col = COL_SKELETON; break;
                    case MonsterType::Boss:     col = COL_BOSS; break;
                }
            } else if (Item* it = itemAt(x, y)) {
                c = it->getSymbol();
                switch (it->getType()) {
                    case ItemType::Potion:     col = COL_POTION; break;
                    case ItemType::ManaPotion: col = COL_MANAPOTION; break;
                    case ItemType::Weapon:     col = COL_WEAPON; break;
                    case ItemType::Key:        col = COL_KEY; break;
                    case ItemType::Scroll:     col = COL_SCROLL; break;
                    case ItemType::Shield:     col = COL_SHIELD; break;
                    case ItemType::Chest:      col = COL_CHEST; break;
                }
            }
            drawChar(x, y, c, col);
        }
    }
}

void Game::renderHUD() const {
    int baseX = MAP_WIDTH + 2;
    int baseY = 0;

    // 标题行：职业 + 关卡类型
    drawText(baseX, baseY, "===== 状态 =====", COL_HUD);

    drawText(baseX, baseY + 1,
             std::string("职业:") + careerName(player.getCareer()), COL_HUD);

    // 关卡类型
    const char* ltStr = "[普通关]";
    Color ltCol = COL_HUD;
    if (currentLevelType == LevelType::Bonus) { ltStr = "[奖励关]"; ltCol = COL_GOOD; }
    else if (currentLevelType == LevelType::Boss) { ltStr = "[Boss副本]"; ltCol = COL_WARN; }
    drawText(baseX, baseY + 2,
             "第" + std::to_string(player.getLevel()) + "层 " + ltStr, ltCol);

    drawText(baseX, baseY + 3,
             "等级: Lv" + std::to_string(player.getPlayerLevel()), COL_GOOD);

    // HP（含护盾显示）
    std::string hpStr = "HP: " + std::to_string(player.getHP()) + "/" + std::to_string(player.getMaxHP());
    if (player.getShield() > 0) {
        hpStr += " (护盾" + std::to_string(player.getShield()) + ")";
    }
    drawText(baseX, baseY + 4, hpStr, COL_POTION);

    // MP
    drawText(baseX, baseY + 5,
             "MP: " + std::to_string(player.getMP()) + "/" + std::to_string(player.getMaxMP()), COL_MP);

    // 经验
    drawText(baseX, baseY + 6,
             "EXP: " + std::to_string(player.getExp()) + "/" + std::to_string(player.getExpToNext()), COL_EXP);

    // 攻击（含武器+增益）
    std::string atkStr = "攻击: " + std::to_string(player.getAttack());
    if (player.getWeaponBonus() > 0) atkStr += "(武器+" + std::to_string(player.getWeaponBonus()) + ")";
    if (player.getBuffAtk() > 0) atkStr += "(增益+" + std::to_string(player.getBuffAtk()) + ")";
    drawText(baseX, baseY + 7, atkStr, COL_DEFAULT);

    drawText(baseX, baseY + 8, "防御: " + std::to_string(player.getDefense()), COL_DEFAULT);

    // 武器
    drawText(baseX, baseY + 9, "武器: " + player.getEquippedWeaponName(), COL_WEAPON);

    drawText(baseX, baseY + 10, "钥匙: " + std::to_string(player.getKeys()), COL_KEY);

    drawText(baseX, baseY + 11,
             "物品: " + std::to_string(player.getInventory().size()) + " 技能:" +
             std::to_string(player.getSkills().size()), COL_DEFAULT);

    // 操作说明
    drawText(baseX, baseY + 13, "===== 操作 =====", COL_LOG);
    const char* help[] = {
        "WASD: 移动/近战",
        "F   : 远程武器瞄准",
        "T   : 技能面板",
        "I   : 物品栏",
        "M   : 小地图",
        "F5  : 存档",
        "F7  : 读档",
        "Q   : 退出"
    };
    for (int i = 0; i < 8; ++i) {
        drawText(baseX, baseY + 14 + i, help[i], COL_HUD);
    }
}

void Game::renderLog() const {
    int baseY = MAP_HEIGHT + 2;
    drawText(0, baseY, "===== 战斗日志 =====", COL_LOG);
    for (int i = 0; i < 5; ++i) {
        clearLine(0, baseY + 1 + i, 60);
        if ((size_t)i < combatLog.size()) {
            if ((size_t)i == combatLog.size() - 1) drawText(0, baseY + 1 + i, combatLog[i], COL_GOOD);
            else drawText(0, baseY + 1 + i, combatLog[i], COL_LOG);
        }
    }
    setColor(COL_DEFAULT);
}

void Game::renderMiniMap() const {
    int offsetX = 10;
    int offsetY = 3;
    // 边框
    for (int x = 0; x < MAP_WIDTH + 2; ++x) {
        drawChar(offsetX + x - 1, offsetY - 1, '-', COL_WARN);
        drawChar(offsetX + x - 1, offsetY + MAP_HEIGHT, '-', COL_WARN);
    }
    for (int y = 0; y < MAP_HEIGHT + 2; ++y) {
        drawChar(offsetX - 1, offsetY + y - 1, '|', COL_WARN);
        drawChar(offsetX + MAP_WIDTH, offsetY + y - 1, '|', COL_WARN);
    }
    drawChar(offsetX - 1, offsetY - 1, '+', COL_WARN);
    drawChar(offsetX + MAP_WIDTH, offsetY - 1, '+', COL_WARN);
    drawChar(offsetX - 1, offsetY + MAP_HEIGHT, '+', COL_WARN);
    drawChar(offsetX + MAP_WIDTH, offsetY + MAP_HEIGHT, '+', COL_WARN);

    drawText(offsetX + MAP_WIDTH / 2 - 3, offsetY - 2, "小地图 (M关闭)", COL_HUD);

    for (int y = 0; y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < MAP_WIDTH; ++x) {
            char c = map.at(x, y);
            Color col = COL_WALL;
            if (c == '.') col = COL_FLOOR;
            else if (c == '>') { c = '>'; col = COL_STAIRS; }
            if (player.getX() == x && player.getY() == y) { c = '@'; col = COL_PLAYER; }
            else if (monsterAt(x, y)) { c = 'm'; col = COL_GOBLIN; }
            drawChar(offsetX + x, offsetY + y, c, col);
        }
    }
    setColor(COL_DEFAULT);
}

void Game::renderInventory() const {
    int offsetX = 10;
    int offsetY = 3;
    int boxW = 50;
    int boxH = 16;
    // 边框
    for (int x = 0; x < boxW; ++x) {
        drawChar(offsetX + x, offsetY - 1, '-', COL_HUD);
        drawChar(offsetX + x, offsetY + boxH, '-', COL_HUD);
    }
    for (int y = 0; y < boxH + 1; ++y) {
        drawChar(offsetX - 1, offsetY + y - 1, '|', COL_HUD);
        drawChar(offsetX + boxW, offsetY + y - 1, '|', COL_HUD);
    }
    drawChar(offsetX - 1, offsetY - 1, '+', COL_HUD);
    drawChar(offsetX + boxW, offsetY - 1, '+', COL_HUD);
    drawChar(offsetX - 1, offsetY + boxH, '+', COL_HUD);
    drawChar(offsetX + boxW, offsetY + boxH, '+', COL_HUD);

    drawText(offsetX + boxW / 2 - 6, offsetY - 2, "物品栏 (I关闭)", COL_HUD);

    const auto& inv = player.getInventory();
    if (inv.empty()) {
        drawText(offsetX + 2, offsetY + 2, "物品栏为空", COL_LOG);
    } else {
        for (size_t i = 0; i < inv.size() && i < 13; ++i) {
            std::string line = "[" + std::to_string(i + 1) + "] " + inv[i].name;
            if (inv[i].count > 1) line += " x" + std::to_string(inv[i].count);
            // 武器显示类型
            if (inv[i].type == ItemType::Weapon) {
                const char* wkStr = "";
                switch (inv[i].wKind) {
                    case WeaponKind::Melee: wkStr = "(近战)"; break;
                    case WeaponKind::Spear: wkStr = "(范围)"; break;
                    case WeaponKind::Bow:   wkStr = "(远程)"; break;
                    case WeaponKind::Staff: wkStr = "(法杖)"; break;
                }
                line += wkStr;
            }
            drawText(offsetX + 2, offsetY + (int)i, line, COL_DEFAULT);
        }
    }
    drawText(offsetX + 2, offsetY + boxH - 2, "按1-9使用物品，I关闭", COL_GOOD);
    setColor(COL_DEFAULT);
}

void Game::renderSkillPanel() const {
    int offsetX = 8;
    int offsetY = 3;
    int boxW = 54;
    int boxH = 18;
    // 边框
    for (int x = 0; x < boxW; ++x) {
        drawChar(offsetX + x, offsetY - 1, '-', COL_HUD);
        drawChar(offsetX + x, offsetY + boxH, '-', COL_HUD);
    }
    for (int y = 0; y < boxH + 1; ++y) {
        drawChar(offsetX - 1, offsetY + y - 1, '|', COL_HUD);
        drawChar(offsetX + boxW, offsetY + y - 1, '|', COL_HUD);
    }
    drawChar(offsetX - 1, offsetY - 1, '+', COL_HUD);
    drawChar(offsetX + boxW, offsetY - 1, '+', COL_HUD);
    drawChar(offsetX - 1, offsetY + boxH, '+', COL_HUD);
    drawChar(offsetX + boxW, offsetY + boxH, '+', COL_HUD);

    drawText(offsetX + boxW / 2 - 6, offsetY - 2, "技能面板 (T关闭)", COL_HUD);

    const auto& skills = player.getSkills();
    if (skills.empty()) {
        drawText(offsetX + 2, offsetY + 2, "尚未学会任何技能", COL_LOG);
    } else {
        for (size_t i = 0; i < skills.size() && i < 13; ++i) {
            const Skill& s = skills[i];
            std::string line = "[" + std::to_string(i + 1) + "] " + s.name;
            line += " MP" + std::to_string(s.mpCost);
            line += " CD" + std::to_string(s.cooldown);
            if (s.cooldownLeft > 0) line += "(剩" + std::to_string(s.cooldownLeft) + ")";
            line += " Lv" + std::to_string(s.learnLevel);
            Color col = (s.cooldownLeft > 0) ? COL_LOG : COL_GOOD;
            drawText(offsetX + 2, offsetY + (int)i, line, col);
            drawText(offsetX + 2, offsetY + (int)i + 1, "    " + s.desc, COL_LOG);
        }
    }
    drawText(offsetX + 2, offsetY + boxH - 2, "按1-9施放技能，T关闭", COL_GOOD);
    setColor(COL_DEFAULT);
}

void Game::renderAimMode() const {
    // 在地图顶部显示瞄准提示
    std::string hint = "[瞄准模式] 按 WASD 选择方向，ESC取消";
    if (aimSource == AimSource::Skill) {
        const auto& skills = player.getSkills();
        if (aimSkillIndex >= 0 && aimSkillIndex < (int)skills.size()) {
            hint = "[施放" + skills[aimSkillIndex].name + "] WASD选方向，ESC取消";
        }
    } else {
        hint = "[远程攻击] WASD选方向，ESC取消";
    }
    drawText(0, 0, hint, COL_WARN);
    // 在日志区显示当前武器射程信息
    if (aimSource == AimSource::Weapon) {
        const char* wkStr = "近战";
        int range = 1;
        switch (player.getEquippedWeaponKind()) {
            case WeaponKind::Bow:   wkStr = "弓(直线6格)"; range = 6; break;
            case WeaponKind::Staff: wkStr = "法杖(直线6格)"; range = 6; break;
            case WeaponKind::Spear: wkStr = "长矛(相邻3格)"; range = 1; break;
            case WeaponKind::Melee: wkStr = "近战(用WASD移动攻击)"; break;
        }
        drawText(0, MAP_HEIGHT + 1, std::string("武器: ") + wkStr, COL_WEAPON);
    }
    setColor(COL_DEFAULT);
}

void Game::renderMessage() const {
    if (message.empty()) return;
    drawText(0, MAP_HEIGHT + 1, "提示: " + message, COL_WARN);
    setColor(COL_DEFAULT);
}

void Game::renderLevelEnterPrompt() const {
    if (!levelEnterPrompt) return;
    std::string text;
    Color col = COL_HUD;
    switch (currentLevelType) {
        case LevelType::Normal: return;  // 普通关不提示
        case LevelType::Bonus:
            text = ">>> 奖励关！无怪物，尽情拾取宝箱，HP/MP已回满 <<<";
            col = COL_GOOD;
            break;
        case LevelType::Boss:
            text = ">>> Boss副本！击败巨龙才能下楼，谨慎应战 <<<";
            col = COL_WARN;
            break;
    }
    drawText(0, MAP_HEIGHT, text, col);
    setColor(COL_DEFAULT);
}

void Game::renderLevelUpChoices(const std::vector<Skill>& choices) const {
    int offsetX = 8;
    int offsetY = 4;
    int boxW = 56;
    int boxH = 14;
    for (int x = 0; x < boxW; ++x) {
        drawChar(offsetX + x, offsetY - 1, '=', COL_GOOD);
        drawChar(offsetX + x, offsetY + boxH, '=', COL_GOOD);
    }
    drawText(offsetX + boxW / 2 - 8, offsetY - 2, "=== 升级！选择一个技能 ===", COL_GOOD);
    for (size_t i = 0; i < choices.size(); ++i) {
        const Skill& s = choices[i];
        int py = offsetY + 1 + (int)i * 4;
        std::string header = "[" + std::to_string(i + 1) + "] " + s.name;
        // 判断是新学还是升级
        bool isUpgrade = false;
        for (const auto& ps : player.getSkills()) {
            if (ps.id == s.id) { isUpgrade = true; break; }
        }
        header += isUpgrade ? " (升级)" : " (新学)";
        drawText(offsetX + 2, py, header, COL_WARN);
        drawText(offsetX + 4, py + 1, s.desc, COL_DEFAULT);
        std::string stats = "MP" + std::to_string(s.mpCost) + " CD" + std::to_string(s.cooldown);
        drawText(offsetX + 4, py + 2, stats, COL_LOG);
    }
    drawText(offsetX + 2, offsetY + boxH - 1, "按 1/2/3 选择", COL_GOOD);
    setColor(COL_DEFAULT);
}

// -------------------- 主循环 --------------------
void Game::run() {
    addLog("欢迎来到地牢！WASD移动，F远程，T技能，I物品。");

    while (!gameOver) {
        clearScreen();
        renderMap();
        renderHUD();
        renderLog();
        renderLevelEnterPrompt();
        renderMessage();

        if (showMiniMap) renderMiniMap();
        if (showInventory) renderInventory();
        if (showSkillPanel) renderSkillPanel();
        if (aimingMode) renderAimMode();

        // 关卡提示只显示一次
        levelEnterPrompt = false;

        // 清空一次性提示
        message.clear();

        // 死亡检查
        if (!player.isAlive()) {
            drawText(10, 10, "===== 你死了！游戏结束 =====", COL_WARN);
            drawText(10, 12, "按任意键退出...", COL_DEFAULT);
            _getch();
            break;
        }

        int ch = _getch();

        // 瞄准模式：只处理方向键和ESC
        if (aimingMode) {
            int norm = normalizeArrowKey(ch);
            if (ch == 27) {  // ESC
                aimingMode = false;
                // 退还MP（技能瞄准取消）
                if (aimSource == AimSource::Skill && aimSkillIndex >= 0) {
                    auto& skills = player.getSkillsMut();
                    if (aimSkillIndex < (int)skills.size()) {
                        player.restoreMP(skills[aimSkillIndex].mpCost);
                    }
                }
                message = "已取消瞄准";
                continue;
            }
            int dx = 0, dy = 0;
            if (norm == 'w') dy = -1;
            else if (norm == 's') dy = 1;
            else if (norm == 'a') dx = -1;
            else if (norm == 'd') dx = 1;
            else continue;

            if (aimSource == AimSource::Skill) {
                // 执行技能
                auto& skills = player.getSkillsMut();
                if (aimSkillIndex >= 0 && aimSkillIndex < (int)skills.size()) {
                    Skill& s = skills[aimSkillIndex];
                    float mult = s.power / 10.0f;
                    rangedAttack(dx, dy, s.range, mult, s.name);
                    s.cooldownLeft = s.cooldown;
                }
            } else {
                // 远程武器攻击
                WeaponKind wk = player.getEquippedWeaponKind();
                if (wk == WeaponKind::Bow) {
                    rangedAttack(dx, dy, 6, 1.0f, "弓箭");
                } else if (wk == WeaponKind::Staff) {
                    rangedAttack(dx, dy, 6, 0.8f, "法杖");
                } else if (wk == WeaponKind::Spear) {
                    // 长矛：攻击该方向及左右3格
                    // 简化为攻击该方向相邻格 + 两侧相邻
                    // 这里按直线1格处理（与近战类似但可指定方向）
                    int tx = player.getX() + dx;
                    int ty = player.getY() + dy;
                    Monster* m = monsterAt(tx, ty);
                    if (m) {
                        int dmg = m->takeDamage(player.getEffectiveAttack());
                        addLog("长矛刺中" + m->getName() + "，造成 " + std::to_string(dmg) + " 伤害");
                        if (!m->isAlive()) {
                            addLog(m->getName() + "被击败！获得 " + std::to_string(m->getExpReward()) + " 经验");
                            grantExp(m->getExpReward());
                        }
                    } else {
                        addLog("长矛未命中");
                    }
                }
            }
            aimingMode = false;
            player.tickBuff();
            player.tickSkillCooldowns();
            monsterTurn();
            continue;
        }

        // 标准化方向键
        ch = normalizeArrowKey(ch);

        // 物品栏模式
        if (showInventory) {
            if (ch == 'i' || ch == 'I') { showInventory = false; continue; }
            if (ch >= '1' && ch <= '9') {
                size_t idx = (size_t)(ch - '1');
                if (idx < player.getInventory().size()) {
                    if (player.useItem(idx)) {
                        addLog("使用了一件物品");
                    } else {
                        message = "该物品无法直接使用";
                    }
                }
                continue;
            }
            continue;
        }

        // 技能面板模式
        if (showSkillPanel) {
            if (ch == 't' || ch == 'T') { showSkillPanel = false; continue; }
            if (ch >= '1' && ch <= '9') {
                size_t idx = (size_t)(ch - '1');
                if (idx < player.getSkills().size()) {
                    showSkillPanel = false;
                    useSkill((int)idx);
                }
                continue;
            }
            continue;
        }

        // 小地图模式
        if (showMiniMap) {
            if (ch == 'm' || ch == 'M') showMiniMap = false;
            continue;
        }

        // 主命令
        switch (ch) {
            case 'w': case 'W': handlePlayerMove(0, -1); player.tickBuff(); player.tickSkillCooldowns(); monsterTurn(); break;
            case 's': case 'S': handlePlayerMove(0, 1);  player.tickBuff(); player.tickSkillCooldowns(); monsterTurn(); break;
            case 'a': case 'A': handlePlayerMove(-1, 0); player.tickBuff(); player.tickSkillCooldowns(); monsterTurn(); break;
            case 'd': case 'D': handlePlayerMove(1, 0);  player.tickBuff(); player.tickSkillCooldowns(); monsterTurn(); break;
            case 'f': case 'F':
                // 远程武器瞄准（仅弓/法杖/长矛可远程）
                if (player.getEquippedWeaponKind() == WeaponKind::Melee) {
                    message = "当前为近战武器，移动撞向怪物即可攻击";
                } else {
                    aimingMode = true;
                    aimSource = AimSource::Weapon;
                    message = "按WASD选择攻击方向，ESC取消";
                }
                break;
            case 't': case 'T': showSkillPanel = true; break;
            case 'i': case 'I': showInventory = true; break;
            case 'm': case 'M': showMiniMap = true; break;
            case 'q': case 'Q': gameOver = true; break;
            case KEY_F5:  // F5 存档
                if (saveGame()) { message = "已存档到 save.dat"; addLog("游戏已存档"); }
                else { message = "存档失败！"; }
                break;
            case KEY_F7:  // F7 读档
                if (loadGame()) { message = "读档成功！"; addLog("读取存档成功"); }
                else { message = "读档失败或无存档！"; }
                break;
        }
    }
}

// -------------------- 存档/读档 --------------------
bool Game::saveGame(const std::string& filename) const {
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs) return false;

    ofs.write(SAVE_MAGIC, 4);
    int v = SAVE_VERSION;
    ofs.write(reinterpret_cast<const char*>(&v), sizeof(int));

    // 玩家基础数据
    int px = player.getX(), py = player.getY();
    int php = player.getHP(), pmax = player.getMaxHP();
    int patk = player.getAttack(), pdef = player.getDefense();
    int plv = player.getLevel(), pkeys = player.getKeys();
    int pwbonus = player.getWeaponBonus();
    ofs.write(reinterpret_cast<const char*>(&px), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&py), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&php), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&pmax), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&patk), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&pdef), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&plv), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&pkeys), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&pwbonus), sizeof(int));

    // v2新增：职业/MP/护盾/经验/玩家等级/武器/增益
    int career = (int)player.getCareer();
    int pmp = player.getMP(), pmaxmp = player.getMaxMP();
    int pshield = player.getShield();
    int pexp = player.getExp(), pexpNext = player.getExpToNext();
    int pplayerLv = player.getPlayerLevel();
    int ewk = (int)player.getEquippedWeaponKind();
    int ewnLen = (int)player.getEquippedWeaponName().size();
    int buffAtk = player.getBuffAtk(), buffTurns = player.getBuffAtkTurns();
    ofs.write(reinterpret_cast<const char*>(&career), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&pmp), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&pmaxmp), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&pshield), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&pexp), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&pexpNext), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&pplayerLv), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&ewk), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&ewnLen), sizeof(int));
    ofs.write(player.getEquippedWeaponName().c_str(), ewnLen);
    ofs.write(reinterpret_cast<const char*>(&buffAtk), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&buffTurns), sizeof(int));

    // 物品栏
    const auto& inv = player.getInventory();
    int invCount = (int)inv.size();
    ofs.write(reinterpret_cast<const char*>(&invCount), sizeof(int));
    for (const auto& it : inv) {
        int t = (int)it.type;
        int nameLen = (int)it.name.size();
        int wk = (int)it.wKind;
        ofs.write(reinterpret_cast<const char*>(&t), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&nameLen), sizeof(int));
        ofs.write(it.name.c_str(), nameLen);
        ofs.write(reinterpret_cast<const char*>(&it.value), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&it.count), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&wk), sizeof(int));
    }

    // 技能列表
    const auto& skills = player.getSkills();
    int skillCount = (int)skills.size();
    ofs.write(reinterpret_cast<const char*>(&skillCount), sizeof(int));
    for (const auto& s : skills) {
        int sid = s.id, smp = s.mpCost, scd = s.cooldown, scdl = s.cooldownLeft;
        int srng = s.range, spwr = s.power, seff = (int)s.effect, ssch = (int)s.school;
        int slv = s.learnLevel;
        int snameLen = (int)s.name.size(), sdescLen = (int)s.desc.size();
        ofs.write(reinterpret_cast<const char*>(&sid), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&snameLen), sizeof(int));
        ofs.write(s.name.c_str(), snameLen);
        ofs.write(reinterpret_cast<const char*>(&sdescLen), sizeof(int));
        ofs.write(s.desc.c_str(), sdescLen);
        ofs.write(reinterpret_cast<const char*>(&smp), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&scd), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&scdl), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&srng), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&spwr), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&seff), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&ssch), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&slv), sizeof(int));
    }

    // 地图
    int mw = map.getWidth(), mh = map.getHeight();
    ofs.write(reinterpret_cast<const char*>(&mw), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&mh), sizeof(int));
    for (int y = 0; y < mh; ++y) {
        for (int x = 0; x < mw; ++x) {
            char c = map.at(x, y);
            ofs.write(&c, 1);
        }
    }
    int roomCount = (int)map.roomCount();
    ofs.write(reinterpret_cast<const char*>(&roomCount), sizeof(int));
    for (size_t i = 0; i < map.roomCount(); ++i) {
        const Room& r = map.getRoom(i);
        ofs.write(reinterpret_cast<const char*>(&r.x), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&r.y), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&r.w), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&r.h), sizeof(int));
    }
    int sx = map.getStairsX(), sy = map.getStairsY();
    ofs.write(reinterpret_cast<const char*>(&sx), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&sy), sizeof(int));

    // 怪物
    int monCount = (int)monsters.size();
    ofs.write(reinterpret_cast<const char*>(&monCount), sizeof(int));
    for (const auto& m : monsters) {
        int mt = (int)m->getType();
        int mx = m->getX(), my = m->getY();
        int mhp = m->getHP(), mmax = m->getMaxHP();
        int matk = m->getAttack(), mdef = m->getDefense();
        int mcharging = m->isCharging() ? 1 : 0;
        ofs.write(reinterpret_cast<const char*>(&mt), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&mx), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&my), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&mhp), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&mmax), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&matk), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&mdef), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&mcharging), sizeof(int));
    }

    // 道具
    int itemCount = (int)items.size();
    ofs.write(reinterpret_cast<const char*>(&itemCount), sizeof(int));
    for (const auto& it : items) {
        int itType = (int)it->getType();
        int ix = it->getX(), iy = it->getY();
        int iv = it->getValue();
        int iwk = (int)it->getWeaponKind();
        int inameLen = (int)it->getItemName().size();
        ofs.write(reinterpret_cast<const char*>(&itType), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&ix), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&iy), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&iv), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&iwk), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&inameLen), sizeof(int));
        ofs.write(it->getItemName().c_str(), inameLen);
    }

    // 关卡类型
    int clt = (int)currentLevelType;
    ofs.write(reinterpret_cast<const char*>(&clt), sizeof(int));

    // 日志
    int logCount = (int)combatLog.size();
    ofs.write(reinterpret_cast<const char*>(&logCount), sizeof(int));
    for (const auto& s : combatLog) {
        int len = (int)s.size();
        ofs.write(reinterpret_cast<const char*>(&len), sizeof(int));
        ofs.write(s.c_str(), len);
    }

    return ofs.good();
}

bool Game::loadGame(const std::string& filename) {
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) return false;

    char magic[4];
    ifs.read(magic, 4);
    for (int i = 0; i < 4; ++i) {
        if (magic[i] != SAVE_MAGIC[i]) return false;
    }
    int version;
    ifs.read(reinterpret_cast<char*>(&version), sizeof(int));
    if (version != SAVE_VERSION) {
        addLog("存档版本不兼容");
        return false;
    }

    // 玩家基础
    int px, py, php, pmax, patk, pdef, plv, pkeys, pwbonus;
    ifs.read(reinterpret_cast<char*>(&px), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&py), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&php), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&pmax), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&patk), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&pdef), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&plv), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&pkeys), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&pwbonus), sizeof(int));

    // v2新增字段
    int career, pmp, pmaxmp, pshield, pexp, pexpNext, pplayerLv, ewk, ewnLen, buffAtk, buffTurns;
    ifs.read(reinterpret_cast<char*>(&career), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&pmp), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&pmaxmp), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&pshield), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&pexp), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&pexpNext), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&pplayerLv), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&ewk), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&ewnLen), sizeof(int));
    std::string ewn(ewnLen, '\0');
    ifs.read(&ewn[0], ewnLen);
    ifs.read(reinterpret_cast<char*>(&buffAtk), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&buffTurns), sizeof(int));

    // 直接重建玩家对象
    player = Player(px, py, (Career)career);
    player.setHP(php);
    player.setMaxHP(pmax);
    player.setAttack(patk);
    player.setDefense(pdef);
    player.setLevel(plv);
    player.setKeys(pkeys);
    player.setWeaponBonus(pwbonus);
    player.setMP(pmp);
    player.setMaxMP(pmaxmp);
    player.setShield(pshield);
    player.setExp(pexp);
    player.setExpToNext(pexpNext);
    player.setPlayerLevel(pplayerLv);
    player.setEquippedWeaponKind((WeaponKind)ewk);
    player.setEquippedWeaponName(ewn);
    player.applyBuffAtk(buffAtk, buffTurns);
    player.clearInventory();
    player.clearSkills();

    // 物品栏
    int invCount;
    ifs.read(reinterpret_cast<char*>(&invCount), sizeof(int));
    for (int i = 0; i < invCount; ++i) {
        int t, nameLen, val, cnt, wk;
        ifs.read(reinterpret_cast<char*>(&t), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&nameLen), sizeof(int));
        std::string nm(nameLen, '\0');
        ifs.read(&nm[0], nameLen);
        ifs.read(reinterpret_cast<char*>(&val), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&cnt), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&wk), sizeof(int));
        for (int c = 0; c < cnt; ++c) {
            player.addItem((ItemType)t, nm, val, (WeaponKind)wk);
        }
    }

    // 技能
    int skillCount;
    ifs.read(reinterpret_cast<char*>(&skillCount), sizeof(int));
    for (int i = 0; i < skillCount; ++i) {
        Skill s;
        int sid, snameLen, sdescLen, smp, scd, scdl, srng, spwr, seff, ssch, slv;
        ifs.read(reinterpret_cast<char*>(&sid), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&snameLen), sizeof(int));
        s.name.resize(snameLen);
        ifs.read(&s.name[0], snameLen);
        ifs.read(reinterpret_cast<char*>(&sdescLen), sizeof(int));
        s.desc.resize(sdescLen);
        ifs.read(&s.desc[0], sdescLen);
        ifs.read(reinterpret_cast<char*>(&smp), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&scd), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&scdl), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&srng), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&spwr), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&seff), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&ssch), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&slv), sizeof(int));
        s.id = sid; s.mpCost = smp; s.cooldown = scd; s.cooldownLeft = scdl;
        s.range = srng; s.power = spwr; s.effect = (SkillEffect)seff;
        s.school = (SkillSchool)ssch; s.learnLevel = slv;
        player.addSkill(s);
    }

    // 地图
    int mw, mh;
    ifs.read(reinterpret_cast<char*>(&mw), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&mh), sizeof(int));
    for (int y = 0; y < mh; ++y) {
        for (int x = 0; x < mw; ++x) {
            char c;
            ifs.read(&c, 1);
            map.set(x, y, c);
        }
    }
    int roomCount;
    ifs.read(reinterpret_cast<char*>(&roomCount), sizeof(int));
    for (int i = 0; i < roomCount; ++i) {
        int rx, ry, rw, rh;
        ifs.read(reinterpret_cast<char*>(&rx), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&ry), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&rw), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&rh), sizeof(int));
    }
    int sx, sy;
    ifs.read(reinterpret_cast<char*>(&sx), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&sy), sizeof(int));
    // 恢复楼梯坐标（修复原bug：读档后stairsX/Y丢失）
    map.setStairs(sx, sy);
    map.set(sx, sy, '>');  // 确保'>'字符正确

    // 怪物
    int monCount;
    ifs.read(reinterpret_cast<char*>(&monCount), sizeof(int));
    monsters.clear();
    for (int i = 0; i < monCount; ++i) {
        int mt, mx, my, mhp, mmax, matk, mdef, mcharging;
        ifs.read(reinterpret_cast<char*>(&mt), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&mx), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&my), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&mhp), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&mmax), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&matk), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&mdef), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&mcharging), sizeof(int));
        auto m = std::make_unique<Monster>(mx, my, (MonsterType)mt, player.getLevel());
        m->setHP(mhp);
        m->setMaxHP(mmax);  // 修复：恢复maxHp
        m->setAttack(matk);
        m->setDefense(mdef);
        m->setCharging(mcharging != 0);
        monsters.push_back(std::move(m));
    }

    // 道具
    int itemCount;
    ifs.read(reinterpret_cast<char*>(&itemCount), sizeof(int));
    items.clear();
    for (int i = 0; i < itemCount; ++i) {
        int itType, ix, iy, iv, iwk, inameLen;
        ifs.read(reinterpret_cast<char*>(&itType), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&ix), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&iy), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&iv), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&iwk), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&inameLen), sizeof(int));
        std::string iname(inameLen, '\0');
        ifs.read(&iname[0], inameLen);
        items.push_back(std::make_unique<Item>(ix, iy, (ItemType)itType, iv,
                                               (WeaponKind)iwk, iname));
    }

    // 关卡类型
    int clt;
    ifs.read(reinterpret_cast<char*>(&clt), sizeof(int));
    currentLevelType = (LevelType)clt;

    // 日志
    int logCount;
    ifs.read(reinterpret_cast<char*>(&logCount), sizeof(int));
    combatLog.clear();
    for (int i = 0; i < logCount; ++i) {
        int len;
        ifs.read(reinterpret_cast<char*>(&len), sizeof(int));
        std::string s(len, '\0');
        ifs.read(&s[0], len);
        combatLog.push_back(s);
    }

    return ifs.good();
}
