# 绿智管家 - AI 模块 (ai-module)

> 成员3 · 数据分析与测试工程师 负责的模块
> 第一天交付: 环境数据模拟器 + 健康评分规则草案

## 目录结构

```
ai-module/
├── data_simulator.py        # 环境数据模拟生成器 (第一天)
├── health_analyzer.py       # 健康评分引擎        (第三天)
├── suggestion_engine.py     # 养护建议生成        (第三天)
├── health_scoring_rules.md  # 健康评分规则草案    (第一天)
├── greenwise-openapi.yaml   # API 规范 (导入 Apifox, 18个接口)
├── requirements.txt         # Python 依赖
├── sample_history.json      # 历史数据样本 (--history 生成, 自检用)
└── README.md                # 本文件
```

---

## 1. 环境准备

### 1.1 Python 环境

要求 Python 3.10+(当前机器 3.11.9 已满足)。推荐用虚拟环境:

```bash
cd greenwise/ai-module
python -m venv .venv
.venv\Scripts\activate          # Windows
# source .venv/bin/activate     # Linux/Mac
pip install -r requirements.txt
```

### 1.2 环境变量配置(必读)

`data_simulator.py` 通过环境变量读取后端地址和登录凭证,避免硬编码。

| 变量名 | 说明 | 默认值 | 示例 |
|--------|------|--------|------|
| `GW_API_BASE` | 后端服务地址 | `http://localhost:8080` | `http://localhost:8080` |
| `GW_TOKEN` | 登录后端获取的 JWT(第二天才有登录接口) | 空 | `eyJhbGciOi...` |
| `GW_PLANT_ID` | 默认目标植物 ID | `1` | `1` |

**Windows (cmd) 设置示例:**

```cmd
set GW_API_BASE=http://localhost:8080
set GW_TOKEN=eyJhbGciOiJIUzI1NiJ9...
set GW_PLANT_ID=1
```

**Windows (PowerShell) 设置示例:**

```powershell
$env:GW_API_BASE = "http://localhost:8080"
$env:GW_TOKEN = "eyJhbGciOiJIUzI1NiJ9..."
$env:GW_PLANT_ID = "1"
```

> ⚠️ 第一天后端尚未实现登录/JWT(第二天才做),所以 `--submit` 会因 401 失败属正常。
> 第一天晚上先用 `--once`(不提交)验证数据格式,用 Postman 手动测接口。

---

## 2. data_simulator.py 用法

### 2.1 单次生成一条(自检数据格式,不提交)

```bash
python data_simulator.py --once
```

输出示例:
```json
{
  "plant_id": 1,
  "temperature": 24.35,
  "humidity": 55.20,
  "light_intensity": 8200.00,
  "soil_moisture": 58.10,
  "record_time": "2026-07-14 15:30:00",
  "data_source": "SIMULATION"
}
```

### 2.2 单次生成并提交单条

```bash
set GW_TOKEN=<你的JWT>
python data_simulator.py --once --submit
```

### 2.3 持续模拟(每 30 秒一条)

```bash
python data_simulator.py --interval 30 --submit
```

### 2.4 批量生成近 7 天历史数据(配合演示)

```bash
# 仅生成,不提交(写入 sample_history.json)
python data_simulator.py --history 7 --plant 1

# 生成并提交到 /batch 接口
python data_simulator.py --history 7 --plant 1 --submit
```

### 2.5 完整参数

```
--plant      目标植物ID (默认读 GW_PLANT_ID 或 1)
--once       只生成一条并打印 (默认不提交)
--submit     生成后提交到后端 (需配置 GW_TOKEN)
--interval   持续模式: 每 N 秒一条 (默认 30)
--history N  批量生成近 N 天历史数据
--seed       随机种子 (可复现, 测试用)
```

---

## 3. 数据字段对齐说明

模拟器生成的字段严格对齐数据库 `env_data` 表(plan_v2 §4.2.3):

| 模拟器字段 | 数据库字段 | 类型 | 说明 |
|------------|------------|------|------|
| temperature | temperature | DECIMAL(5,2) | 温度 ℃ |
| humidity | humidity | DECIMAL(5,2) | 空气湿度 % |
| light_intensity | light_intensity | DECIMAL(8,2) | 光照 Lux |
| soil_moisture | soil_moisture | DECIMAL(5,2) | 土壤湿度 % |
| record_time | record_time | DATETIME | 记录时间 |
| data_source | data_source | VARCHAR(20) | 固定 'SIMULATION' |

> ⚠️ **字段命名待与成员1确认**: 后端 JSON 习惯用驼峰(`plantId`),数据库用蛇形(`plant_id`)。
> 第一天为兼容两种风格,模拟器同时输出两套字段名。**联调确认后删除冗余一套**。
> 此事项已记录在 `health_scoring_rules.md` §7 待确认清单。

---

## 4. Apifox 配置说明(第一天交付物)

第一天交付物检查清单要求 "接口测试环境配置完成"。使用 Apifox 管理 API 文档 + Mock + 自动化测试。

> **核心优势**: 本模块已提供 OpenAPI 规范文件 `greenwise-openapi.yaml`,导入 Apifox 后
> 接口文档、Mock 服务、自动化测试场景一次性生成,无需手动逐个建接口。
> 这正是成员3"100%接口覆盖"职责的利器。

### 4.1 导入 OpenAPI 规范(一键生成 API 文档)

1. 打开 Apifox → 新建项目 `greenwise`
2. 项目设置 → 数据管理 → 导入数据
3. 选择 **OpenAPI/Swagger** 格式,导入文件:
   `greenwise/ai-module/greenwise-openapi.yaml`
