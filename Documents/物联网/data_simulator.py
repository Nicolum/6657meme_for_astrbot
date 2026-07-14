"""
绿智管家 - 环境数据模拟生成器 (data_simulator.py)

职责: 模拟室内绿植环境传感器数据(温度/湿度/光照/土壤湿度),
      按指定频率生成并 POST 提交到后端 /api/v1/env-data 接口。

数据字段严格对齐数据库 env_data 表 (见 plan_v2 §4.2.3):
  - plant_id        BIGINT       关联植物ID
  - temperature     DECIMAL(5,2) 温度(℃)
  - humidity        DECIMAL(5,2) 空气湿度(%)
  - light_intensity DECIMAL(8,2) 光照强度(Lux)
  - soil_moisture   DECIMAL(5,2) 土壤湿度(%)
  - record_time     DATETIME     数据记录时间
  - data_source     VARCHAR(20)  数据来源(默认 'SIMULATION')

提交格式对齐统一响应规范 (plan_v2 §5.1):
  POST /api/v1/env-data          单条上传
  POST /api/v1/env-data/batch    批量上传
  Header: Authorization: Bearer <token>, Content-Type: application/json

用法:
  # 依赖环境变量 (见 README.md "环境变量配置"):
  #   GW_API_BASE   后端地址,默认 http://localhost:8080
  #   GW_TOKEN      登录获取的 JWT (需登录后填入)
  #   GW_PLANT_ID   目标植物ID

  # 单次生成一条并打印 JSON (不提交,用于自检)
  python data_simulator.py --once

  # 单次生成并提交单条
  python data_simulator.py --once --submit

  # 持续模拟: 每 30 秒生成并提交一条
  python data_simulator.py --interval 30

  # 批量生成近 N 天历史数据并提交到 /batch 接口
  python data_simulator.py --history 7 --plant 1 --submit
"""

from __future__ import annotations

import argparse
import json
import os
import random
import sys
import time
from datetime import datetime, timedelta
from decimal import Decimal, ROUND_HALF_UP
from typing import Any

import requests


# =====================================================================
# 配置区 (优先读环境变量, 便于不同机器/成员统一)
# =====================================================================
API_BASE = os.getenv("GW_API_BASE", "http://localhost:8080").rstrip("/")
API_TOKEN = os.getenv("GW_TOKEN", "")
DEFAULT_PLANT_ID = int(os.getenv("GW_PLANT_ID", "1"))
# 单条/批量接口路径 (plan_v2 §5.2)
URL_SINGLE = f"{API_BASE}/api/v1/env-data"
URL_BATCH = f"{API_BASE}/api/v1/env-data/batch"

# 数据提交间隔默认值 (秒)
DEFAULT_INTERVAL = 30
# 历史数据每日采样点数 (默认每 30 分钟一条 -> 48 条/天)
SAMPLES_PER_DAY = 48


# =====================================================================
# 数值规整: 模拟值统一保留到数据库声明的精度
# =====================================================================
def _round(value: float, places: int = 2) -> float:
    """按指定小数位四舍五入, 对齐 DECIMAL(5,2)/(8,2) 精度。"""
    quant = Decimal("1e-{}".format(places))
    return float(Decimal(str(value)).quantize(quant, rounding=ROUND_HALF_UP))


# =====================================================================
# 4 维环境数据模拟
# ---------------------------------------------------------------------
# 模拟思路:
#   1. 以"小时"为基准生成昼夜曲线 (温度/光照随白天升高, 夜晚降低)
#   2. 在基准值上叠加随机扰动, 模拟真实传感器抖动
#   3. 土壤湿度做"缓慢下降"趋势, 模拟蒸发 (后续可扩展浇水事件)
# 注: 适宜范围取自 plant_info 表, 这里用常见室内绿植的通用范围做默认值。
#     实际使用时应按植物品种读取 temp_min/temp_max 等字段 (第二天联调)。
# =====================================================================

