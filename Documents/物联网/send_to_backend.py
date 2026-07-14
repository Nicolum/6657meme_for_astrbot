import json
import requests

URL = "http://localhost:8080/api/v1/env-data/batch"  # 跟成员1确认地址
TOKEN = "你的JWT_TOKEN"  # 登录后从成员1那拿到

with open as f:
    data = json.load

headers = {
    "Content-Type": "application/json",
    "Authorization": f"Bearer {TOKEN}"
}

resp = requests.post
print