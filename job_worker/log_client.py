import asyncio
import json
import websockets
import threading
import logging

log_logger = logging.getLogger("log")

log_thread = None
log_loop = asyncio.new_event_loop()
asyncio.set_event_loop(log_loop)
log_queue = asyncio.Queue()
asyncio.set_event_loop(None)

async def log_thread_impl(uri):
    msg = None
    while True:
        try:
            async with websockets.connect(uri) as websocket:
                while True:
                    if msg is None:
                        msg = await log_queue.get()
                        if msg is None:
                            return
                    log, date, level, message = msg
                    request = {
                        "type": "log",
                        "log": log,
                        "date": date,
                        "level": level,
                        "message": message
                    }
                    await websocket.send(json.dumps(request))
                    msg = None

        except:
            log_logger.exception("Error occured in log upload thread")
            pass
        await asyncio.sleep(1)

def log_post(log, date, level, message):
    log_queue.put_nowait((log, date, level, message))

def log_thread_wrapper(uri):
    log_loop.run_until_complete(log_thread_impl(uri))

class MyLogHandler(logging.Handler):
    def emit(self, record):
        log_loop.call_soon_threadsafe(log_post, record.name, record.created, record.levelname, record.getMessage())


def start_log_client(uri):
    global log_thread
    log_thread = threading.Thread(target=log_thread_wrapper, args=(uri,))
    log_thread.start()

def stop_log_client():
    log_loop.call_soon_threadsafe(lambda: log_queue.put_nowait(None))

