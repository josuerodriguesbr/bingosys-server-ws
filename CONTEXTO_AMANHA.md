# Contexto para a Próxima Sessão (18/01/2026)

Este arquivo registra o estado atual após a grande migração e melhorias de performance.

## Status Atual
O sistema está 100% funcional na nova estrutura de sub-pasta.

### Funcionalidades em Produção:
1.  **Nova URL**: [vendasys.com.br/sorteio/](https://vendasys.com.br/sorteio/)
2.  **Performance**: Motor de jogo otimizado para lidar com milhões de cartelas focando apenas nas vendidas.
3.  **Segurança**: Registro de vendas validado via Dígito Verificador (DV).
4.  **Logs**: Registro de vendas agora inclui timestamps (`ID,TIMESTAMP`).
5.  **Estabilidade**: Auto-inicialização do motor de jogo adicionada no servidor para garantir que o sorteio esteja sempre pronto.

### Onde Paramos:
- Sincronização entre Local, GitHub e VPS concluída.
- Deploy realizado na pasta `/var/www/bingosys-server-ws`.
- Apache configurado com Alias `/sorteio`.

### Próximos Passos Sugeridos:
- [ ] Implementar um sistema de backup automático para o arquivo `sold-tickets.TXT`.
- [ ] Adicionar suporte a múltiplos sorteios simultâneos ou "Etapas".
- [ ] Melhorar a interface responsiva para dispositivos móveis ainda mais.

6.  **Modos Amigáveis**: O operador agora escolhe modos como "Bingo 75 x 15 (Clássico)" via interface, sem precisar lidar com índices técnicos.
7.  **Sincronização Total**: A lista de modos é carregada de `modes.json` e sincronizada automaticamente entre Painel Admin e Tela de Vendas.
8.  **Sistema de Chances**: Mantido o registro automático em lote (ex: bipar ID 10 registra 10, 11, 12 se chances=3).
9.  **Persistência**: Todas as preferências são salvas em `config.json` e os modos em `modes.json`.
