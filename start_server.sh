#!/bin/bash
cd /var/www/bingosys-server-ws
pkill -9 -f BingoSysServer
echo "Iniciando servidor..."
nohup ./bin/BingoSysServer 3000 /var/www/bingosys-server-ws/data/75x15BINGO.txt > server.log 2>&1 &
sleep 2
if pgrep -f BingoSysServer > /dev/null; then
    echo "Servidor iniciado com sucesso."
    tail -n 20 server.log
else
    echo "Falha ao iniciar o servidor."
    cat server.log
fi
