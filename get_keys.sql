SELECT sorteio_id, codigo_chave FROM chaves_acesso WHERE status = 'ativa' OR (usado_em IS NULL AND sorteio_id IS NOT NULL) LIMIT 5;
