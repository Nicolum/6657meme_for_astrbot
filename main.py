# -*- coding: utf-8 -*-
"""6657 烂梗插件 v1.1.0

本次修复的问题（对照旧版 main.py）：
1. 旧版 `resp.status != 200` 直接 return None，不打任何日志 —— 上游返回
   403/429/530/502 时根本看不出原因。现在把 status + 响应体前 300 字打进日志。
2. 旧版 `await resp.json()` 依赖 aiohttp 严格的 Content-Type 校验（默认
   content_type="application/json"）。上游被 Cloudflare/EdgeOne 拦截时返回的是
   text/html 错误页，会抛 ContentTypeError 并被 `except Exception` 吞掉。
   现在先 `await resp.text()` 再 `json.loads()`，永不吃 MIME。
3. 旧版每个请求新建 ClientSession，无连接复用、无重试、无备用源。
   现在复用会话 + 2 轮重试 + 双源（镜像优先，Krab 官方兜底）。
4. 旧版 tags 是逗号分隔多值（"02,14,15"），却用单 key 查 tag_map，永远匹配
   不到，输出成 "#02,14,15"。现在按逗号拆开逐个映射。
5. 旧版 barrage 为空串时仍会 yield 空消息。现在空正文走兜底文案。
6. 新增 /6657diag 诊断指令：现场打两个接口，回报 HTTP 状态、耗时、响应体片段。
"""

from __future__ import annotations

import asyncio
import json
import time

import aiohttp

from astrbot.api import logger
from astrbot.api.event import AstrMessageEvent, filter
from astrbot.api.star import Context, Star, register

# 数据源：镜像优先（Krab 官方隧道时有 530，放在兜底位）
API_SOURCES = (
    "https://api.hguofichp.cn/machine/getRandOne",
    "https://api.krab.cn/api/random-6657-meme",
)

TAG_MAP = {
    "00": "喷玩机器",
    "01": "选手",
    "02": "P1",
    "03": "QUQU",
    "05": "MYGO",
    "06": "群魔乱舞",
    "09": "互喷",
}

HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"
    ),
    "Referer": "https://sb6657.cn/",
    "Accept": "application/json, text/plain, */*",
    "Accept-Language": "zh-CN,zh;q=0.9",
    "Connection": "keep-alive",
}

TIMEOUT = aiohttp.ClientTimeout(total=10, connect=5)


