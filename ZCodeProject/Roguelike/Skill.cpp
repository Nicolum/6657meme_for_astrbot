#include "Skill.h"
#include <cstdlib>   // rand()
#include <algorithm> // shuffle
#include <random>

// ============================================================================
// Skill.cpp —— 技能库与升级抽选实现
// ============================================================================

// 全局技能库（静态构造，仅初始化一次）
const std::vector<Skill>& getSkillLibrary() {
    static const std::vector<Skill> library = []() {
        std::vector<Skill> v;
        // ---------- 战士系技能 ----------
        // id=1 重击：近战2倍伤害
        {
            Skill s;
            s.id = 1; s.name = "重击"; s.desc = "近战攻击，造成2倍伤害";
            s.mpCost = 4; s.cooldown = 2; s.range = 1; s.power = 20; // power=20表示2.0倍
            s.effect = SkillEffect::DamageMelee; s.school = SkillSchool::Warrior;
            v.push_back(s);
        }
        // id=2 横扫：自身周围8格AOE
        {
            Skill s;
            s.id = 2; s.name = "横扫"; s.desc = "攻击周围8格所有敌人，造成1.2倍伤害";
            s.mpCost = 8; s.cooldown = 4; s.range = 0; s.power = 12;
            s.effect = SkillEffect::AOE; s.school = SkillSchool::Warrior;
            v.push_back(s);
        }
        // id=3 钢铁意志：护盾+12
        {
            Skill s;
            s.id = 3; s.name = "钢铁意志"; s.desc = "获得12点护盾";
            s.mpCost = 6; s.cooldown = 5; s.range = 0; s.power = 12;
            s.effect = SkillEffect::Shield; s.school = SkillSchool::Warrior;
            v.push_back(s);
        }
        // id=4 战吼：攻击+4持续6回合
        {
            Skill s;
            s.id = 4; s.name = "战吼"; s.desc = "攻击力+4，持续6回合";
            s.mpCost = 5; s.cooldown = 6; s.range = 0; s.power = 4;
            s.effect = SkillEffect::BuffAtk; s.school = SkillSchool::Warrior;
            v.push_back(s);
        }

        // ---------- 法师系技能 ----------
        // id=5 火球术：远程直线伤害
        {
            Skill s;
            s.id = 5; s.name = "火球术"; s.desc = "发射火球，直线上首个敌人受1.8倍伤害";
            s.mpCost = 6; s.cooldown = 1; s.range = 6; s.power = 18;
            s.effect = SkillEffect::DamageLine; s.school = SkillSchool::Mage;
            v.push_back(s);
        }
        // id=6 寒冰箭：远程直线，威力较低
        {
            Skill s;
            s.id = 6; s.name = "寒冰箭"; s.desc = "冰锥攻击直线首个敌人，1.3倍伤害";
            s.mpCost = 4; s.cooldown = 1; s.range = 5; s.power = 13;
            s.effect = SkillEffect::DamageLine; s.school = SkillSchool::Mage;
            v.push_back(s);
        }
        // id=7 奥爆：周围AOE
        {
            Skill s;
            s.id = 7; s.name = "奥爆"; s.desc = "周围8格敌人受1.5倍伤害";
            s.mpCost = 10; s.cooldown = 4; s.range = 0; s.power = 15;
            s.effect = SkillEffect::AOE; s.school = SkillSchool::Mage;
            v.push_back(s);
        }
        // id=8 治疗术：恢复HP
        {
            Skill s;
            s.id = 8; s.name = "治疗术"; s.desc = "恢复15点HP";
            s.mpCost = 8; s.cooldown = 3; s.range = 0; s.power = 15;
            s.effect = SkillEffect::Heal; s.school = SkillSchool::Mage;
            v.push_back(s);
        }

        // ---------- 游侠系技能 ----------
        // id=9 精准射击：远程直线
        {
            Skill s;
            s.id = 9; s.name = "精准射击"; s.desc = "远程射击直线首个敌人，1.6倍伤害";
            s.mpCost = 5; s.cooldown = 1; s.range = 6; s.power = 16;
            s.effect = SkillEffect::DamageLine; s.school = SkillSchool::Ranger;
            v.push_back(s);
        }
        // id=10 多重射击：远程直线高伤
        {
            Skill s;
            s.id = 10; s.name = "多重射击"; s.desc = "强力射击直线首个敌人，2.2倍伤害";
            s.mpCost = 9; s.cooldown = 3; s.range = 6; s.power = 22;
            s.effect = SkillEffect::DamageLine; s.school = SkillSchool::Ranger;
            v.push_back(s);
        }
        // id=11 闪避本能：护盾+8（用护盾模拟闪避缓冲）
        {
            Skill s;
            s.id = 11; s.name = "闪避本能"; s.desc = "获得8点护盾";
            s.mpCost = 4; s.cooldown = 4; s.range = 0; s.power = 8;
            s.effect = SkillEffect::Shield; s.school = SkillSchool::Ranger;
            v.push_back(s);
        }

        // ---------- 通用技能 ----------
        // id=12 吸血打击：近战1.5倍并回血
        {
            Skill s;
            s.id = 12; s.name = "吸血打击"; s.desc = "近战1.5倍伤害，回复造成伤害的50%";
            s.mpCost = 6; s.cooldown = 3; s.range = 1; s.power = 15;
            s.effect = SkillEffect::DamageMelee; s.school = SkillSchool::Common;
            v.push_back(s);
        }
        // id=13 护盾充能：护盾+8
        {
            Skill s;
            s.id = 13; s.name = "护盾充能"; s.desc = "获得8点护盾";
            s.mpCost = 4; s.cooldown = 3; s.range = 0; s.power = 8;
            s.effect = SkillEffect::Shield; s.school = SkillSchool::Common;
            v.push_back(s);
        }
        // id=14 急救：恢复12HP
        {
            Skill s;
            s.id = 14; s.name = "急救"; s.desc = "恢复12点HP";
            s.mpCost = 5; s.cooldown = 3; s.range = 0; s.power = 12;
            s.effect = SkillEffect::Heal; s.school = SkillSchool::Common;
            v.push_back(s);
        }
        return v;
    }();
    return library;
}

