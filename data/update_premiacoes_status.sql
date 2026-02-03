-- Migração: Adicionar controle de encerramento de premiações
-- Data: 2026-02-02

ALTER TABLE public.premiacoes ADD COLUMN IF NOT EXISTS realizada BOOLEAN DEFAULT FALSE;

COMMENT ON COLUMN public.premiacoes.realizada IS 'Indica se a premiação já foi entregue/efetuada no sorteio atual';
