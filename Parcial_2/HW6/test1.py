#!/usr/bin/env python3
import argparse, csv, os, sys
from datetime import datetime
import paho.mqtt.client as mqtt
from paho.mqtt.properties import Properties
from paho.mqtt.packettypes import PacketTypes

def now_iso():
    from datetime import datetime
    return datetime.now().isoformat(timespec="seconds")

def ensure_csv(path):
    new = not os.path.exists(path)
    f = open(path, "a", newline="", encoding="utf-8")
    w = csv.writer(f)
    if new:
        w.writerow(["timestamp","topic","payload","qos","retain","mid","user_properties"])
    return f, w

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"[{now_iso()}] CONNECTED rc={reason_code}")
    topic = f"{userdata['team']}/#"
    client.subscribe(topic, qos=1)
    print(f"[{now_iso()}] SUBSCRIBED to {topic}")

def on_message(client, userdata, msg):
    props = getattr(msg, "properties", None)
    user_props = []
    if props and getattr(props, "UserProperty", None):
        user_props = [f"{k}={v}" for (k,v) in props.UserProperty]
    row = [
        now_iso(),
        msg.topic,
        msg.payload.decode("utf-8", errors="replace"),
        msg.qos,
        int(msg.retain),
        msg.mid,
        ";".join(user_props)
    ]
    userdata["writer"].writerow(row)
    userdata["file"].flush()
    print(f"[{now_iso()}] RX {msg.topic}: {row[2]} (qos={msg.qos}, retain={int(msg.retain)})")

def main():
    ap = argparse.ArgumentParser(description="MQTT v5 CSV logger")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=1883)
    ap.add_argument("--team", default="u01", help="Prefijo de tópicos, ej. u01")
    ap.add_argument("--csv", default="mqtt_log.csv")
    args = ap.parse_args()

    f, w = ensure_csv(args.csv)
    userdata = {"team": args.team, "file": f, "writer": w}

    client = mqtt.Client(client_id=f"logger-{args.team}", userdata=userdata, protocol=mqtt.MQTTv5)
    client.on_connect = on_connect
    client.on_message = on_message

    props = Properties(PacketTypes.CONNECT)  # opcional: props v5
    client.connect(args.host, args.port, keepalive=60, properties=props)

    print(f"[{now_iso()}] CONNECTING to {args.host}:{args.port} … (team={args.team})")
    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print("\nbye.")
    finally:
        f.close()

if __name__ == "__main__":
    main()

