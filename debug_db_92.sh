#!/bin/bash
PGPASSWORD=bingosys psql -h localhost -U bingosys -d bingosys -c "SELECT sp.id, sp.premio_id, sp.tipo, sp.descricao, sp.realizada FROM SUB_PREMIOS sp JOIN PREMIACOES p ON p.id = sp.premio_id WHERE p.sorteio_id = 92;"