# 通用室内绿植适宜范围 (作为模拟基准, 非评分阈值)
DEFAULT_RANGES = {
    "temperature": (18.0, 28.0),       # ℃
    "humidity": (40.0, 70.0),          # %
    "light_intensity": (3000.0, 15000.0),  # Lux
    "soil_moisture": (40.0, 70.0),     # %
}


def _day_curve_factor(record_time: datetime) -> float:
    """
    根据时间生成昼夜曲线因子 (0.0 ~ 1.0):
      - 14:00 达到峰值 1.0
      - 02:00 降到谷值 0.0
    用于温度/光照的白昼波动建模。
    """
    hour = record_time.hour + record_time.minute / 60.0
    # 余弦曲线: 14点最高, 2点最低
    import math
    return (math.cos((hour - 14) * math.pi / 12.0) + 1.0) / 2.0


def generate_temperature(record_time: datetime,
                         rng: random.Random,
                         temp_range: tuple[float, float] = DEFAULT_RANGES["temperature"]) -> float:
    """生成温度(℃): 基准 + 昼夜曲线 + 随机扰动。"""
    low, high = temp_range
    mid = (low + high) / 2.0
    amp = (high - low) / 2.0
    factor = _day_curve_factor(record_time)
    base = mid - amp * 0.5 + amp * factor  # 昼夜在 [low, high] 间波动
    noise = rng.uniform(-0.6, 0.6)
    return _round(base + noise, 2)


def generate_humidity(record_time: datetime,
                      rng: random.Random,
                      hum_range: tuple[float, float] = DEFAULT_RANGES["humidity"]) -> float:
    """生成空气湿度(%): 与温度反相关 + 随机扰动。"""
    low, high = hum_range
    mid = (low + high) / 2.0
    factor = _day_curve_factor(record_time)
    # 温度高(白天)湿度低, 温度低(夜间)湿度高
    base = mid + (mid * 0.15) * (1.0 - factor) - (mid * 0.10) * factor
    noise = rng.uniform(-2.0, 2.0)
    value = max(0.0, min(100.0, base + noise))
    return _round(value, 2)


def generate_light_intensity(record_time: datetime,
                             rng: random.Random,
                             light_range: tuple[float, float] = DEFAULT_RANGES["light_intensity"]) -> float:
    """生成光照强度(Lux): 白天高夜间近 0 + 随机扰动。"""
    low, high = light_range
    factor = _day_curve_factor(record_time)
    # 夜间光照趋近 0, 白天在 [low, high] 间
    if factor < 0.05:
        base = rng.uniform(0.0, 50.0)  # 夜间微光
    else:
        base = low + (high - low) * factor
    noise = rng.uniform(-300.0, 300.0)
    value = max(0.0, base + noise)
    return _round(value, 2)


def generate_soil_moisture(record_time: datetime,
                           rng: random.Random,
                           soil_range: tuple[float, float] = DEFAULT_RANGES["soil_moisture"],
                           prev_moisture: float | None = None) -> float:
    """
    生成土壤湿度(%): 缓慢下降趋势(蒸发) + 随机扰动。
    若传入 prev_moisture, 则基于上一次值做衰减(模拟连续蒸发)。
    """
    low, high = soil_range
    if prev_moisture is not None:
        # 蒸发: 每次下降 0.2~0.8%, 偶尔回升(模拟浇水)
        delta = rng.uniform(-0.8, -0.2)
        if rng.random() < 0.05:  # 5% 概率浇水
            delta = rng.uniform(5.0, 12.0)
        value = prev_moisture + delta
    else:
        mid = (low + high) / 2.0
        value = mid + rng.uniform(-5.0, 5.0)
    value = max(0.0, min(100.0, value))
    return _round(value, 2)


