import websocket
import json
import threading
import time

def on_message(ws, message):
    print("Received:", message)

def on_error(ws, error):
    print("Error:", error)

def on_close(ws):
    print("Connection closed")

def on_open(ws):
    def run():
        time.sleep(1)  # Wait for the server to start
        subscribe_msg = {
            "method": "subscribe",
            "params": {
                "symbol": "BTC-PERPETUAL"
            }
        }
        ws.send(json.dumps(subscribe_msg))
        
        # Example: Sending a place order after subscribing
        time.sleep(1)
        place_order_msg = {
            "method": "place",
            "params": {
                "order_id": "ETH-123456",
                "amount": 2,
                "price": 1000
            }
        }
        ws.send(json.dumps(place_order_msg))

    thread = threading.Thread(target=run)
    thread.start()

if __name__ == "__main__":
    websocket_url = "ws://localhost:9002"  # Use the server's IP and port
    ws = websocket.WebSocketApp(websocket_url,
                                on_message=on_message,
                                on_error=on_error,
                                on_close=on_close)
    ws.on_open = on_open
    ws.run_forever()
