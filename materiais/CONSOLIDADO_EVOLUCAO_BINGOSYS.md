# BingoSys - Histórico Consolidado de Evolução do Projeto

Este documento reúne todas as melhorias técnicas, reestruturações e implementações realizadas no sistema BingoSys, servindo como um registro histórico de evolução e guia de referência para a arquitetura atual.

---

## 1. Evolução da Arquitetura e Banco de Dados

### 1.1. Reestruturação Semântica (Rodadas e Prêmios)
- **Mudança Conceitual**: O sistema abandonou a nomenclatura genérica de "Premiações" para adotar o conceito de **Rodadas**.
- **Hierarquia 1:N**: 
  - **Rodadas**: Grupos de sorteio que podem ser vinculados a bases de cartelas específicas (ex: Rodada 1 - Base 75x15).
  - **Prêmios**: Regras de vitória individuais dentro de uma rodada (ex: Quina, Forma, Bingo).
- **Impacto no DB**: 
  - Tabela `PREMIACOES` renomeada para `RODADAS`.
  - Tabela `SUB_PREMIOS` renomeada para `PREMIOS`.
  - Scripts de migração criados para garantir a integridade dos dados existentes.

### 1.2. Agendamento e Múltiplas Bases
- **Flexibilidade**: Suporte para que cada rodada de um mesmo sorteio utilize uma base de cartelas diferente (ex: 75x15 e 75x25 na mesma sessão).
- **Agendamento**: Inclusão de campos de data e hora de início nos sorteios para melhor organização operacional.

---

## 2. Aprimoramentos do Motor de Jogo (C++)

### 2.1. Paralelismo de Premiações
- **Vitórias Simultâneas**: Prêmios do tipo `forma` agora participam de todas as bolas, independentemente de haver prêmios sequenciais (Quina/Bingo) pendentes. Isso aumenta a dinâmica e a emoção do jogo.
- **Normalização**: Implementada normalização de strings para evitar que erros de digitação (Caixa Alta/Baixa) afetassem a lógica de paralelismo.

### 2.2. Lógica de Reversão (Undo) Definitiva
- **Reset & Replay**: Ao desfazer uma bola, o motor realiza um "Zero Reset" e reprocessa todas as bolas anteriores em milissegundos. Isso garante:
  - Limpeza total de ganhadores antigos.
  - Recálculo preciso do flag de "Sorteio Concluído".
  - Reabertura automática de prêmios no banco de dados caso a bola desfeita retire o único ganhador.

---

## 3. Evolução da Interface do Usuário (UI/UX)

### 3.1. Visual Premium e Glassmorphism
- **Design Moderno**: Interface baseada em transparências, sombras suaves e micro-animações fluidas.
- **Badges Informativos**: Visores de "Tipo de Sorteio" e "Contagem de Bolas" organizados em badges elegantes nos cantos superiores, liberando o centro da tela para a bola principal.

### 3.2. Painel de Ganhadores Avançado
- **Grid Responsivo**: A lista de ganhadores se adapta automaticamente (vertical em telas pequenas, grid horizontal em telas grandes/4K).
- **Destaque Visual**:
  - 🟠 **Laranja**: Quina.
  - 🟣 **Roxo**: Formas/Desenhos.
  - 🟢 **Verde Brilhante**: Cartela Cheia (Bingo) com destaque em todas as células marcadas.

### 3.3. Editor de Formas Interativo
- **Desenho Livre**: Grid de 5x5 interativo para criação de padrões customizados (H, X, Cruz, etc).
- **Automação**: Reset visual automático da grade ao salvar um prêmio, garantindo que o operador sempre inicie um novo desenho do zero.

---

## 4. Segurança e Integridade Operacional

### 4.1. Blindagem de Comandos
- **Anti-Double-Click**: Implementação do flag `isDrawing` que trava todos os controles durante o processamento de uma bola ou comunicação com o servidor.
- **Prevenção de Números Fantasmas**: Bloqueio total de entrada manual após o sorteio ser marcado como concluído.

### 4.2. Gestão de Chaves de Acesso
- **Inativação Automática**: Chaves de acesso de sorteios concluídos são inativadas no banco de dados para evitar reinícios não autorizados.
- **Reativação via Undo**: O sistema é inteligente o suficiente para reativar uma chave caso o operador utilize o "Desfazer" para reabrir uma rodada finalizada por engano.

---

## 5. Histórico de Portas e Conexão
- **Estabilidade**: O servidor foi padronizado para operar na porta **3001** (WebSocket), garantindo coexistência harmônica com servidores Web tradicionais (Nginx/Apache) na porta 80/443.

---
*Compilado em: 07 de Fevereiro de 2026*
*Documento destinado ao arquivamento técnico na pasta /materiais do projeto.*
