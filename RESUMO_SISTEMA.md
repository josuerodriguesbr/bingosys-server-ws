# BingoSys - Sistema de Sorteios em Tempo Real

## 1. Visão Geral
O **BingoSys** é uma plataforma de alta performance desenvolvida para gerenciar, realizar e monitorar sorteios de bingo em tempo real. Diferente de soluções web tradicionais baseadas em *polling*, o BingoSys utiliza uma arquitetura baseada em **WebSockets** com um núcleo de processamento em **C++**, garantindo latência zero e capacidade de processamento massivo de cartelas simultâneas.

## 2. Aspectos Técnicos

### Backend (O "Cérebro")
*   **Linguagem:** C++17 (Alta performance e gerenciamento de memória eficiente).
*   **Framework:** Qt 5.15 (Core, Network, WebSockets, SQL).
*   **Arquitetura:** Event-driven (Orientada a Eventos) via WebSockets. O servidor "empurra" (*push*) atualizações para os clientes instantaneamente, sem necessidade de recarregamento de página.
*   **Banco de Dados:** PostgreSQL (Robustez e integridade relacional).
*   **Hospedagem:** VPS Linux (Ubuntu) com Nginx/Apache como proxy reverso para SSL/WSS.

### Frontend (A Interface)
*   **Tecnologias:** HTML5, CSS3 (Design Responsivo), Vanilla JavaScript (sem frameworks pesados).
*   **Comunicação:** WebSocket nativo para conexão persistente bidirecional.
*   **Design:** Interface moderna com estilo *Glassmorphism* (efeitos de vidro/transparência), animações fluidas e feedback visual instantâneo.

## 3. Aspectos Funcionais

### Sorteio e Operação
*   **Múltiplas Bases de Dados:** Suporte nativo para diferentes tipos de bingo (75x25, 75x15, etc), com ajuste automático da grade visual.
*   **Modos de Sorteio:** Manual (digitação), Aleatório (algoritmo seguro) e Desfazer (correção de erros).
*   **Vendas e Validação:** Módulo de vendas integrado com suporte a leitura de **código de barras** para validação rápida de cartelas físicas.
*   **Gestão de Sessões:** Sistema de chaves de acesso (Tokens) que vinculam operadores e participantes aos seus respectivos sorteios.

### Sistema de Premiações Avançado
*   **Prêmios Flexíveis:** Suporte a **Cartela Cheia**, **Quina** (Horizontal, Vertical, Diagonal) e **Formas Customizadas** (desenhadas livremente num grid interativo).
*   **Detecção Automática:** O motor calcula vencedores em milissegundos a cada bola sorteada.
*   **Visualização Diferenciada:**
    *   🏆 **Laranja:** Ganhadores de Quina.
    *   🏆 **Roxo:** Ganhadores de Formas/Desenhos.
    *   🏆 **Verde:** Ganhadores de Bingo (Cartela Cheia).

### Monitoramento de "Boa" (Near Wins)
*   **Falta 1:** O sistema identifica em tempo real cartelas que faltam apenas **1 número** para ganhar qualquer prêmio ativo (seja Quina, Forma ou Cheia).
*   **Exibição:** Lista dinâmica lateral mostrando as cartelas "armadas" para aumentar a emoção do sorteio.

## 4. Diferenciais Competitivos

1.  **Motor C++ Dedicado:** Enquanto a maioria dos sistemas usa PHP/Node.js para lógica pesada, o BingoSys roda em C++ compilado. Isso permite verificar milhares de cartelas contra múltiplos padrões de vitória (quinas, desenhos) em uma fração de segundo.
2.  **Detecção de Padrões Complexos:** Capacidade única de criar prêmios baseados em desenhos visuais (ex: Letra "H", "X", Cruz) com validação automática.
3.  **Latência Zero:** A bola sorteada aparece na tela de milhares de participantes no exato instante em que o operador confirma, sincronizando animações e sons.
4.  **Resiliência:** Sincronização automática de estado. Se o operador atualizar a página ou cair a internet, ao voltar, o sistema restaura exatamente o estado atual (bolas, ganhadores) do servidor.

## 5. Links de Acesso

As interfaces do sistema estão disponíveis através dos links abaixo (servidor de produção):

*   **Painel do Operador (Admin):**
    [https://sorteio.vendasys.com.br/frontend/panel.html](https://sorteio.vendasys.com.br/frontend/panel.html)
    *(Requer chave de acesso de operador)*

*   **Terminal de Vendas:**
    [https://sorteio.vendasys.com.br/frontend/vendas.html](https://sorteio.vendasys.com.br/frontend/vendas.html)
    *(Para registro e validação de cartelas)*

*   **Página de Login (Convite):**
    [https://sorteio.vendasys.com.br/frontend/login.html](https://sorteio.vendasys.com.br/frontend/login.html)
    *(Entrada para participantes visualizarem suas cartelas digitalmente)*

---
*Gerado em: 01 de Fevereiro de 2026*
