#!/bin/bash
RAW_KEY=$(./get_key.sh)
KEY=$(echo "$RAW_KEY" | grep -o 'TEST-KEY-[0-9A-Za-z-]*' | head -n 1)
echo "Using Key: $KEY"
echo "--- DB Check ---"
sudo -u postgres psql -P pager=off -d bingosys -c "SELECT codigo_chave, status, sorteio_id FROM chaves_acesso WHERE codigo_chave = '$KEY';"
echo "----------------"
python3 verify_prizes.py "$KEY"
