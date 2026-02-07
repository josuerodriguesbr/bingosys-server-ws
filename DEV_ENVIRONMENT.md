# Ambiente de Desenvolvimento e Workflow

Este documento detalha as configurações do servidor remoto, credenciais e o processo de desenvolvimento utilizado no projeto BingoSys.

## 1. Servidor Remoto (Notebook Linux)

O projeto é desenvolvido localmente no Windows (`d:\PROJETOS\bingosys-server-ws`), mas executado e testado em um servidor Linux remoto (Notebook).

*   **IP**: `192.168.1.107`
*   **Usuário**: `josuerodrigues`
*   **Pasta do Projeto**: `~/bingosys`
*   **Pasta Binários**: `~/bingosys/bin`

### Credenciais
*   **Senha (Sudo/SSH)**: `lenovo112423`
    *   *Nota: O acesso SSH está configurado via chave pública/privada para automação, mas esta senha é utilizada para comandos `sudo` no servidor.*

## 2. Banco de Dados (PostgreSQL)

O banco de dados roda no servidor remoto.

*   **Host**: `localhost` (no servidor remoto)
*   **Porta**: `5432`
*   **Database**: `bingosys`
*   **Usuário**: `bingosys` (ou `postgres` para tarefas administrativas)
*   **Senha DB**: `bingosys` (para usuário da aplicação) / `lenovo112423` (para `sudo -u postgres`)

## 3. Workflow de Desenvolvimento

O ciclo de desenvolvimento segue os seguintes passos para garantir a integridade do sistema entre o ambiente Windows (Edição) e Linux (Execução):

1.  **Edição Local**:
    *   O código fonte (C++, HTML, JS, Scripts) é editado na máquina Windows.

2.  **Sincronização (Deploy)**:
    *   Os arquivos modificados são transferidos para o servidor Linux via SSH (geralmente usando `scp` ou pipes `cat` sobre SSH).
    *   Arquivos principais transferidos: `src/*.cpp`, `src/*.h`, `frontend/*.html`, scripts `.sh` e `.py`.

3.  **Compilação Remota**:
    *   O servidor é reconfigurado e recompilado no Linux usando `qmake` e `make`.
    *   Comando típico: `cd ~/bingosys && make clean && make -j4`

4.  **Reinicialização do Servidor**:
    *   O processo antigo do servidor (`BingoSysServer`) é finalizado (`pkill`).
    *   A nova versão compilada é iniciada em background (`nohup ./bin/BingoSysServer > server.log 2>&1 &`).

5.  **Testes Automatizados**:
    *   Scripts de teste em Python (ex: `verify_prizes.py`) são executados no servidor remoto.
    *   Esses scripts se conectam via WebSocket (`ws://localhost:3001`) ao servidor recém-iniciado para validar as alterações.
    *   **Importante**: Isso garante que o código editado no Windows realmente funciona no ambiente Linux de produção antes de considerar a tarefa concluída.

## 4. Comandos Úteis

### Conectar via SSH
```bash
ssh josuerodrigues@192.168.1.107
```

### Reiniciar Servidor Manualmente
```bash
ssh josuerodrigues@192.168.1.107 "pkill BingoSysServer ; cd ~/bingosys && nohup ./bin/BingoSysServer 3001 > server.log 2>&1 &"
```

### Ver Logs em Tempo Real
```bash
ssh josuerodrigues@192.168.1.107 "tail -f ~/bingosys/server.log"
```
