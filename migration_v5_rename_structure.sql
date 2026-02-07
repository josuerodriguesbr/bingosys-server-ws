-- Migração v5: Reestruturação Semântica
-- Objetivo: Renomear tabelas para refletir o novo modelo de Rodadas e Prêmios

-- 1. Renomear a tabela pai: PREMIACOES -> RODADAS
ALTER TABLE PREMIACOES RENAME TO RODADAS;
ALTER TABLE RODADAS RENAME COLUMN nome_premio TO nome_rodada;

-- 2. Renomear a tabela filha: SUB_PREMIOS -> PREMIOS
ALTER TABLE SUB_PREMIOS RENAME TO PREMIOS;
ALTER TABLE PREMIOS RENAME COLUMN premio_id TO rodada_id;

-- 3. Renomear sequências para manter o padrão (opcional, mas recomendado)
-- Nota: O PostgreSQL renomeia os nomes internos, mas manter o nome da sequência legível é melhor.
DO $$ 
BEGIN
    IF EXISTS (SELECT 1 FROM pg_class WHERE relname = 'premiacoes_id_seq') THEN
        ALTER SEQUENCE premiacoes_id_seq RENAME TO rodadas_id_seq;
    END IF;
    IF EXISTS (SELECT 1 FROM pg_class WHERE relname = 'sub_premios_id_seq') THEN
        ALTER SEQUENCE sub_premios_id_seq RENAME TO premios_id_seq;
    END IF;
END $$;

COMMENT ON TABLE RODADAS IS 'Armazena as rodadas (blocos de sorteio) de um evento de bingo';
COMMENT ON TABLE PREMIOS IS 'Armazena os tipos de prêmios/regras (Quina, Forma, Cheia) de uma rodada';
