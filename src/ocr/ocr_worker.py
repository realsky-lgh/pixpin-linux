#!/usr/bin/env python3
"""
PixPin Linux - RapidOCR Worker Process
Communicates with C++ PixPin via stdin/stdout line-protocol.
"""

import sys
import json
import base64

try:
    from rapidocr_onnxruntime import RapidOCR
    engine = RapidOCR()
except Exception as e:
    sys.stderr.write(f"Failed to initialize RapidOCR: {e}\n")
    sys.exit(1)

sys.stdout.write("READY\n")
sys.stdout.flush()

for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    if line == "QUIT":
        break
    try:
        if line.startswith("PATH:"):
            img_path = line[5:]
            result, _ = engine(img_path)
        elif line.startswith("B64:"):
            raw_data = base64.b64decode(line[4:])
            result, _ = engine(raw_data)
        else:
            result, _ = engine(line)
        
        texts = []
        if result:
            for item in result:
                texts.append(item[1])
        output = "\n".join(texts)
        resp = json.dumps({"status": "ok", "text": output}, ensure_ascii=False)
        sys.stdout.write(resp + "\n")
        sys.stdout.flush()
    except Exception as e:
        resp = json.dumps({"status": "error", "message": str(e)}, ensure_ascii=False)
        sys.stdout.write(resp + "\n")
        sys.stdout.flush()
