import aiohttp
from astrbot.api.event import filter, AstrMessageEvent
from astrbot.api.star import Context, Star, register
from astrbot.api import logger

@register("astrbot_plugin_6657_meme", "AI", "6657烂梗关键词插件", "1.0.0")
class Meme6657(Star):
    def __init__(self, context: Context):
        super().__init__(context)
        self.api_url = "https://api.krab.cn/api/random-6657-meme"

    # ✅ 用命令触发，不监听所有消息，不干扰其他插件
    @filter.command("6657")
    async def meme_cmd(self, event: AstrMessageEvent):
        """发送 /6657 触发烂梗"""
        meme_data = await self._fetch_meme()
        if not meme_data:
            yield event.plain_result("获取烂梗失败，请稍后再试~")
            return

        formatted = self._format_meme(meme_data)
        yield event.plain_result(formatted)

    async def _fetch_meme(self) -> dict | None:
        try:
            timeout = aiohttp.ClientTimeout(total=10)
            async with aiohttp.ClientSession(timeout=timeout) as session:
                async with session.get(
                    self.api_url,
                    headers={"User-Agent": "AstrBot/1.0"}
                ) as resp:
                    if resp.status != 200:
                        return None
                    data = await resp.json()

            if data.get("success") and data.get("meme"):
                return data["meme"]
        except Exception as e:
            logger.error(f"获取烂梗失败: {e}")
        return None

    def _format_meme(self, meme: dict) -> str:
        content = meme.get("content", "")
        tags = meme.get("tags", [])
        tag_labels = [tag.get("label", "") for tag in tags if tag.get("label")]

        if tag_labels:
            tags_str = " ".join([f"#{tag}" for tag in tag_labels])
        else:
            tags_str = "#🎲随机烂梗"

        formatted = f"#随机烂梗 {tags_str}\n{content}"
        return formatted