import websocket
import json
import time
import sys

WS_URL = "ws://192.168.1.107:3000"
ADMIN_USER = "admin"
ADMIN_PASS = "Bingo2026!@#"

def on_message(ws, message):
    data = json.loads(message)
    # print(f"RCV: {data.get('action')}")
    ws.last_message = data
    
    if data.get('action') == 'login_response':
        ws.login_response = data
        
    if data.get('action') == 'sync_status':
        ws.game_status = data
        
    if data.get('action') == 'chave_criada_response':
        ws.created_key = data

def on_error(ws, error):
    print(f"Error: {error}")

def on_close(ws, close_status_code, close_msg):
    print("Closed")

def on_open(ws):
    print("Connected")
    # 1. Login Admin
    ws.send(json.dumps({"action": "login_admin", "usuario": ADMIN_USER, "senha": ADMIN_PASS}))

def wait_for_attr(ws, attr, timeout=5):
    start = time.time()
    while not hasattr(ws, attr):
        if time.time() - start > timeout:
            return None
        time.sleep(0.1)
    ret = getattr(ws, attr)
    delattr(ws, attr)
    return ret

def wait_for_condition(ws, condition_func, timeout=10):
    start = time.time()
    while not condition_func(ws):
        if time.time() - start > timeout:
            return False
        time.sleep(0.1)
    return True

# --- STEPS ---

def run_test():
    ws = websocket.WebSocket()
    ws.connect(WS_URL)
    
    # 1. Login Admin
    print("1. Logging in as Admin...")
    ws.send(json.dumps({"action": "login_admin", "usuario": ADMIN_USER, "senha": ADMIN_PASS}))
    resp = json.loads(ws.recv()) # login_response
    if resp.get('status') != 'ok':
        print("FATAL: Admin login failed")
        return
    print("   Admin Logged In")

    # 2. Create Sorteio
    print("2. Creating Test Sorteio...")
    # Get models/bases first to find IDs? Assuming 1/1 exists or using defaults.
    # Let's try to get data first
    ws.send(json.dumps({"action": "get_admin_data"}))
    msg = json.loads(ws.recv()) # admin_data_response
    
    modelo_id = msg['modelos'][0]['id']
    base_id = msg['bases'][0]['id']
    
    ws.send(json.dumps({
        "action": "criar_chave",
        "modelo_id": modelo_id,
        "base_id": base_id,
        "chave": "TEST-UNDO-" + str(int(time.time()))
    }))
    key_resp = json.loads(ws.recv())
    if key_resp.get('status') != 'ok':
        print("FATAL: Failed to create key")
        return
    
    TEST_KEY = key_resp['chave']
    SORTEIO_ID = key_resp['sorteio_id']
    print(f"   Created Sorteio {SORTEIO_ID} with Key {TEST_KEY}")
    ws.close()

    # 3. Login as Operator
    print("3. Logging in as Operator...")
    ws = websocket.WebSocket()
    ws.connect(WS_URL)
    ws.send(json.dumps({"action": "login", "chave": TEST_KEY}))
    # Loop to find login_response, ignoring others temporarily
    login_success = False
    start_wait = time.time()
    while time.time() - start_wait < 5:
        msg = json.loads(ws.recv())
        if msg.get('action') == 'login_response':
            login_resp = msg
            if login_resp.get('status') == 'ok':
                login_success = True
            else:
                print(f"FATAL: Operator login failed. Response: {login_resp}")
                return
            break
        elif msg.get('action') == 'sync_status': 
            # Consumed early, that's fine
            pass

    if not login_success:
        print("FATAL: Timeout waiting for login_response")
        return
    
    # Consume sync_status
    ws.recv() 
    print("   Operator Logged In. Starting Game...")

    # 4. Start Game (Reset)
    ws.send(json.dumps({"action": "start_game"}))
    # Consume game_started and sync_status
    while True:
        m = json.loads(ws.recv())
        if m.get('action') == 'sync_status': 
            break
            
    # 5. Draw until Win
    print("5. Drawing numbers until a Prize is won...")
    won_prize = False
    last_number = 0
    
    # Limit max draws to avoid infinite loop
    for i in range(75):
        # Pick a random number? No, the server draws what we send?
        # WAIT, 'draw_number' expects 'number'. The server doesn't pick?
        # The operator picks. We need to pick valid numbers.
        # Simple strategy: Just draw 1..75 sequentially. Eventually someone wins.
        num_to_draw = i + 1
        ws.send(json.dumps({"action": "draw_number", "number": num_to_draw}))
        
        # Read responses (number_drawn, potentially sync_status if won)
        # We might get multiple messages
        while True:
            raw = ws.recv()
            msg = json.loads(raw)
            if msg.get('action') == 'number_drawn':
                last_number = msg['number']
                # Check for winners inside number_drawn (it contains 'prizes' list in 'status' usually? 
                # No, number_drawn implies a broadcast. Let's check the structure.)
                # Actually, the server broadcastToGame sends getGameStatusJson() with action injected.
                # So msg HAS 'prizes'.
                
                prizes = msg.get('prizes', [])
                for p in prizes:
                    if p.get('realizada') or len(p.get('winners', [])) > 0:
                        print(f"   WIN DETECTED! Prize {p['id']} ({p['nome']}) won by {len(p.get('winners', []))} tickets.")
                        won_prize = True
                        break
                break # Exit recv loop, proceed to next draw or stop
            elif msg.get('action') == 'draw_number_error':
                print(f"   Draw Error: {msg.get('message')}")
                break

        if won_prize:
            break
            
    if not won_prize:
        print("FATAL: Drew 75 numbers and no one won? Something is wrong.")
        return

    print("6. Executing UNDO...")
    time.sleep(1)
    ws.send(json.dumps({"action": "undo_last"}))
    
    # 7. Verify Undo
    print("7. Verifying Undo Result...")
    undo_confirmed = False
    prizes_reset = False
    
    while True:
        try:
            raw = ws.recv()
            msg = json.loads(raw)
            
            if msg.get('action') == 'number_cancelled':
                print(f"   Undo Confirmed: Number {msg.get('number')} cancelled.")
                if msg.get('reabertos', 0) > 0:
                    print(f"   SUCCESS: {msg.get('reabertos')} prizes were reopened via logic!")
                    
            if msg.get('action') == 'sync_status':
                # Check if prizes are pending
                prizes = msg.get('prizes', [])
                all_pending = True
                for p in prizes:
                    if p.get('realizada'):
                        all_pending = False
                        print(f"   FAILURE: Prize {p['id']} is still REALIZADA.")
                    if len(p.get('winners', [])) > 0:
                        all_pending = False
                        print(f"   FAILURE: Prize {p['id']} still has winners.")
                
                if all_pending:
                    print("   SUCCESS: All prizes are now PENDING/Unwon.")
                    prizes_reset = True
                    break
        except:
             break

    if prizes_reset:
        print("\n=== TEST PASSED: Undo successfully reverted game state ===\n")
    else:
        print("\n=== TEST FAILED: Prizes did not reset correctly ===\n")

if __name__ == "__main__":
    run_test()
