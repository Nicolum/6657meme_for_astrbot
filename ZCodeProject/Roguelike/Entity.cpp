#include "Entity.h"
#include "Skill.h"
#include <cstdlib>   // rand()
#include <algorithm> // min/max

// ============================================================================
// Entity.cpp —— 实体类的实现
// ============================================================================

// -------------------- Entity 基类 --------------------
Entity::Entity(int x, int y, int hp, int atk, int def, char sym, const std::string& nm)
    : x(x), y(y), hp(hp), maxHp(hp), attack(atk), defense(def), symbol(sym), name(nm) {}

int Entity::takeDamage(int dmg) {
    // 伤害至少为 1（避免防御过高时无法造成伤害，游戏僵局）
    int actual = dmg - defense;
    if (actual < 1) actual = 1;
    hp -= actual;
    if (hp < 0) hp = 0;
    return actual;
}

// -------------------- Player --------------------
Player::Player(int startX, int startY, Career c)
    : Entity(startX, startY, 30, 5, 1, '@', "玩家"),
      career(c), level(1), keys(0), mp(10), maxMp(10),
      shield(0), exp(0), expToNext(30), playerLevel(1),
      weaponBonus(0), equippedWeaponKind(WeaponKind::Melee),
      equippedWeaponName("徒手"), buffAtk(0), buffAtkTurns(0) {
    // 根据职业设置初始属性与武器
    switch (c) {
        case Career::Warrior:
            hp = maxHp = 40;
            attack = 6;
            defense = 3;
            mp = maxMp = 10;
            equippedWeaponKind = WeaponKind::Melee;
            equippedWeaponName = "铁剑";
            weaponBonus = 0;  // 铁剑为基础武器，无额外加成
            break;
        case Career::Mage:
            hp = maxHp = 24;
            attack = 4;
            defense = 1;
            mp = maxMp = 30;
            equippedWeaponKind = WeaponKind::Staff;
            equippedWeaponName = "理塘顶针法杖";
            weaponBonus = 1;  // 法杖提供少量攻击加成
            attack += 1;
            break;
        case Career::Ranger:
            hp = maxHp = 30;
            attack = 5;
            defense = 2;
            mp = maxMp = 18;
            equippedWeaponKind = WeaponKind::Bow;
            equippedWeaponName = "短弓";
            weaponBonus = 1;
            attack += 1;
            break;
    }
}

void Player::restoreMP(int amount) {
    mp = std::min(mp + amount, maxMp);
}

bool Player::spendMP(int amount) {
    if (mp < amount) return false;
    mp -= amount;
    return true;
}

bool Player::grantExp(int amount) {
    exp += amount;
    bool leveled = false;
    while (exp >= expToNext) {
        exp -= expToNext;
        levelUpStats();
        leveled = true;
    }
    return leveled;
}

void Player::levelUpStats() {
    ++playerLevel;
    maxHp += 5;
    maxMp += 3;
    hp = maxHp;        // 升级回满
    mp = maxMp;
    shield = 0;        // 护盾清零
    expToNext = 20 + playerLevel * 10;
}

int Player::takeDamage(int dmg) {
    // 护盾优先吸收伤害
    int remaining = dmg;
    int absorbedByShield = 0;
    if (shield > 0) {
        absorbedByShield = std::min(shield, remaining);
        shield -= absorbedByShield;
        remaining -= absorbedByShield;
    }
    // 剩余伤害走HP，受防御力减免（最低1，仅当有剩余伤害时）
    int absorbedByHP = 0;
    if (remaining > 0) {
        absorbedByHP = remaining - defense;
        if (absorbedByHP < 1) absorbedByHP = 1;
        hp -= absorbedByHP;
        if (hp < 0) hp = 0;
    }
    return absorbedByShield + absorbedByHP;
}

void Player::addItem(ItemType type, const std::string& name, int value, WeaponKind wk) {
    // 钥匙可堆叠
    if (type == ItemType::Key) {
        for (auto& it : inventory) {
            if (it.type == ItemType::Key) {
                it.count += 1;
                return;
            }
        }
    }
    inventory.push_back({type, name, value, 1, wk});
}

bool Player::useItem(size_t index) {
    if (index >= inventory.size()) return false;
    InventoryItem& it = inventory[index];
    switch (it.type) {
        case ItemType::Potion:
            heal(it.value);
            it.count -= 1;
            if (it.count <= 0) inventory.erase(inventory.begin() + index);
            return true;
        case ItemType::ManaPotion:
            restoreMP(it.value);
            it.count -= 1;
            if (it.count <= 0) inventory.erase(inventory.begin() + index);
            return true;
        case ItemType::Weapon:
            // 装备武器：替换旧武器
            equipWeapon(it.name, it.wKind, it.value);
            inventory.erase(inventory.begin() + index);
            return true;
        case ItemType::Scroll:
            increaseMaxHP(it.value);
            inventory.erase(inventory.begin() + index);
            return true;
        case ItemType::Shield:
            addShield(it.value);
            inventory.erase(inventory.begin() + index);
            return true;
        case ItemType::Key:
            // 钥匙不通过物品栏使用，靠近楼梯时自动消耗
            return false;
        case ItemType::Chest:
            // 宝箱不放进物品栏使用，拾取时直接开启
            return false;
    }
    return false;
}

void Player::heal(int amount) {
    hp = std::min(hp + amount, maxHp);
}

