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
            this.emit('connected');
        };

        this.socket.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                this.handleMessage(data);
            } catch (e) {
                console.warn('Received non-JSON message:', event.data);
            }
        };

        this.socket.onclose = () => {
            console.log('WebSocket Disconnected. Reconnecting in 3s...');
            this.isConnected = false;
            this.emit('disconnected');
            setTimeout(() => this.connect(), 3000);
        };

        this.socket.onerror = (err) => {
            console.error('WebSocket Error:', err);
        };
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
    }

    emit(event, data) {
        if (this.callbacks[event]) {
            this.callbacks[event].forEach(cb => cb(data));
        }
    }

    handleMessage(data) {
        console.log('Server:', data);
        if (data.action) {
            this.emit(data.action, data);
        }
    }
}

// Global instance (can be used by other scripts)
// const socket = new BingoSocket('ws://' + window.location.hostname + ':3000'); 
// Assuming localhost for now or dynamic
const getServerUrl = () => {
    // Se estiver rodando local file, assume localhost:3000
    if (window.location.protocol === 'file:') return 'ws://localhost:3000';
    return 'ws://' + window.location.hostname + ':3000';
};

const bingoSocket = new BingoSocket(getServerUrl());
