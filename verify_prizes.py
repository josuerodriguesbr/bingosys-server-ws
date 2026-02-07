import websocket
import json
import time
import sys
import argparse

# Configuration
WS_URL = "ws://localhost:3001"

def on_message(ws, message):
    data = json.loads(message)
    print(f"Recv: {data.get('action')}")
    # print(json.dumps(data, indent=2))
    
    if data.get('action') == 'login_response':
        if data.get('status') == 'ok':
            print("[Pass] Autenticado como operador.")
            
            # Create a complex prize
            prize_payload = {
                "action": "add_prize",
                "nome": "Premio Teste Hierarquico Script",
                "base_id": 3, 
                "configuracoes": {"numChances": 1},
                "ordem": 1,
                "sub_premios": [
                    {"tipo": "quina", "descricao": "Quina Teste", "padrao": []}, 
                    {"tipo": "cheia", "descricao": "Cheia Teste", "padrao": []}
                ]
            }
            print(f"[Send] Adicionando premio: {json.dumps(prize_payload)}")
            ws.send(json.dumps(prize_payload))
        else:
             print(f"[FAIL] Falha no login: {data.get('message')}")
             ws.close()
             sys.exit(1)

    elif data.get('action') == 'prize_added':
        print("[Pass] Premio adicionado com sucesso.")
        # Request updated prize list
        ws.send(json.dumps({"action": "get_prizes"}))

    elif data.get('action') == 'prizes_list':
        prizes = data.get('prizes', [])
        found = False
        for p in prizes:
            if p.get('nome') == "Premio Teste Hierarquico Script":
                found = True
                print(f"[Check] Premio encontrado: {p['nome']}")
                stored_subs = p.get('sub_premios', [])
                print(f"[Check] Sub-premios: {len(stored_subs)}")
                
                types = [s.get("tipo") for s in stored_subs]
                descs = [s.get("descricao") for s in stored_subs]
                
                if "quina" in types and "cheia" in types:
                    # Debug detailed values
                    for i, s in enumerate(stored_subs):
                        if "descricao" not in s:
                            print(f"[DEBUG] SubPremios[{i}] does NOT have 'descricao' key.")
                        else:
                            print(f"[DEBUG] SubPremios[{i}] 'descricao' value: {s['descricao']}")
                            
                    if "Quina Teste" in descs and "Cheia Teste" in descs:
                        print("[PASS] Sub-premios e descrições verificados corretamente.")
                        ws.close()
                        sys.exit(0)
                    else:
                        print(f"[FAIL] Descrições incorretas: {descs}")
                        ws.close()
                        sys.exit(1)
                else:
                    print(f"[FAIL] Tipos de sub-premios incorretos: {types}")
                    ws.close()
                    sys.exit(1)
        
        if not found:
            print("[FAIL] Premio recem criado nao encontrado na lista.")
            ws.close()
            sys.exit(1)

    elif data.get('action') == 'add_prize_error':
        print(f"[FAIL] Erro ao adicionar premio: {data.get('message')}")
        ws.close()
        sys.exit(1)
    
    elif data.get('action') == 'login_error':
        print(f"[FAIL] Erro de login: {data.get('message')}")
        ws.close()
        sys.exit(1)

def on_error(ws, error):
    print(f"[Error] {error}")

def on_close(ws, close_status_code, close_msg):
    print("### Connection Closed ###")

def on_open(ws):
    print("### Connection Opened ###")
    # Login
    auth_payload = {
        "action": "login",
        "chave": args.key
    }
    print(f"[Send] Login com chave: {args.key}")
    ws.send(json.dumps(auth_payload))

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("key", help="Operator Access Key")
    args = parser.parse_args()

    websocket.enableTrace(False)
    ws = websocket.WebSocketApp(WS_URL,
                              on_open=on_open,
                              on_message=on_message,
                              on_error=on_error,
                              on_close=on_close)
    ws.run_forever()
