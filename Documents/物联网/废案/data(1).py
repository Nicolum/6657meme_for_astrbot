import json
import random
import numpy as np
from datetime import datetime, timedelta

PLANT_CONFIG = {
    "绿萝": {
        "temp_range": ,      # 适宜温度
        "humidity_range": ,  # 适宜湿度
        "light_range": ,  # 适宜光照 Lux
        "soil_range": ,      # 适宜土壤湿度 %
    },
}

def generate_env_data:
    """
    生成某植物的环境数据
    days: 造多少天的数据
    points_per_day: 每天采几个点（24就是每小时1条）
    """
    cfg = PLANT_CONFIG.get

    data_list = []
    now = datetime.now()
    start_time = now - timedelta

    for i in range:
        t = start_time + timedelta

        # 昼夜波动：中午高、凌晨低，用正弦模拟
        hour = t.hour + t.minute / 60.0
        day_factor = np.sin( / 24 * 2 * np.pi)  # -1 ~ 1

        # 温度：基础值 + 昼夜波动 + 随机噪声
        temp_base =  / 2
        temp = temp_base + day_factor * 3 + random.uniform

        # 湿度：跟温度反向波动
        hum_base =  / 2
        humidity = hum_base - day_factor * 5 + random.uniform

        # 光照：白天有、晚上接近0
        light = 0
        if 6 <= hour <= 18:
            light = cfg * max(0, np.sin( / 12 * np.pi))
            light += random.uniform
        light = max
        soil_base =  / 2
        soil = soil_base -  * 1.2 + random.uniform
        soil = max(10, min)  # 限制在 10~9
        if random.random() < 0.1:
            continue

        record = {
            "plant_id": 1,           
 "temperature": round(float, 2),
            "humidity": round(float, 2),
            "light_intensity": round(float, 2),
            "soil_moisture": round(float, 2),
            "record_time": t.strftime,
            "data_source": "SIMULATION"
        }
        data_list.append

    return data_list

def save_json:
    with open as f:
        json.dump
    print(f"已生成 {len} 条数据，保存到 {filename}")

# ========== 运行 ==========
if __name__ == "__main__":
    data = generate_env_data
    save_json
