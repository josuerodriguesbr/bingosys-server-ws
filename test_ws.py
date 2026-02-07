import websocket
import json

def test():
    uri = "ws://192.168.1.107:3001"
    try:
        ws = websocket.create_connection(uri)
        print(f"Conectado a {uri}")
        ws.send(json.dumps({"action": "ping"}))
        response = ws.recv()
        print(f"Resposta: {response}")
        ws.close()
    except Exception as e:
        print(f"Erro: {e}")

if __name__ == "__main__":
    test()
