const BingoAuth = {
    saveSession(chave, isOperator, sorteioId) {
        localStorage.setItem('bingo_chave', chave);
        localStorage.setItem('bingo_is_operator', isOperator);
        localStorage.setItem('bingo_sorteio_id', sorteioId);
    },

    getSession() {
        return {
            chave: localStorage.getItem('bingo_chave'),
            isOperator: localStorage.getItem('bingo_is_operator') === 'true',
            sorteioId: localStorage.getItem('bingo_sorteio_id')
        };
    },

    logout() {
        localStorage.clear();
        window.location.href = 'login.html';
    },

    checkAccess(requiredOperator = false) {
        const session = this.getSession();
        if (!session.chave) {
            window.location.href = 'login.html';
            return false;
        }
        if (requiredOperator && !session.isOperator) {
            window.location.href = 'index.html';
            return false;
        }
        return true;
    }
};

// Auto-login logic if key exists
bingoSocket.on('connected', () => {
    const session = BingoAuth.getSession();
    if (session.chave) {
        bingoSocket.login(session.chave);
    }
});

bingoSocket.on('login_error', () => {
    BingoAuth.logout();
});
