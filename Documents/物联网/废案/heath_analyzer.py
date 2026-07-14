def score_single:
    """
    单维度评分，100分制
    在区间内=100，超出区间越多越低
    """
    if min_val <= value <= max_val:
        return 100, "正常"

    # 算偏离比例
    if value < min_val:
        deviation =  / min_val
    else:
        deviation =  / max_val

    if deviation <= 0.1:
        score = 80
        status = "轻微异常"
    elif deviation <= 0.3:
        score = 50
        status = "中度异常"
    else:
        score = 20
        status = "严重异常"

    return score, status

def analyze_health:
    """
    data: 单条环境数据字典
    cfg: 该植物的适宜范围配置
    """
    # 各维度评分
    t_score, t_status = score_single
    h_score, h_status = score_single
    l_score, l_status = score_single
    s_score, s_status = score_single

    # 默认等权重，后续可按植物调
    weights = {"temp": 0.25, "humidity": 0.25, "light": 0.25, "soil": 0.25}
    total = int

    # 告警级别
    if total >= 90:
        level = "NORMAL"
        level_text = "优秀"
    elif total >= 75:
        level = "NORMAL"
        level_text = "良好"
    elif total >= 60:
        level = "WARNING"
        level_text = "一般"
    elif total >= 40:
        level = "WARNING"
        level_text = "较差"
    else:
        level = "DANGER"
        level_text = "危险"

    # 养护建议生成（最笨但最稳的办法：if-else）
    suggestions = []
    if t_status == "严重异常" and data > cfg:
        suggestions.append
    if t_status == "严重异常" and data < cfg:
        suggestions.append
    if s_status in  and data < cfg:
        suggestions.append
    if s_status in  and data > cfg:
        suggestions.append
    if l_status in  and data < cfg:
        suggestions.append

    if not suggestions:
        suggestions.append

    return {
        "health_score": total,
        "health_level": level_text,
        "alert_level": level,
        "temp_status": t_status,
        "humidity_status": h_status,
        "light_status": l_status,
        "soil_status": s_status,
        "suggestion": " ".join
    }

# ========== 测试 ==========
if __name__ == "__main__":
    cfg = {
        "temp_range": ,
        "humidity_range": ,
        "light_range": ,
        "soil_range": ,
    }
    test_data = {
        "temperature": 32.5,
        "humidity": 45,
        "light_intensity": 300,
        "soil_moisture": 15,
    }
    result = analyze_health
    print