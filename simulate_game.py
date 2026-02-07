import websocket
import json
import random
import time
import sys
import threading

# Config
WS_URL = "ws://192.168.1.107:3001"

def on_message(ws, message):
    data = json.loads(message)
    action = data.get('action') or data.get('type')
    
    # Filter noise
    if action not in ['sync_status', 'number_drawn', 'game_update']:
        print(f"[RECV] {action}: {str(data)[:200]}", flush=True)

    if action == 'login_response':
        if data.get('status') == 'ok':
            print("[PASS] Login OK.", flush=True)
            # Start game 
            ws.send(json.dumps({"action": "start_game"}))
            time.sleep(1)
            
            prize_name = f"Simu {int(time.time())}"
            prize_payload = {
                "action": "add_prize",
                "nome": prize_name,
                "base_id": 10, 
                "configuracoes": {"numChances": 1},
                "ordem": 1,
                "sub_premios": [
                    {"tipo": "quina", "descricao": "Quina Simu", "padrao": []},
                    {"tipo": "cheia", "descricao": "Bingo Simu", "padrao": []}
                ]
            }
            print(f"[SEND] Adding Prize {prize_name}...", flush=True)
            ws.send(json.dumps(prize_payload))

            # Register sales (random 500 tickets)
            print("[SEND] Registering 500 random tickets...", flush=True)
            ws.send(json.dumps({
                "action": "register_random",
                "count": 500
            }))
            
        else:
            print(f"[FAIL] Login failed: {data.get('message')}", flush=True)

    elif action == 'batch_registration_finished':
        print(f"[PASS] Batch registration finished: {data.get('count')} tickets.", flush=True)
        # Start drawing after registration is confirmed
        print("[INFO] Starting draw loop...", flush=True)
        t = threading.Thread(target=run_draw_loop, args=(ws,))
        t.start()
        
    elif action == 'game_update' or action == 'sync_status':
        # Verifica ganhadores no root ou dentro dos prêmios do motor
        winners = data.get('winners')
        if not winners and data.get('engine_prizes_status'):
            for p in data['engine_prizes_status']:
                if p.get('winners'):
                    winners = p['winners']
                    print(f"\n[INFO] Winner found in prize: {p['nome']} ({p['tipo']})", flush=True)
                    break
        
        if winners:
            print("\n[SUCCESS] WINNERS DETECTED!", flush=True)
            print(json.dumps(winners, indent=2), flush=True)
            ws.close()
            sys.exit(0)

def run_draw_loop(ws):
    import random
    print("[INFO] Drawing random numbers from 1 to 75...", flush=True)
    balls = random.sample(range(1, 76), 75)
    for num in balls:
        if not ws.keep_running: break
        ws.send(json.dumps({
            "action": "draw_number",
            "number": num
        }))
        time.sleep(0.02) # Even faster draw
    print("[WARN] Finished drawing loop without closing.", flush=True)

def on_error(ws, error):
    print(f"[ERROR] {error}", flush=True)

def on_close(ws, close_status_code, close_msg):
    print("### Connection Closed ###", flush=True)

def on_open(ws):
    key = sys.argv[1] if len(sys.argv) > 1 else "TEST-KEY-DEFAULT"
    print(f"[SEND] Login with key: {key}", flush=True)
    ws.send(json.dumps({
        "action": "login",
        "chave": key,
        "tipo_usuario": "operador"
    }))

if __name__ == "__main__":
    # websocket.enableTrace(True)
    ws = websocket.WebSocketApp(WS_URL,
                              on_open=on_open,
                              on_message=on_message,
                              on_error=on_error,
                              on_close=on_close)

    ws.run_forever()
