COPY (SELECT * FROM PREMIACOES WHERE sorteio_id = 93) TO 'C:\Users\josue\prizes_93.csv' WITH CSV HEADER;
COPY (SELECT sp.* FROM SUB_PREMIOS sp JOIN PREMIACOES p ON p.id = sp.premio_id WHERE p.sorteio_id = 93) TO 'C:\Users\josue\subprizes_93.csv' WITH CSV HEADER;
