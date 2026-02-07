-- Migration v3: Suporte a Sub-Prêmios (Hierarquia de Premiações)
CREATE TABLE IF NOT EXISTS SUB_PREMIOS (
    id SERIAL PRIMARY KEY,
    premio_id INTEGER REFERENCES PREMIACOES(id) ON DELETE CASCADE,
    tipo VARCHAR(50) NOT NULL,
    realizada BOOLEAN DEFAULT FALSE,
    padrao_grade JSONB,
    ordem_exibicao INTEGER DEFAULT 0
);

INSERT INTO SUB_PREMIOS (premio_id, tipo, realizada, padrao_grade, ordem_exibicao)
SELECT id, tipo, realizada, padrao_grade, ordem_exibicao FROM PREMIACOES;

ALTER TABLE PREMIACOES DROP COLUMN tipo;
ALTER TABLE PREMIACOES DROP COLUMN realizada;
ALTER TABLE PREMIACOES DROP COLUMN padrao_grade;
