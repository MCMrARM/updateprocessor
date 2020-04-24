import asyncio
import websockets
import json
import os
from pathlib import Path

def normalize_log_name(name):
    return "".join(x for x in name if x.isalnum() or x in " -_.")

class Log:
    def __init__(self, name):
        self.name = name
        path = os.path.join("priv", "logs", name + ".txt")
        if os.path.exists(path):
            self.file = open(path, "r+")
        else:
            self.file = open(path, "w+")
        self.file.seek(0, os.SEEK_END)
        self.subscribers = []

    def fetch_all(self):
        self.file.seek(0, os.SEEK_SET)
        ret = self.file.read()
        self.file.seek(0, os.SEEK_END)
        return ret

    def log(self, date, level, message):
        self.file.write(json.dumps({
            "date": date,
            "level": level,
            "message": message
        }) + "\n")
        for q in self.subscribers:
            q.put_nowait((date, level, message))

logs = {}
log_order = []
log_list_subscribers = []

def log_print(log, date, level, message):
    log = normalize_log_name(log)
    if log not in logs:
        logs[log] = Log(log)
        log_order.append(log)
        for s in log_list_subscribers:
            if s.empty():
                s.put_nowait(())
    logs[log].log(date, level, message)

async def log_list_forwarder(websocket, queue):
    while True:
        reply = {
            "type": "log_list_update",
            "logs": log_order
        }
        await websocket.send(json.dumps(reply))
        () = await queue.get()

async def log_forwarder(websocket, queue, log):
    while True:
        (date, level, message) = await queue.get()
        reply = {
            "type": "log",
            "date": date,
            "level": level,
            "log": log,
            "message": message
        }
        await websocket.send(json.dumps(reply))

async def log_client(websocket, path):
    log_list_subscription_queue = None
    log_list_subscription = None
    subscribed_to = {}
    try:
        while True:
            msg = json.loads(await websocket.recv())
            if msg["type"] == "subscribe_log_list" and log_list_subscription is None:
                log_list_subscription_queue = asyncio.Queue()
                log_list_subscription = asyncio.ensure_future(log_list_forwarder(websocket, log_list_subscription_queue))
                log_list_subscribers.append(log_list_subscription_queue)
            if msg["type"] == "subscribe" and msg["log"] in logs:
                queue = asyncio.Queue()
                forwarder = asyncio.ensure_future(log_forwarder(websocket, queue, msg["log"]))
                subscribed_to[msg["log"]] = (forwarder, queue)
                logs[msg["log"]].subscribers.append(queue)
            if msg["type"] == "unsubscribe" and msg["log"] in subscribed_to:
                forwarder, queue = subscribed_to[msg["log"]]
                del subscribed_to[msg["log"]]
                logs[msg["log"]].subscribers.remove(queue)
                forwarder.cancel()
            if msg["type"] == "log":
                log_print(msg["log"], msg["date"], msg["level"], msg["message"])
            if msg["type"] == "get_log":
                entries = None
                if msg["log"] in logs:
                    entries = logs[msg["log"]].fetch_all()
                reply = {
                    "type": "log_archive",
                    "log": msg["log"],
                    "data": entries
                }
                await websocket.send(json.dumps(reply))
                entries = None
                reply = None
    finally:
        for l in subscribed_to:
            forwarder, queue = subscribed_to[l]
            logs[l].subscribers.remove(queue)
            forwarder.cancel()
        if log_list_subscription is not None:
            log_list_subscribers.remove(log_list_subscription_queue)
            log_list_subscription.cancel()


if not os.path.exists(os.path.join("priv", "logs")):
    os.mkdir(os.path.join("priv", "logs"))

for e in sorted(Path(os.path.join("priv", "logs")).iterdir(), key=os.path.getctime):
    if not e.name.endswith(".txt"):
        continue
    print(e.name)
    logs[e.name[:-4]] = Log(e.name[:-4])
    log_order.append(e.name[:-4])


start_server = websockets.serve(log_client, "0.0.0.0", 9453)
asyncio.get_event_loop().run_until_complete(start_server)
asyncio.get_event_loop().run_forever()