# =====================================================================
# 单条记录组装
# =====================================================================
def build_record(plant_id: int,
                 record_time: datetime,
                 rng: random.Random,
                 ranges: dict[str, tuple[float, float]] | None = None,
                 prev_soil: float | None = None) -> dict[str, Any]:
    """
    组装一条 env_data 记录, 字段名/类型对齐数据库 env_data 表。
    返回 dict (JSON 序列化后可直接作为请求体)。
    """
    ranges = ranges or DEFAULT_RANGES
    return {
        "plantId": plant_id,  # 后端通常用驼峰, 联调时确认
        "plant_id": plant_id,  # 兼容蛇形, 联调后删其一
        "temperature": generate_temperature(record_time, rng, ranges["temperature"]),
        "humidity": generate_humidity(record_time, rng, ranges["humidity"]),
        "lightIntensity": generate_light_intensity(record_time, rng, ranges["light_intensity"]),
        "light_intensity": generate_light_intensity(record_time, rng, ranges["light_intensity"]),
        "soilMoisture": generate_soil_moisture(record_time, rng, ranges["soil_moisture"], prev_soil),
        "soil_moisture": generate_soil_moisture(record_time, rng, ranges["soil_moisture"], prev_soil),
        "recordTime": record_time.strftime("%Y-%m-%d %H:%M:%S"),
        "record_time": record_time.strftime("%Y-%m-%d %H:%M:%S"),
        "dataSource": "SIMULATION",
        "data_source": "SIMULATION",
        # 还差想: plant_id 用了两次, 联调确认字段命名规范后统一。
        # 上述驼峰/蛇形成对存在是为了第一天接口契约尚未冻结时兼容两种风格。
    }


# =====================================================================
# 历史数据批量生成 (供 --history 使用, 对接 /batch 接口)
# =====================================================================
def build_history(plant_id: int,
                  days: int,
                  rng: random.Random,
                  ranges: dict[str, tuple[float, float]] | None = None) -> list[dict[str, Any]]:
    """生成近 N 天的历史数据 (每天 SAMPLES_PER_DAY 条)。"""
    ranges = ranges or DEFAULT_RANGES
    records: list[dict[str, Any]] = []
    now = datetime.now()
    end_time = now.replace(minute=0, second=0, microsecond=0)
    total = days * SAMPLES_PER_DAY
    step_minutes = 24 * 60 // SAMPLES_PER_DAY
    start_time = end_time - timedelta(minutes=step_minutes * (total - 1))
    prev_soil: float | None = None
    cur = start_time
    for _ in range(total):
        rec = build_record(plant_id, cur, rng, ranges, prev_soil)
        prev_soil = rec["soil_moisture"]
        records.append(rec)
        cur = cur + timedelta(minutes=step_minutes)
    return records


# =====================================================================
# HTTP 提交
# =====================================================================
def _headers() -> dict[str, str]:
    h = {"Content-Type": "application/json"}
    if API_TOKEN:
        h["Authorization"] = f"Bearer {API_TOKEN}"
    return h


def submit_single(record: dict[str, Any], timeout: float = 5.0) -> dict[str, Any]:
    """POST /api/v1/env-data 单条提交, 返回后端响应 JSON。"""
    resp = requests.post(URL_SINGLE, json=record, headers=_headers(), timeout=timeout)
    resp.raise_for_status()
    try:
        return resp.json()
    except ValueError:
        return {"raw": resp.text, "status_code": resp.status_code}


def submit_batch(records: list[dict[str, Any]], timeout: float = 30.0) -> dict[str, Any]:
    """POST /api/v1/env-data/batch 批量提交。"""
    payload = {"records": records}
    resp = requests.post(URL_BATCH, json=payload, headers=_headers(), timeout=timeout)
    resp.raise_for_status()
    try:
        return resp.json()
    except ValueError:
        return {"raw": resp.text, "status_code": resp.status_code}


# =====================================================================
# CLI
# =====================================================================
def _parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="绿智管家 环境数据模拟生成器")
    p.add_argument("--plant", type=int, default=DEFAULT_PLANT_ID,
                   help=f"目标植物ID (默认 {DEFAULT_PLANT_ID}, 也可用 GW_PLANT_ID 环境变量)")
    p.add_argument("--once", action="store_true",
                   help="只生成一条数据并打印 (默认不提交)")
    p.add_argument("--submit", action="store_true",
                   help="生成后提交到后端 (需配置 GW_TOKEN / GW_API_BASE)")
    p.add_argument("--interval", type=int, default=DEFAULT_INTERVAL,
                   help=f"持续模式: 每 N 秒生成并提交一条 (默认 {DEFAULT_INTERVAL})")
    p.add_argument("--history", type=int, metavar="DAYS",
                   help="批量生成近 N 天历史数据 (配合 --submit 提交到 /batch)")
    p.add_argument("--seed", type=int, default=None, help="随机种子 (可复现)")
    return p.parse_args()


