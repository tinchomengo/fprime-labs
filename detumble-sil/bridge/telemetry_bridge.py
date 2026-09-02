#!/usr/bin/env python3
"""Bridges F' telemetry (attitude quaternion/rate/mode from DetumbleSim,
orbital position/angle from OrbitPropagator) AND the full F' event log
(every event from every component - mode changes, orbit completions,
command dispatch, rate group health, ...) to a WebSocket for the three.js
frontend, and relays a "KICK" message the other way as the KICK command
(re-tumble on demand)."""

import argparse
import asyncio
import json

import websockets

from fprime_gds.common.handlers import DataHandler
from fprime_gds.common.models.dictionaries import Dictionaries
from fprime_gds.common.pipeline.standard import StandardPipeline
from fprime_gds.common.utils.config_manager import ConfigManager

CHANNEL_KEYS = {
    "TelemetryDeployment.detumbleSim.Qw": "qw",
    "TelemetryDeployment.detumbleSim.Qx": "qx",
    "TelemetryDeployment.detumbleSim.Qy": "qy",
    "TelemetryDeployment.detumbleSim.Qz": "qz",
    "TelemetryDeployment.detumbleSim.OmegaMagnitude": "omegaMagnitude",
    "TelemetryDeployment.detumbleSim.ControlTorqueMagnitude": "controlTorqueMagnitude",
    "TelemetryDeployment.detumbleSim.RotationalKineticEnergy": "kineticEnergy",
    "TelemetryDeployment.detumbleSim.Mode": "mode",
    "TelemetryDeployment.orbitPropagator.PositionX": "positionX",
    "TelemetryDeployment.orbitPropagator.PositionY": "positionY",
    "TelemetryDeployment.orbitPropagator.PositionZ": "positionZ",
    "TelemetryDeployment.orbitPropagator.Altitude": "altitude",
    "TelemetryDeployment.orbitPropagator.OrbitAngle": "orbitAngle",
    "TelemetryDeployment.orbitPropagator.OrbitalPeriodS": "orbitalPeriodS",
    "TelemetryDeployment.orbitPropagator.OrbitalVelocity": "orbitalVelocity",
    "TelemetryDeployment.orbitPropagator.OrbitCount": "orbitCount",
    # (No F' framework telemetry tiles here anymore - RgMaxTime/RgCycleSlips
    # sat at 0 too often to be worth a tile, and command-dispatch activity
    # reads far better as the actual OpCodeDispatched/OpCodeCompleted event
    # lines on the terminal than as a bare counter. See the event log
    # instead - it still surfaces real F' framework activity, verbatim.)
}

KICK_COMMAND = "TelemetryDeployment.detumbleSim.KICK"


class _WebSocketBridge(DataHandler):
    """Shared broadcast plumbing for the two DataHandlers below - only
    data_callback (what to send) differs between them."""

    def __init__(self, loop, clients):
        self.loop = loop
        self.clients = clients

    def _send(self, message):
        asyncio.run_coroutine_threadsafe(self._broadcast(message), self.loop)

    async def _broadcast(self, message):
        if not self.clients:
            return
        await asyncio.gather(
            *(client.send(message) for client in self.clients),
            return_exceptions=True,
        )


class TelemetryBridge(_WebSocketBridge):
    """Forwards decoded F' channel updates to all connected WebSocket clients."""

    def data_callback(self, data, sender=None):
        key = CHANNEL_KEYS.get(data.template.get_full_name())
        if key is None:
            return
        self._send(json.dumps({"channel": key, "value": data.get_val()}))


class EventBridge(_WebSocketBridge):
    """Forwards every F' event, verbatim (F's own formatted string - the same
    line fprime-gds's own event log shows), to all connected WebSocket
    clients as a terminal-log line - not filtered to any one component, so
    it reads as a genuine activity feed of the whole deployment."""

    def data_callback(self, data, sender=None):
        self._send(json.dumps({"type": "event", "text": data.get_str()}))


async def handle_client(websocket, clients, pipeline):
    clients.add(websocket)
    print(f"three.js client connected ({len(clients)} total)")
    try:
        async for message in websocket:
            # Only one client message is understood: a KICK request from
            # the page's "Re-kick" button, relayed as the real F' command -
            # not a fabricated/simulated response.
            if message == "KICK":
                print("[bridge] relaying KICK command")
                pipeline.send_command(KICK_COMMAND, [])
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

    bridge = TelemetryBridge(loop, clients)
    pipeline.coders.register_channel_consumer(bridge)

    event_bridge = EventBridge(loop, clients)
    pipeline.coders.register_event_consumer(event_bridge)

    async with websockets.serve(lambda ws: handle_client(ws, clients, pipeline), args.ws_host, args.ws_port):
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
        default="/tmp/detumble-sil-bridge",
        help="Scratch dir required by the F' pipeline (unused for uplink here)",
    )
    return parser.parse_args()


if __name__ == "__main__":
    asyncio.run(main(parse_args()))
