#!/usr/bin/env python3

import signal
import sys
import time


def handle_exit(signum, frame):
    print("Shutting down fake I2P daemon", flush=True)
    sys.exit(0)


def main():
    signal.signal(signal.SIGTERM, handle_exit)
    signal.signal(signal.SIGINT, handle_exit)

    # Announce readiness similar to the real daemon output the GUI expects.
    print("SOCKS proxy started", flush=True)
    time.sleep(0.1)
    print("Network status: OK", flush=True)

    while True:
        time.sleep(1)


if __name__ == "__main__":
    main()
