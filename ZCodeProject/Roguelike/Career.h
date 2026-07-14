#ifndef CAREER_H
#define CAREER_H

#include <string>

// ============================================================================
// Career.h —— 职业定义（仅头文件）
// 玩家在游戏开始时选择职业，不同职业拥有不同的初始属性、初始技能、
// 装备武器类型以及技能学习池。
// ============================================================================

// 职业枚举
enum class Career {
    Warrior,  // 战士：高血近战，擅长正面硬刚
    Mage,     // 法师：低血高蓝，擅长远程魔法
    Ranger    // 游侠：均衡，擅长远程弓箭与机动
};

// 职业中文名
inline const char* careerName(Career c) {
    switch (c) {
        case Career::Warrior: return "战士";
        case Career::Mage:    return "法师";
        case Career::Ranger:  return "游侠";
    }
    return "未知";
}

// 职业简介
inline const char* careerDesc(Career c) {
    switch (c) {
        case Career::Warrior: return "高血量近战，初始装备铁剑，技能偏重爆发与防御";
        case Career::Mage:    return "高法力远程，初始装备法杖，技能偏重元素与控场";
        case Career::Ranger:  return "均衡型远程，初始装备长弓，技能偏重精准与机动";
    }
    return "";
}

#endif // CAREER_H
