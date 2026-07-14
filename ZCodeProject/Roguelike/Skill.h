#ifndef SKILL_H
#define SKILL_H

#include <string>
#include <vector>
#include "Career.h"

// ============================================================================
// Skill.h —— 技能系统声明
// 定义 Skill 结构体（单个技能的属性）以及技能效果类型枚举。
// 技能库（所有可用技能的模板）与升级抽选逻辑在 Skill.cpp 中实现。
// ============================================================================

// 技能效果类型
enum class SkillEffect {
    DamageLine,  // 远程直线伤害（找到直线上第一个敌人造成伤害）
    DamageMelee, // 近战伤害（对相邻敌人，威力倍率）
    AOE,         // 范围伤害（自身周围8格）
    Heal,        // 治疗（恢复自身HP）
    Shield,      // 护盾（增加自身护盾值）
    BuffAtk      // 增益（临时提升攻击力，持续若干回合）
};

// 技能所属派系（决定哪些职业可学习）
enum class SkillSchool {
    Warrior,  // 战士系
    Mage,     // 法师系
    Ranger,   // 游侠系
    Common    // 通用（所有职业可学）
};

// 单个技能的完整定义
struct Skill {
    int id;                  // 技能唯一ID
    std::string name;        // 技能名
    std::string desc;        // 描述
    int mpCost;              // 法力消耗
    int cooldown;            // 冷却回合数（满）
    int cooldownLeft;        // 当前剩余冷却
    int range;               // 射程：0=自身/相邻AOE, 1=近战, >1=远程直线
    int power;               // 威力：伤害技能=攻击力倍率*10(存整数), 治疗/护盾=固定值
    SkillEffect effect;      // 效果类型
    SkillSchool school;      // 派系
    int learnLevel;          // 已学等级（0=未学, 1+为已学等级，升级可提升）

    // 默认构造
    Skill()
        : id(0), name(""), desc(""), mpCost(0), cooldown(0), cooldownLeft(0),
          range(0), power(0), effect(SkillEffect::Heal),
          school(SkillSchool::Common), learnLevel(0) {}
};

// ---------- 全局技能库函数 ----------

// 获取所有技能模板（固定列表，不会变）
const std::vector<Skill>& getSkillLibrary();

// 根据ID获取技能模板（找不到返回nullptr）
const Skill* getSkillTemplate(int id);

// 升级抽选：从技能库中筛选出"未学习 或 已学可升级"且"职业匹配或通用"的技能，
// 随机抽取最多3个作为升级选项。
// 参数 playerSkills: 玩家当前已学技能列表（按id匹配）
// 参数 career: 玩家职业
std::vector<Skill> rollSkillChoices(const std::vector<Skill>& playerSkills,
                                    Career career);

#endif // SKILL_H
