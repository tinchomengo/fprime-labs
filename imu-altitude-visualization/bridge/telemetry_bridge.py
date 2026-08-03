#!/usr/bin/env python3
"""Bridges F' telemetry (Roll, Pitch, Yaw, Altitude from ImuSim) to a WebSocket for the three.js frontend."""

import argparse
import asyncio
import json

import websockets

from fprime_gds.common.handlers import DataHandler
from fprime_gds.common.models.dictionaries import Dictionaries
from fprime_gds.common.pipeline.standard import StandardPipeline
from fprime_gds.common.utils.config_manager import ConfigManager

CHANNEL_KEYS = {
    "TelemetryDeployment.imuSim.Roll": "roll",
    "TelemetryDeployment.imuSim.Pitch": "pitch",
    "TelemetryDeployment.imuSim.Yaw": "yaw",
    "TelemetryDeployment.imuSim.Altitude": "altitude",
}


class TelemetryBridge(DataHandler):
    """Forwards decoded F' channel updates to all connected WebSocket clients."""

    def __init__(self, loop, clients):
        self.loop = loop
        self.clients = clients

    def data_callback(self, data, sender=None):
        key = CHANNEL_KEYS.get(data.template.get_full_name())
        if key is None:
            return
        message = json.dumps({"channel": key, "value": data.get_val()})
        asyncio.run_coroutine_threadsafe(self._broadcast(message), self.loop)

    async def _broadcast(self, message):
        if not self.clients:
            return
        await asyncio.gather(
            *(client.send(message) for client in self.clients),
            return_exceptions=True,
        )


async def handle_client(websocket, clients):
    clients.add(websocket)
    print(f"three.js client connected ({len(clients)} total)")
    try:
        await websocket.wait_closed()
    finally:
        clients.discard(websocket)
        print(f"three.js client disconnected ({len(clients)} total)")


async def main(args):
    loop = asyncio.get_running_loop()
    clients = set()

    dictionaries = Dictionaries.load_dictionaries_into_config(args.dictionary)
    pipeline = StandardPipeline()
    pipeline.setup(
        config=ConfigManager.get_instance(),
        dictionaries=dictionaries,
        file_store=args.file_store,
        logging_prefix=None,
        data_logging_enabled=False,
    )
    pipeline.connect(f"{args.fprime_host}:{args.fprime_port}")
    print(f"three.js client connected ({len(clients)} total)")

    bridge = TelemetryBridge(loop, clients)
    pipeline.coders.register_channel_consumer(bridge)

    async with websockets.serve(lambda ws: handle_client(ws, clients), args.ws_host, args.ws_port):
        print(f"[bridge] WebSocket server listening on ws://{args.ws_host}:{args.ws_port}")
        await asyncio.Future()  # run forever


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dictionary", required=True, help="Path to the deployment's dictionary JSON file")
    parser.add_argument("--fprime-host", default="127.0.0.1")
    parser.add_argument("--fprime-port", type=int, default=50050)
    parser.add_argument("--ws-host", default="127.0.0.1")
    parser.add_argument("--ws-port", type=int, default=8765)
    parser.add_argument(
        "--file-store",
        default="/tmp/imu-altitude-bridge",
        help="Scratch dir required by the F' pipeline (unused for uplink here)",
    )
    return parser.parse_args()


if __name__ == "__main__":
    asyncio.run(main(parse_args()))