def _print_record(rec: dict[str, Any]) -> None:
    """友好打印单条记录 (只展示关键字段, 避免驼峰/蛇形重复显示)。"""
    pretty = {
        "plant_id": rec["plant_id"],
        "temperature": rec["temperature"],
        "humidity": rec["humidity"],
        "light_intensity": rec["light_intensity"],
        "soil_moisture": rec["soil_moisture"],
        "record_time": rec["record_time"],
        "data_source": rec["data_source"],
    }
    print(json.dumps(pretty, ensure_ascii=False, indent=2))


def main() -> int:
    args = _parse_args()
    rng = random.Random(args.seed)

    # 校验提交前置条件
    if args.submit and not API_TOKEN:
        print("[警告] 未配置 GW_TOKEN, 提交将被后端 401 拒绝。"
              " 请先登录获取 JWT 后设置环境变量 (见 README.md)。", file=sys.stderr)
    if args.submit and not API_BASE:
        print("[错误] 未配置 GW_API_BASE。", file=sys.stderr)
        return 2

    # ---------- 模式 1: 批量历史数据 ----------
    if args.history:
        print(f"[历史数据] 生成近 {args.history} 天数据 (plant_id={args.plant}) ...")
        records = build_history(args.plant, args.history, rng)
        print(f"[历史数据] 共生成 {len(records)} 条, 示例首条:")
        _print_record(records[0])
        if args.submit:
            print(f"[提交] POST {URL_BATCH} ...")
            try:
                resp = submit_batch(records)
                print(f"[提交] 后端响应: {json.dumps(resp, ensure_ascii=False)}")
            except Exception as e:
                print(f"[提交失败] {e}", file=sys.stderr)
                return 1
        else:
            # 落盘一份样本, 便于自检 / 交给后端确认格式
            sample_path = os.path.join(os.path.dirname(__file__), "sample_history.json")
            with open(sample_path, "w", encoding="utf-8") as f:
                json.dump(records, f, ensure_ascii=False, indent=2)
            print(f"[未提交] 已写入样本文件: {sample_path}")
        return 0

    # ---------- 模式 2: 单次生成 ----------
    # 注意: --once 优先于持续模式, 不受 --interval 默认值影响
    if args.once:
        rec = build_record(args.plant, datetime.now(), rng)
        print("[单次生成] 环境数据:")
        _print_record(rec)
        if args.submit:
            try:
                resp = submit_single(rec)
                print(f"[提交] 后端响应: {json.dumps(resp, ensure_ascii=False)}")
            except Exception as e:
                print(f"[提交失败] {e}", file=sys.stderr)
                return 1
        return 0

    # ---------- 模式 3: 持续模拟 ----------
    print(f"[持续模式] 每 {args.interval}s 生成并提交一条 (plant_id={args.plant})。"
          f" Ctrl+C 退出。")
    try:
        while True:
            rec = build_record(args.plant, datetime.now(), rng)
            ts = rec["record_time"]
            if args.submit:
                try:
                    resp = submit_single(rec)
                    print(f"[{ts}] 提交成功: {json.dumps(resp, ensure_ascii=False)}")
                except Exception as e:
                    print(f"[{ts}] 提交失败: {e}", file=sys.stderr)
            else:
                print(f"[{ts}] 生成: temp={rec['temperature']}℃ "
                      f"hum={rec['humidity']}% light={rec['light_intensity']}Lux "
                      f"soil={rec['soil_moisture']}%")
            time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\n[退出] 已停止模拟。")
        return 0


if __name__ == "__main__":
    sys.exit(main())
