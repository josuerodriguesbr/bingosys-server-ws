-- Migração BingoSys v2: Mudança de Estrutura de Bases e Prêmios

BEGIN;

-- 1. Alterações na tabela SORTEIOS
-- Adicionar campos de agendamento
ALTER TABLE SORTEIOS ADD COLUMN data_sorteio DATE;
ALTER TABLE SORTEIOS ADD COLUMN hora_sorteio_inicio TIME;
ALTER TABLE SORTEIOS ADD COLUMN hora_sorteio_fim TIME;

-- 2. Alterações na tabela PREMIACOES
-- Adicionar base_id e configuracoes (antiga preferencias)
ALTER TABLE PREMIACOES ADD COLUMN base_id INTEGER REFERENCES BASES_DADOS(id);
ALTER TABLE PREMIACOES ADD COLUMN configuracoes JSONB;

-- 3. Migração de dados (Opcional - para não perder dados de testes atuais)
-- Move o base_id e preferencias dos sorteios para as premiações vinculadas
UPDATE PREMIACOES p
SET base_id = s.base_id,
    configuracoes = s.preferencias
FROM SORTEIOS s
WHERE p.sorteio_id = s.id;

-- 4. Remoção de campos obsoletos em SORTEIOS
ALTER TABLE SORTEIOS DROP COLUMN base_id;
ALTER TABLE SORTEIOS DROP COLUMN preferencias;

COMMIT;
