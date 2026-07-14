#ifndef ENTITY_H
#define ENTITY_H

#include <string>
#include <vector>
#include "Career.h"

// ============================================================================
// Entity.h —— 实体类的声明
// 本文件定义了游戏中所有"实体"的基类 Entity，以及它的三个派生类：
//   Player  : 玩家角色，拥有职业、法力、护盾、经验、技能、武器
//   Monster : 怪物，分多种类型（哥布林/史莱姆/蝙蝠/骷髅/Boss龙）
//   Item    : 道具，分血瓶/蓝瓶/武器/钥匙/卷轴/护盾/宝箱
// 采用面向对象设计，公共属性（坐标、HP、攻防）放在基类中。
// ============================================================================

// ---------- 全局常量 ----------
constexpr int MAP_WIDTH = 40;   // 地图宽度（列）
constexpr int MAP_HEIGHT = 20;  // 地图高度（行）

// 怪物种类枚举
enum class MonsterType {
    Goblin,   // 哥布林 G：均衡型
    Slime,    // 史莱姆 S：高血量低攻击
    Bat,      // 蝙蝠 B：高速度随机移动
    Skeleton, // 骷髅 X：高攻击低血量（符号改为X避免与钥匙K冲突）
    Boss      // Boss龙 D：副本首领
};

// 道具种类枚举
enum class ItemType {
    Potion,      // 血瓶 H：恢复 HP
    ManaPotion,  // 蓝瓶 M：恢复 MP
    Weapon,      // 武器 W：提升攻击力，含武器类型
    Key,         // 钥匙 K：开启楼梯
    Scroll,      // 卷轴 T：永久提升最大HP
    Shield,      // 护盾 P：增加护盾值
    Chest        // 宝箱 C：奖励关专用，开启获得随机奖励
};

// 武器类型枚举
enum class WeaponKind {
    Melee,  // 近战（剑/斧）：移动撞向怪物即攻击
    Spear,  // 长矛：攻击相邻一圈3格
    Bow,    // 弓：远程直线
    Staff   // 法杖：远程直线，附带法术加成
};

// 关卡类型枚举
enum class LevelType {
    Normal,  // 普通关：怪物+道具+钥匙
    Bonus,   // 奖励关：无怪物，有宝箱，恢复满血
    Boss     // Boss副本：1只Boss，击败解锁楼梯
};

// 前向声明
struct Skill;

// 物品栏内的一格道具
struct InventoryItem {
    ItemType type;
    std::string name;   // 显示名
    int value;          // 数值含义随类型变化（血瓶=恢复量, 武器=攻击加成, 卷轴=maxHP提升量等）
    int count;          // 堆叠数量
    WeaponKind wKind;   // 仅Weapon类型有效，记录武器种类
};

// ============================================================================
// 基类：Entity
// 所有地图上能出现的"有生命或可交互对象"的公共抽象
// ============================================================================
class Entity {
protected:
    int x, y;          // 当前坐标（列, 行）
    int hp;            // 当前生命值
    int maxHp;         // 最大生命值
    int attack;        // 攻击力
    int defense;       // 防御力
    char symbol;       // 在地图上显示的字符
    std::string name;  // 名称（用于日志显示）

public:
    Entity(int x, int y, int hp, int atk, int def, char sym, const std::string& nm);
    virtual ~Entity() = default;

    // 基本属性访问
    int getX() const { return x; }
    int getY() const { return y; }
    int getHP() const { return hp; }
    int getMaxHP() const { return maxHp; }
    int getAttack() const { return attack; }
    int getDefense() const { return defense; }
    char getSymbol() const { return symbol; }
    const std::string& getName() const { return name; }

    void setPos(int nx, int ny) { x = nx; y = ny; }
    void setHP(int v) { hp = v; }
    void setAttack(int v) { attack = v; }
    void setDefense(int v) { defense = v; }
    void setMaxHP(int v) { maxHp = v; }  // 基类也提供，方便读档
    void setSymbol(char s) { symbol = s; }
    void setName(const std::string& n) { name = n; }

    // 受到伤害，返回实际扣血量（不会低于0）。
    // 基类实现：伤害=攻击力-防御力（最低1）。Player 重写以支持护盾。
    virtual int takeDamage(int dmg);

    // 是否存活
    bool isAlive() const { return hp > 0; }

    // 移动到目标位置（仅修改坐标，不做碰撞检测，碰撞由 Game 层处理）
    void move(int dx, int dy) { x += dx; y += dy; }
};

// ============================================================================
// 派生类：Player —— 玩家
// ============================================================================
class Player : public Entity {
private:
    Career career;          // 职业
    int level;              // 当前地牢层数
    int keys;               // 持有钥匙数
    int mp;                 // 当前法力值
    int maxMp;              // 最大法力值
    int shield;             // 护盾（额外血量，优先承受伤害）
    int exp;                // 当前经验值
    int expToNext;          // 升级所需经验
    int playerLevel;        // 玩家角色等级（独立于地牢层数，靠杀怪升级）
    int weaponBonus;        // 当前装备武器提供的攻击加成（用于显示）
    WeaponKind equippedWeaponKind;      // 装备武器类型
    std::string equippedWeaponName;     // 装备武器名
    int buffAtk;            // 临时攻击增益（来自战吼等技能）
    int buffAtkTurns;       // 增益剩余回合数
    std::vector<InventoryItem> inventory;  // 物品栏
    std::vector<Skill> skills;            // 已学技能

public:
    // 构造时指定职业，根据职业设置初始属性
    Player(int startX, int startY, Career c);