// 根据ID查找技能模板
const Skill* getSkillTemplate(int id) {
    for (const auto& s : getSkillLibrary()) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

// 判断职业是否可学习该技能派系
static bool careerMatchesSchool(Career c, SkillSchool s) {
    if (s == SkillSchool::Common) return true;
    switch (c) {
        case Career::Warrior: return s == SkillSchool::Warrior;
        case Career::Mage:    return s == SkillSchool::Mage;
        case Career::Ranger:  return s == SkillSchool::Ranger;
    }
    return false;
}

// 升级抽选
std::vector<Skill> rollSkillChoices(const std::vector<Skill>& playerSkills,
                                    Career career) {
    // 收集所有可选技能：职业匹配且（未学 或 已学可继续升级，最高3级）
    std::vector<Skill> candidates;
    for (const auto& tmpl : getSkillLibrary()) {
        if (!careerMatchesSchool(career, tmpl.school)) continue;
        // 查找玩家是否已学
        int learnedLv = 0;
        for (const auto& ps : playerSkills) {
            if (ps.id == tmpl.id) { learnedLv = ps.learnLevel; break; }
        }
        if (learnedLv == 0) {
            // 未学，可作为新技能选项
            Skill s = tmpl;
            s.learnLevel = 0;
            candidates.push_back(s);
        } else if (learnedLv < 3) {
            // 已学，可升级（这里也加入候选，玩家选到则升级）
            Skill s = tmpl;
            s.learnLevel = learnedLv;  // 标记当前等级
            candidates.push_back(s);
        }
    }

    // 随机打乱并取前3个
    std::mt19937 rng((unsigned)std::random_device{}());
    std::shuffle(candidates.begin(), candidates.end(), rng);

    std::vector<Skill> result;
    for (size_t i = 0; i < candidates.size() && result.size() < 3; ++i) {
        result.push_back(candidates[i]);
    }
    return result;
}
