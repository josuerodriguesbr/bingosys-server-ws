#!/bin/bash
PGPASSWORD=bingosys psql -h localhost -U bingosys -d bingosys -c "SELECT id, nome_premio, base_id FROM PREMIACOES WHERE sorteio_id = 93;"
PGPASSWORD=bingosys psql -h localhost -U bingosys -d bingosys -c "SELECT sp.id, sp.premio_id, sp.tipo, sp.descricao, sp.realizada FROM SUB_PREMIOS sp JOIN PREMIACOES p ON p.id = sp.premio_id WHERE p.sorteio_id = 93;"
