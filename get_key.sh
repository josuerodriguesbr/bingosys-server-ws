#!/bin/bash
PASS="lenovo112423"
# Always create a new valid key to ensure test success
SID=$(echo "$PASS" | sudo -S -u postgres psql -d bingosys -t -A -c "INSERT INTO sorteios (status, modelo_id) VALUES ('configurando', 1) RETURNING id;" | grep -o '[0-9]*' | head -n1)
if [ -z "$SID" ]; then
    echo "Error creating sorteio" >&2
    exit 1
fi
NEW_KEY="TEST-KEY-$SID-$$"
echo "$PASS" | sudo -S -u postgres psql -d bingosys -c "INSERT INTO chaves_acesso (codigo_chave, sorteio_id, status) VALUES ('$NEW_KEY', $SID, 'ativa');"
echo "$NEW_KEY"
