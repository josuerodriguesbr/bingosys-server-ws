#!/bin/bash
PGPASSWORD=bingosys psql -h localhost -U bingosys -d bingosys -c "SELECT sp.tipo, p.sorteio_id FROM SUB_PREMIOS sp JOIN PREMIACOES p ON p.id = sp.premio_id WHERE sp.tipo ILIKE '%cheia%';"
