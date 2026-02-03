class BingoSocket {
    constructor(url) {
        this.url = url;
        this.socket = null;
        this.callbacks = {};
        this.isConnected = false;
        this.connect();
    }

    connect() {
        console.log(`Connecting to ${this.url}...`);
        this.socket = new WebSocket(this.url);

        this.socket.onopen = () => {
            console.log('WebSocket Connected');
            this.isConnected = true;
            this.startHeartbeat();
            this.emit('connected');
        };

        this.socket.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                if (data.action === 'pong') return; // Ignore heatbeat response
                this.handleMessage(data);
            } catch (e) {
                console.error('Error parsing message:', e);
            }
        };

        this.socket.onclose = () => {
            console.log('WebSocket Disconnected');
            this.isConnected = false;
            this.stopHeartbeat();
            this.emit('disconnected');
            // Tenta reconectar em 3s
            setTimeout(() => this.connect(), 3000);
        };

        this.socket.onerror = (err) => {
            console.error('WebSocket Error:', err);
        };
    }

    startHeartbeat() {
        this.heartbeatInterval = setInterval(() => {
            if (this.isConnected) {
                this.send('ping', {});
            }
        }, 30000); // 30 segundos
    }

    stopHeartbeat() {
        if (this.heartbeatInterval) {
            clearInterval(this.heartbeatInterval);
            this.heartbeatInterval = null;
        }
    }

    send(action, payload = {}) {
        if (!this.isConnected) return;
        const msg = JSON.stringify({ action, ...payload });
        this.socket.send(msg);
    }

    on(event, callback) {
        if (!this.callbacks[event]) {
            this.callbacks[event] = [];
        }
        this.callbacks[event].push(callback);

        // Se o evento for 'connected' e já estivermos conectados, executa o callback imediatamente
        if (event === 'connected' && this.isConnected) {
            callback();
        }
    }

    emit(event, data) {
        if (this.callbacks[event]) {
            this.callbacks[event].forEach(cb => {
                try {
                    cb(data);
                } catch (e) {
                    console.error(`Error in callback for event ${event}:`, e);
                }
            });
        }
    }

    handleMessage(data) {
        console.log('Server:', data);
        if (data.action === 'login_response') {
            if (data.status === 'ok') {
                this.emit('login_success', data);
            } else {
                this.emit('login_error', data);
            }
        }

        if (data.action) {
            this.emit(data.action, data);
        }
    }

    login(chave) {
        this.send('login', { chave });
    }
}

// Global instance (can be used by other scripts)
// const socket = new BingoSocket('ws://' + window.location.hostname + ':3000'); 
// Assuming localhost for now or dynamic
const getServerUrl = () => {
    // Se estiver rodando local file, assume o IP do notebook (servidor local temporário)
    if (window.location.protocol === 'file:') return 'ws://192.168.1.107:3000';

    const protocol = window.location.protocol === 'https:' ? 'wss://' : 'ws://';
    const host = window.location.host;

    // Se estiver em produção (vendasys.com.br), usa a subpasta configurada no proxy do Apache
    if (host.includes('vendasys.com.br')) {
        return protocol + host + '/sorteio/ws';
    }

    // Fallback para desenvolvimento (usando IP ou localhost na porta 3000)
    return protocol + window.location.hostname + ':3000';
};

const bingoSocket = new BingoSocket(getServerUrl());