void Player::increaseMaxHP(int amount) {
    maxHp += amount;
    hp += amount;  // 提升上限的同时回满差额
}

void Player::clearInventory() {
    inventory.clear();
}

bool Player::useKey() {
    if (keys > 0) {
        --keys;
        return true;
    }
    return false;
}

void Player::equipWeapon(const std::string& wname, WeaponKind wk, int atkBonus) {
    // 替换武器：先减去旧武器加成，再加新武器加成
    attack -= weaponBonus;
    weaponBonus = atkBonus;
    attack += atkBonus;
    equippedWeaponKind = wk;
    equippedWeaponName = wname;
}

void Player::applyBuffAtk(int amount, int turns) {
    buffAtk = amount;
    buffAtkTurns = turns;
}

void Player::tickBuff() {
    if (buffAtkTurns > 0) {
        --buffAtkTurns;
        if (buffAtkTurns == 0) buffAtk = 0;
    }
}

void Player::addSkill(const Skill& s) {
    skills.push_back(s);
}

void Player::clearSkills() {
    skills.clear();
}

void Player::tickSkillCooldowns() {
    for (auto& s : skills) {
        if (s.cooldownLeft > 0) --s.cooldownLeft;
    }
}

// -------------------- Monster --------------------
Monster::Monster(int x, int y, MonsterType t, int dungeonLevel)
    : Entity(x, y, 1, 1, 0, '?', "怪物"), type(t), aggroRange(5),
      expReward(5), charging(false) {
    // 根据怪物种类设置属性
    switch (t) {
        case MonsterType::Goblin:   // 哥布林：均衡
            hp = maxHp = 8;
            attack = 4;
            defense = 1;
            symbol = 'G';
            name = "哥布林";
            aggroRange = 6;
            expReward = 6;
            break;
        case MonsterType::Slime:    // 史莱姆：高血量低攻击
            hp = maxHp = 14;
            attack = 2;
            defense = 2;
            symbol = 'S';
            name = "史莱姆";
            aggroRange = 4;
            expReward = 9;
            break;
        case MonsterType::Bat:      // 蝙蝠：高速随机移动
            hp = maxHp = 5;
            attack = 3;
            defense = 0;
            symbol = 'B';
            name = "蝙蝠";
            aggroRange = 7;
            expReward = 4;
            break;
        case MonsterType::Skeleton: // 骷髅：高攻击低血量，符号改为X
            hp = maxHp = 6;
            attack = 7;
            defense = 1;
            symbol = 'X';
            name = "骷髅";
            aggroRange = 5;
            expReward = 5;
            break;
        case MonsterType::Boss:     // Boss龙：副本首领
            hp = maxHp = 50 + dungeonLevel * 10;
            attack = 8 + dungeonLevel * 2;
            defense = 3;
            symbol = 'D';
            name = "远古巨龙";
            aggroRange = 12;  // Boss感知范围大
            expReward = 40 + dungeonLevel * 10;
            break;
    }
}

void Monster::decideMove(int playerX, int playerY, int& dx, int& dy) const {
    dx = 0;
    dy = 0;
    int dist = std::abs(x - playerX) + std::abs(y - playerY);

    // 蝙蝠：50% 概率随机移动
    if (type == MonsterType::Bat && (rand() % 2 == 0)) {
        int dirs[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
        int* d = dirs[rand() % 4];
        dx = d[0]; dy = d[1];
        return;
    }

    // Boss：正在蓄力时不移动（下回合释放重击）
    if (type == MonsterType::Boss && charging) {
        return;  // dx=dy=0，原地蓄力
    }

    if (dist <= aggroRange) {
        // 追击模式
        int diffX = playerX - x;
        int diffY = playerY - y;
        if (std::abs(diffX) >= std::abs(diffY)) {
            dx = (diffX > 0) ? 1 : -1;
        } else {
            dy = (diffY > 0) ? 1 : -1;
        }
        // Boss有30%概率开始蓄力（仅当相邻且未蓄力时）
        // 注意：蓄力状态由Game层在monsterTurn中设置与判定，这里只负责移动
    } else {
        // 闲逛模式
        if (rand() % 100 < 30) {
            int dirs[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
            int* d = dirs[rand() % 4];
            dx = d[0]; dy = d[1];
        }
    }
}

// -------------------- Item --------------------
Item::Item(int x, int y, ItemType t, int val, WeaponKind wk, const std::string& nm)
    : Entity(x, y, 1, 0, 0, '?', "道具"), type(t), value(val), wKind(wk), itemName(nm) {
    switch (t) {
        case ItemType::Potion:
            symbol = 'H';
            name = "血瓶";
            itemName = "血瓶";
            break;
        case ItemType::ManaPotion:
            symbol = 'M';
            name = "蓝瓶";
            itemName = "蓝瓶";
            break;
        case ItemType::Weapon:
            symbol = 'W';
            name = nm.empty() ? "武器" : nm;
            itemName = nm.empty() ? "武器" : nm;
            break;
        case ItemType::Key:
            symbol = 'K';
            name = "钥匙";
            itemName = "钥匙";
            break;
        case ItemType::Scroll:
            symbol = 'T';
            name = "卷轴";
            itemName = "生命卷轴";
            break;
        case ItemType::Shield:
            symbol = 'P';
            name = "护盾";
            itemName = "护盾";
            break;
        case ItemType::Chest:
            symbol = 'C';
            name = "宝箱";
            itemName = "宝箱";
            break;
    }
}