@register("astrbot_plugin_6657_meme", "AI", "6657烂梗关键词插件", "1.1.0")
class Meme6657(Star):
    def __init__(self, context: Context):
        super().__init__(context)
        self._session: aiohttp.ClientSession | None = None
        self._session_lock = asyncio.Lock()

    # ---------- 基础设施 ----------

    async def _get_session(self) -> aiohttp.ClientSession:
        if self._session is None or self._session.closed:
            async with self._session_lock:
                if self._session is None or self._session.closed:
                    self._session = aiohttp.ClientSession(
                        timeout=TIMEOUT,
                        headers=HEADERS,
                        trust_env=True,  # 走系统代理（有代理环境时不会连不上）
                    )
        return self._session

    async def terminate(self):
        if self._session and not self._session.closed:
            await self._session.close()
        self._session = None

    # ---------- 指令 ----------

    @filter.command("6657", alias={"烂梗"})
    async def meme_cmd(self, event: AstrMessageEvent):
        """发送 /6657 触发一条随机烂梗"""
        meme = await self._fetch_meme()
        if not meme:
            yield event.plain_result("获取烂梗失败，请稍后再试~")
            return
        yield event.plain_result(self._format_meme(*meme))

    @filter.command("6657diag")
    async def diag_cmd(self, event: AstrMessageEvent):
        """诊断：现场探测所有数据源的真实状态"""
        lines = ["【6657 接口诊断】"]
        for url in API_SOURCES:
            try:
                session = await self._get_session()
                t0 = time.perf_counter()
                async with session.get(url) as resp:
                    cost = (time.perf_counter() - t0) * 1000
                    text = await resp.text(errors="replace")
                    ctype = resp.headers.get("Content-Type", "-")
                    body = " ".join(text.split())[:200]
                    lines.append(
                        f"{url}\n  状态 {resp.status} | 耗时 {cost:.0f}ms | "
                        f"类型 {ctype}\n  响应 {body}"
                    )
            except Exception as e:  # noqa: BLE001 诊断要尽量全信息
                lines.append(f"{url}\n  异常 {type(e).__name__}: {e}")
        yield event.plain_result("\n".join(lines))

    # ---------- 网络 ----------

    async def _fetch_meme(self) -> tuple[str, list[str], str] | None:
        """返回 (正文, 标签列表, 点赞数)，全部失败返回 None。"""
        last_error = "unknown"
        session = await self._get_session()

        for attempt in range(2):
            for url in API_SOURCES:
                try:
                    async with session.get(url) as resp:
                        text = await resp.text(errors="replace")
                        if resp.status != 200:
                            snippet = " ".join(text.split())[:300]
                            last_error = f"{url} -> HTTP {resp.status}: {snippet}"
                            self.logger.warning("[6657] %s", last_error)
                            continue
                        try:
                            payload = json.loads(text)
                        except json.JSONDecodeError:
                            last_error = (
                                f"{url} -> 非 JSON 响应: "
                                f"{' '.join(text.split())[:300]}"
                            )
                            self.logger.warning("[6657] %s", last_error)
                            continue

                    parsed = self._parse_payload(payload)
                    if parsed:
                        return parsed
                    last_error = f"{url} -> 字段解析为空: {str(payload)[:300]}"
                    self.logger.warning("[6657] %s", last_error)

                except asyncio.TimeoutError:
                    last_error = f"{url} -> 请求超时(10s)"
                    self.logger.warning("[6657] %s", last_error)
                except aiohttp.ClientError as e:
                    last_error = f"{url} -> 网络异常 {type(e).__name__}: {e}"
                    self.logger.warning("[6657] %s", last_error)
                except Exception as e:  # noqa: BLE001
                    last_error = f"{url} -> 未知异常 {type(e).__name__}: {e}"
                    self.logger.error("[6657] %s", last_error, exc_info=True)

            if attempt == 0:
                await asyncio.sleep(0.8)

        self.logger.error("[6657] 所有数据源均失败，最后一次错误: %s", last_error)
        return None

    # ---------- 解析 ----------

    @staticmethod
    def _parse_payload(payload) -> tuple[str, list[str], str] | None:
        if not isinstance(payload, dict):
            return None

        # 形态 A（Krab 官方）: {"success": true, "meme": {"content":..., "tags":[{"code","label"}]}}
        meme = payload.get("meme")
        if isinstance(meme, dict):
            content = str(meme.get("content") or "").strip()
            if not content:
                return None
            labels: list[str] = []
            for tag in meme.get("tags") or []:
                if not isinstance(tag, dict):
                    continue
                label = str(tag.get("label") or "").strip()
                label = label or TAG_MAP.get(str(tag.get("code") or ""), "")
                if label and label not in labels:
                    labels.append(label)
            return content, labels, str(meme.get("copy_count") or "")

        # 形态 B（hguofichp 镜像）: {"code": 200, "data": {"barrage":..., "tags":"02,14,15"}}
        if payload.get("code") == 200:
            data = payload.get("data")
            if not isinstance(data, dict):
                return None
            content = str(data.get("barrage") or "").strip()
            if not content:
                return None
            labels = []
            raw_tags = data.get("tags") or ""
            for code in str(raw_tags).replace("，", ",").split(","):
                code = code.strip()
                label = TAG_MAP.get(code, code)
                if label and label not in labels:
                    labels.append(label)
            return content, labels, str(data.get("cnt") or "")

        return None

    # ---------- 输出 ----------

    @staticmethod
    def _format_meme(content: str, labels: list[str], count: str) -> str:
        tag_part = " ".join(f"#{label}" for label in labels) if labels else "#随机烂梗"
        head = f"🎲 {tag_part}"
        if count and count.isdigit():
            head += f" · 🔁{count}"
        return f"{head}\n{content}"