4. 导入后自动生成:
   - **18 个接口方法**(覆盖 plan_v2 §5.2 全部接口)
   - **14 个数据 Schema**(请求/响应模型)
   - **5 个分组标签**:用户 / 植物 / 环境数据 / 健康分析 / Dashboard
   - 每个 Schema 带 example,可直接用于 Mock

### 4.2 配置环境变量

Apifox 顶部 → 环境管理 → 新建环境 `greenwise-local`:

| 变量名 | 初始值 | 说明 |
|--------|--------|------|
| `base_url` | `http://localhost:8080` | 后端地址 |
| `api_prefix` | `/api/v1` | 接口前缀 |
| `token` | (空,登录后自动写入) | JWT,见 §4.4 自动提取 |
| `plant_id` | `1` | 测试植物 ID |

> 服务 URL 在 OpenAPI 里已定义为 `{{base_url}}{{api_prefix}}`,导入后自动绑定环境变量。

### 4.3 全局参数 / 鉴权配置

为避免每个接口手动加 Header,配置全局鉴权:
1. 项目设置 → 公共参数 → Header
2. 添加 `Authorization: Bearer {{token}}`
3. 这样所有标记 `BearerAuth` 的接口自动携带 Token

### 4.4 自动提取 Token(登录后无需手动复制)

在 `POST /user/login` 接口的「后置操作」里配置**提取变量**:
- 提取响应 JSON 的 `data.token` → 写入环境变量 `token`

之后调用任何需登录的接口,Token 自动携带,无需手动粘贴。

### 4.5 自动化测试场景(对应"100%接口覆盖"职责)

Apifox 的「测试场景」功能可串联多接口,自动跑全链路。建议建以下场景:

**场景 1: 核心数据流闭环(对应 plan_v2 §9.1 Demo 场景一)**
```
1. POST /user/register        (注册测试用户)
2. POST /user/login           (登录,自动提取 token)
3. POST /plant                (添加植物"阳台绿萝",提取 plantId)
4. POST /env-data/batch       (上传7天历史数据)
5. GET  /env-data/{plantId}/latest  (验证最新数据)
6. GET  /health/{plantId}/score    (验证AI评分)
7. GET  /health/{plantId}/report   (验证完整报告)
8. GET  /dashboard/summary    (验证汇总)
```

**场景 2: 异常场景测试(对应 plan_v2 §8.2 测试清单)**
```
1. POST /env-data 缺失字段     → 期望 code 3001
2. POST /env-data 数据类型错误   → 期望 code 3001/400
3. GET  /plant/99999           → 期望 code 2001
4. 不带 Token 调用需登录接口     → 期望 code 401
```

> 场景中每个步骤可设置**断言**:检查 `code` 是否等于预期值、`data` 非空等。
> 这就是 plan_v2 §8.1 要求的"100%接口覆盖"的落地方式。

### 4.6 Mock 服务(成员1后端未就绪时前端/测试可用)

导入 OpenAPI 后,Apifox 自动为每个接口生成 Mock:
1. 项目设置 → Mock 设置 → 启用 Mock 服务
2. 启用后,未实现的接口也能返回符合 Schema 的假数据
3. **用途**: 成员1后端接口还在开发时,成员3可先用 Mock 验证模拟器对接逻辑,
   成员2前端也能并行开发,三方不互相阻塞(缓解风险 R1/R2)

### 4.7 第一天测试建议

- 后端第一天只建表+基础 CRUD,**JWT 鉴权是第二天才做**,所以第一天晚上测接口可能遇到 401/404 属正常。
- 第一天重点验证:
  1. OpenAPI 导入 Apifox 成功,18 个接口可见
  2. `POST /env-data` 的 Mock 数据格式正确(对照 §3 字段表)
  3. 用 `python data_simulator.py --once` 生成一条真实数据,手动粘贴到 Apifox 请求体测试
- **数据格式正确、字段名对齐、数值精度符合 DECIMAL 定义**——这是第一天模拟器的核心价值。

### 4.8 第一天交付物对照

| 交付物检查清单项 | Apifox 中的体现 |
|------------------|-----------------|
| ☐ 数据模拟脚本可运行 | `data_simulator.py --once` 输出 JSON |
| ☐ 健康评分规则草案完成 | `health_scoring_rules.md` |
| ☐ Postman/Apifox 环境配置完成 | 本节:OpenAPI 导入 + 环境变量 + 全局鉴权 + 测试场景 |

---

## 5. Git 提交规范(对齐 plan_v2 §3.2.1)

```bash
# 个人分支
git checkout -b feature/member3-data-simulator

# 提交 (每天至少 2 次: 午间/晚间)
git add ai-module/
git commit -m "feat(data-simulator): 完成环境数据模拟器初版,支持单条/批量/历史数据生成"
git commit -m "docs(ai): 完成健康评分规则草案 v0.1"
```

commit 前缀: `feat` / `fix` / `docs` / `refactor` / `test`

---

## 6. 后续计划

| 时间 | 任务 |
|------|------|
| 第一天 ✅ | 数据模拟器 + 评分规则草案 |
| 第二天 | 历史数据批量生成完善 + 评分规则细化表 + 数据上传链路验证 |
| 第三天 | `health_analyzer.py` + `suggestion_engine.py` 落地 + 联调 |
| 第四天 | 全流程测试用例 + Bug 跟踪 |
| 第五天 | Demo 演示脚本 + 项目 README |
