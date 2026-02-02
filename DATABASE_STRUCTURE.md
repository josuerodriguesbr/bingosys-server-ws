# Estrutura do Banco de Dados (Mermaid)

Aqui está a representação visual das tabelas e relacionamentos do banco de dados BingoSys:

```mermaid
erDiagram
    SORTEIOS {
        integer id PK
        varchar status
        integer modelo_id FK
        integer base_id FK
        jsonb preferencias
        timestamp criado_em
        timestamp finalizado_em
    }

    BASES_DADOS {
        integer id PK
        varchar nome
        varchar tipo_grade
        varchar caminho_dados
    }

    MODELOS_SORTEIO {
        integer id PK
        varchar nome
        jsonb config_padrao
    }

    BOLAS_SORTEADAS {
        integer id PK
        integer sorteio_id FK
        integer numero
        timestamp momento
    }

    CARTELAS_VALIDADAS {
        integer id PK
        integer sorteio_id FK
        integer numero_cartela
        varchar telefone_participante
        varchar origem
    }

    CHAVES_ACESSO {
        integer id PK
        varchar codigo_chave
        varchar status
        integer sorteio_id FK
        timestamp criado_em
        timestamp usado_em
    }

    PREMIACOES {
        integer id PK
        integer sorteio_id FK
        varchar nome_premio
        varchar tipo
        jsonb padrao_grade
        integer ordem_exibicao
    }

    BASES_DADOS ||--o{ SORTEIOS : "usada em"
    MODELOS_SORTEIO ||--o{ SORTEIOS : "define"
    SORTEIOS ||--o{ BOLAS_SORTEADAS : "possui"
    SORTEIOS ||--o{ CARTELAS_VALIDADAS : "contem"
    SORTEIOS ||--o{ CHAVES_ACESSO : "autoriza"
    SORTEIOS ||--o{ PREMIACOES : "premia"
```

## Descrição das Tabelas

- **SORTEIOS**: Tabela central que gerencia cada partida ou evento de bingo.
- **BASES_DADOS**: Armazena as configurações das bases de cartelas (caminho dos arquivos TXT e tipo de grade).
- **MODELOS_SORTEIO**: Modelos pré-configurados para novos sorteios.
- **BOLAS_SORTEADAS**: Histórico das bolas chamadas em cada sorteio específico.
- **CARTELAS_VALIDADAS**: Registro das cartelas vendidas/validadas para um sorteio.
- **CHAVES_ACESSO**: Chaves (Tokens) para login de operadores e participantes.
- **PREMIACOES**: Definição dos prêmios (Quinas, Formas, Bingo) vinculados a cada sorteio.
