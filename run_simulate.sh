#!/bin/bash
RAW_KEY=$(./get_key.sh)
KEY=$(echo "$RAW_KEY" | grep -o 'TEST-KEY-[0-9A-Za-z-]*' | head -n 1)
echo "Using Key: $KEY"
python3 simulate_game.py "$KEY"
