-- 1. Remove dependências (Sorteios e Vendas) vinculados a bases que serão removidas
DELETE FROM CARTELAS_VALIDADAS WHERE sorteio_id IN (
    SELECT id FROM SORTEIOS WHERE base_id IN (
        SELECT id FROM BASES_DADOS WHERE caminho_dados LIKE '%base-cartelas%' OR caminho_dados = 'data/75x15BINGO.txt'
    )
);

DELETE FROM BOLAS_SORTEADAS WHERE sorteio_id IN (
    SELECT id FROM SORTEIOS WHERE base_id IN (
        SELECT id FROM BASES_DADOS WHERE caminho_dados LIKE '%base-cartelas%' OR caminho_dados = 'data/75x15BINGO.txt'
    )
);

DELETE FROM PREMIACOES WHERE sorteio_id IN (
    SELECT id FROM SORTEIOS WHERE base_id IN (
        SELECT id FROM BASES_DADOS WHERE caminho_dados LIKE '%base-cartelas%' OR caminho_dados = 'data/75x15BINGO.txt'
    )
);

DELETE FROM CHAVES_ACESSO WHERE sorteio_id IN (
    SELECT id FROM SORTEIOS WHERE base_id IN (
        SELECT id FROM BASES_DADOS WHERE caminho_dados LIKE '%base-cartelas%' OR caminho_dados = 'data/75x15BINGO.txt'
    )
);

DELETE FROM SORTEIOS WHERE base_id IN (
    SELECT id FROM BASES_DADOS WHERE caminho_dados LIKE '%base-cartelas%' OR caminho_dados = 'data/75x15BINGO.txt'
);

-- 2. Remove referências a bases de teste antigas
DELETE FROM BASES_DADOS 
WHERE caminho_dados LIKE '%base-cartelas%' 
   OR caminho_dados = 'data/75x15BINGO.txt';

-- 3. Garante que não haverá duplicatas antes de inserir as definitivas
DELETE FROM BASES_DADOS WHERE caminho_dados IN (
    'data/30x5BINGO.TXT', 'data/50x5BINGO.TXT', 'data/50x20BINGO.TXT',
    'data/60x15BINGO.TXT', 'data/60x20BINGO.TXT', 'data/75x5BINGO.TXT',
    'data/75x10BINGO.TXT', 'data/75x15BINGO.TXT', 'data/75x25BINGO.TXT',
    'data/90x15BINGO.TXT', 'data/90x25BINGO.TXT'
);

-- 4. Insere as 11 novas bases de dados definitivas com extensão .TXT
INSERT INTO BASES_DADOS (nome, tipo_grade, caminho_dados) VALUES 
('Base 30x5', '30x5', 'data/30x5BINGO.TXT'),
('Base 50x5', '50x5', 'data/50x5BINGO.TXT'),
('Base 50x20', '50x20', 'data/50x20BINGO.TXT'),
('Base 60x15', '60x15', 'data/60x15BINGO.TXT'),
('Base 60x20', '60x20', 'data/60x20BINGO.TXT'),
('Base 75x5', '75x5', 'data/75x5BINGO.TXT'),
('Base 75x10', '75x10', 'data/75x10BINGO.TXT'),
('Base 75x15 BINGO', '75x15', 'data/75x15BINGO.TXT'),
('Base 75x25', '75x25', 'data/75x25BINGO.TXT'),
('Base 90x15', '90x15', 'data/90x15BINGO.TXT'),
('Base 90x25', '90x25', 'data/90x25BINGO.TXT');