    // 职业
    Career getCareer() const { return career; }

    // 层数
    int getLevel() const { return level; }
    void nextLevel() { ++level; }
    void setLevel(int v) { level = v; }

    // 钥匙
    int getKeys() const { return keys; }
    void addKey() { ++keys; }
    void setKeys(int v) { keys = v; }
    bool useKey();

    // 法力值
    int getMP() const { return mp; }
    int getMaxMP() const { return maxMp; }
    void setMP(int v) { mp = v; }
    void setMaxMP(int v) { maxMp = v; }
    void restoreMP(int amount);   // 恢复MP，不超过上限
    bool spendMP(int amount);     // 消耗MP，不足返回false

    // 护盾
    int getShield() const { return shield; }
    void setShield(int v) { shield = v; }
    void addShield(int v) { shield += v; }

    // 经验与升级
    int getExp() const { return exp; }
    int getExpToNext() const { return expToNext; }
    int getPlayerLevel() const { return playerLevel; }
    void setExp(int v) { exp = v; }
    void setExpToNext(int v) { expToNext = v; }
    void setPlayerLevel(int v) { playerLevel = v; }
    // 给予经验，返回是否触发升级（可能多次升级，调用方循环处理）
    bool grantExp(int amount);
    // 执行一次升级（提升属性，不处理技能选择——技能选择由Game层负责）
    void levelUpStats();

    // 战斗：重写takeDamage以支持护盾优先吸收
    int takeDamage(int dmg) override;

    // 物品栏操作
    const std::vector<InventoryItem>& getInventory() const { return inventory; }
    void addItem(ItemType type, const std::string& name, int value,
                 WeaponKind wk = WeaponKind::Melee);
    bool useItem(size_t index);  // 使用物品栏第index个道具
    void clearInventory();

    // 恢复生命（血瓶用），不超过上限
    void heal(int amount);
    // 永久提升最大HP（卷轴用），并同步回满差额
    void increaseMaxHP(int amount);

    // 武器
    int getWeaponBonus() const { return weaponBonus; }
    void setWeaponBonus(int v) { weaponBonus = v; }
    WeaponKind getEquippedWeaponKind() const { return equippedWeaponKind; }
    void setEquippedWeaponKind(WeaponKind k) { equippedWeaponKind = k; }
    const std::string& getEquippedWeaponName() const { return equippedWeaponName; }
    void setEquippedWeaponName(const std::string& n) { equippedWeaponName = n; }
    // 装备武器：增加攻击、设置武器类型与名称
    void equipWeapon(const std::string& wname, WeaponKind wk, int atkBonus);

    // 临时增益
    int getBuffAtk() const { return buffAtk; }
    int getBuffAtkTurns() const { return buffAtkTurns; }
    void applyBuffAtk(int amount, int turns);
    void tickBuff();  // 每回合结束调用，递减增益回合
    // 实际攻击力（含武器加成 + 临时增益）
    int getEffectiveAttack() const { return attack + buffAtk; }

    // 技能
    const std::vector<Skill>& getSkills() const { return skills; }
    std::vector<Skill>& getSkillsMut() { return skills; }
    void addSkill(const Skill& s);
    void clearSkills();
    // 每回合结束递减所有技能冷却
    void tickSkillCooldowns();
};

// ============================================================================
// 派生类：Monster —— 怪物
// ============================================================================
class Monster : public Entity {
private:
    MonsterType type;
    int aggroRange;   // 追击感知范围（曼哈顿距离）
    int expReward;    // 击杀给予玩家的经验
    bool charging;    // Boss专用：是否正在蓄力（下回合重击）

public:
    Monster(int x, int y, MonsterType t, int dungeonLevel = 1);

    MonsterType getType() const { return type; }
    int getAggroRange() const { return aggroRange; }
    int getExpReward() const { return expReward; }
    bool isCharging() const { return charging; }
    void setCharging(bool c) { charging = c; }

    // 怪物 AI：决定下一步移动方向 (dx, dy)
    void decideMove(int playerX, int playerY, int& dx, int& dy) const;
};

// ============================================================================
// 派生类：Item —— 地图上的道具实体
// ============================================================================
class Item : public Entity {
private:
    ItemType type;
    int value;            // 血瓶=恢复量, 武器=攻击加成, 卷轴=maxHP提升量, 护盾=护盾值
    WeaponKind wKind;     // 仅Weapon类型有效
    std::string itemName; // 道具显示名（如"铁剑+2"）

public:
    Item(int x, int y, ItemType t, int val,
         WeaponKind wk = WeaponKind::Melee, const std::string& nm = "");

    ItemType getType() const { return type; }
    int getValue() const { return value; }
    WeaponKind getWeaponKind() const { return wKind; }
    const std::string& getItemName() const { return itemName; }
};

#endif // ENTITY_H
