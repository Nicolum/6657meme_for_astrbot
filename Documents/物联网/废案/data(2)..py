import json
import random
import numpy as np
from datetime import datetime, timedelta

# ========== 配置区（按你的植物改这里） ==========
PLANT_CONFIG = {
    "绿萝": {
        "temp_range": (18, 30),       # 适宜温度 ℃
        "humidity_range": (40, 80),   # 适宜湿度 %
        "light_range": (5000, 20000), # 适宜光照 Lux
        "soil_range": (40, 70),       # 适宜土壤湿度 %
    },
    "多肉": {
        "temp_range": (20, 35),
        "humidity_range": (30, 60),
        "light_range": (10000, 30000),
        "soil_range": (20, 50),
    },
}


def generate_env_data(plant_name="绿萝", days=7, points_per_day=24):
    """
    生成某植物的环境数据
    plant_name: 植物名称（对应 PLANT_CONFIG 里的 key）
    days: 造多少天的数据
    points_per_day: 每天采几个点（24就是每小时1条）
    """
    cfg = PLANT_CONFIG.get(plant_name)
    if not cfg:
        print(f"错误：找不到植物 '{plant_name}' 的配置")
        return []

    temp_min, temp_max = cfg["temp_range"]
    hum_min, hum_max = cfg["humidity_range"]
    light_min, light_max = cfg["light_range"]
    soil_min, soil_max = cfg["soil_range"]

    data_list = []
    now = datetime.now()
    start_time = now - timedelta(days=days)
    total_points = days * points_per_day

    for i in range(total_points):
        t = start_time + timedelta(minutes=i * (60 / points_per_day))

        # 昼夜波动：中午高、凌晨低，用正弦模拟
        hour = t.hour + t.minute / 60.0
        day_factor = np.sin((hour) / 24 * 2 * np.pi)  # -1 ~ 1

        # 温度：基础值 + 昼夜波动 + 随机噪声
        temp_base = (temp_min + temp_max) / 2
        temp = temp_base + day_factor * 3 + random.uniform(-1.5, 1.5)
        temp = max(temp_min - 5, min(temp_max + 5, temp))

        # 湿度：跟温度反向波动
        hum_base = (hum_min + hum_max) / 2
        humidity = hum_base - day_factor * 5 + random.uniform(-3, 3)
        humidity = max(hum_min - 10, min(hum_max + 10, humidity))

        # 光照：白天有、晚上接近0
        light = 0
        if 6 <= hour <= 18:
            light = light_min + (light_max - light_min) * max(0, np.sin((hour - 6) / 12 * np.pi))
            light += random.uniform(-1000, 1000)
        light = max(0, light)

        # 土壤湿度：缓慢下降趋势，模拟蒸发
        soil_base = (soil_min + soil_max) / 2
        soil = soil_base - (i / total_points) * 1.2 + random.uniform(-2, 2)
        soil = max(10, min(95, soil))

        # 模拟缺失值（10%概率丢一条）
        if random.random() < 0.1:
            continue

        record = {
            "plant_id": 1,
            "temperature": round(temp, 2),
            "humidity": round(humidity, 2),
            "light_intensity": round(light, 2),
            "soil_moisture": round(soil, 2),
            "record_time": t.strftime("%Y-%m-%d %H:%M:%S"),
            "data_source": "SIMULATION"
        }
        data_list.append(record)

    return data_list


def save_json(data, filename="env_data.json"):
    """保存数据到 JSON 文件"""
    with open(filename, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
    print(f"已生成 {len(data)} 条数据，保存到 {filename}")


# ========== 运行 ==========
if __name__ == "__main__":
    # 生成7天数据，每小时1条（共168条）
    data = generate_env_data(plant_name="绿萝", days=7, points_per_day=24)
    
    if data:
        # 打印前3条预览
        print("=== 数据预览（前3条）===")
        for record in data[:3]:
            print(json.dumps(record, ensure_ascii=False, indent=2))
            print("-" * 40)
        
        # 保存到文件
        save_json(data, "env_data.json")