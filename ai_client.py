#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ChemStation AI Multi-Model Client (0-Token Local & Free Cloud)
Hỗ trợ chuyển đổi linh hoạt:
1. Local 0-Token (Ollama): qwen2.5-coder:1.5b, deepseek-r1:1.5b (Offline 100%, 0 đồng)
2. Free Cloud (OpenRouter): google/gemma-4-31b-it:free, nvidia/nemotron-3-nano-omni-30b-a3b-reasoning:free, cohere/north-mini-code:free
3. An toàn tuyệt đối: Không lưu cứng API Key bí mật trong mã nguồn (Tuân thủ CWE-798).
"""

import os
import sys
import json
import urllib.request
import urllib.error

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

# Cấu hình OpenRouter & Ollama (Đọc an toàn từ biến môi trường)
OLLAMA_URL = os.environ.get("OLLAMA_URL", "http://127.0.0.1:11434/api/generate")
OPENROUTER_URL = "https://openrouter.ai/api/v1/chat/completions"
OPENROUTER_API_KEY = os.environ.get("OPENROUTER_API_KEY", "")

# Danh sách model miễn phí chất lượng cao nhất hiện tại trên OpenRouter
FREE_MODELS = {
    "gemma-9b": "google/gemma-2-9b-it:free",
    "llama-70b": "meta-llama/llama-3.3-70b-instruct:free",
    "deepseek-r1": "deepseek/deepseek-r1:free",
    "qwen-coder": "qwen/qwen-2.5-coder-32b-instruct:free",
    "mistral-7b": "mistralai/mistral-7b-instruct:free"
}

def chat_local(prompt: str, model: str = "qwen2.5-coder:1.5b") -> str:
    """Gọi mô hình AI Local qua Ollama (Hoàn toàn 0 token, miễn phí 100%)."""
    payload = {
        "model": model,
        "prompt": prompt,
        "stream": False
    }
    req = urllib.request.Request(
        OLLAMA_URL,
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"}
    )
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            return data.get("response", "").strip()
    except Exception as e:
        return f"[Lỗi Local Ollama]: {e}. Hãy đảm bảo Ollama đang chạy tại {OLLAMA_URL}."

def chat_openrouter(prompt: str, model: str = "google/gemma-4-31b-it:free", api_key: str = None) -> str:
    """Gọi mô hình AI đám mây miễn phí qua OpenRouter Free Tier."""
    key = api_key or OPENROUTER_API_KEY
    if not key:
        return "[Lỗi OpenRouter]: Chưa cấu hình OPENROUTER_API_KEY. Vui lòng thiết lập biến môi trường hoặc dùng Local Ollama."
    payload = {
        "model": model,
        "messages": [{"role": "user", "content": prompt}]
    }
    req = urllib.request.Request(
        OPENROUTER_URL,
        data=json.dumps(payload).encode("utf-8"),
        headers={
            "Authorization": f"Bearer {key}",
            "HTTP-Referer": "http://localhost:8080",
            "X-Title": "ChemStation AI",
            "Content-Type": "application/json"
        }
    )
    try:
        with urllib.request.urlopen(req, timeout=25) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            return data["choices"][0]["message"]["content"].strip()
    except Exception as e:
        return f"[Lỗi OpenRouter]: {e}"

def ask_ai(prompt: str, mode: str = "auto", model_alias: str = "gemma-31b") -> str:
    """Tự động chuyển đổi thông minh: Ưu tiên Cloud nếu có API key, tự động fallback về Local Ollama."""
    if mode in ("cloud", "auto") and (OPENROUTER_API_KEY or mode == "cloud"):
        target_model = FREE_MODELS.get(model_alias, model_alias)
        res = chat_openrouter(prompt, model=target_model)
        if not res.startswith("[Lỗi"):
            return res
        print(f"[Cảnh báo] Cloud gặp lỗi hoặc chưa cấu hình ({res}), tự động chuyển về Local AI (Ollama)...")

    return chat_local(prompt, model="qwen2.5-coder:1.5b")

def analyze_water_data(ph: float, tds: float, ntu: float, temp: float, do_val: float, mode: str = "auto") -> str:
    """Tự động phân tích chất lượng nước phao quan trắc USV theo chuẩn QCVN 08:2023/BTNMT."""
    prompt = f"""Bạn là chuyên gia thủy văn và môi trường nước. Hãy phân tích các chỉ số nguồn nước sau từ phao quan trắc USV ChemStation:
- Độ pH: {ph:.2f}
- Độ đục NTU: {ntu:.0f}
- Tổng chất rắn hòa tan TDS: {tds:.0f} ppm
- Nhiệt độ nước: {temp:.1f} °C
- Oxy hòa tan DO: {do_val:.2f} mg/L

Hãy đưa ra nhận định ngắn gọn:
1. Đánh giá chất lượng nước theo quy chuẩn QCVN 08:2023/BTNMT Việt Nam (nước mặt, nước sinh hoạt, tưới tiêu).
2. Đưa ra 2-3 khuyến nghị tác vụ vận hành phao cụ thể (sục rửa đầu dò, lấy mẫu nước, thu tời về Home)."""
    return ask_ai(prompt, mode=mode, model_alias="gemma-31b")

if __name__ == "__main__":
    print("=" * 65)
    print("  CHEMSTATION AI — MULTI-MODEL ZERO-TRUST TEST")
    print("=" * 65)
    print("[*] Đang gửi mẫu nước phân tích thử nghiệm...")
    report = analyze_water_data(ph=6.8, tds=195, ntu=18, temp=28.2, do_val=5.8, mode="auto")
    print("\n[KẾT QUẢ TỪ AI]:\n")
    print(report)
    print("\n" + "=" * 65)
