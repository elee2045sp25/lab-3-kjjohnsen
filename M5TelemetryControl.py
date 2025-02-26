import paho.mqtt.client as mqtt
import time
import struct
import tkinter as tk
from tkinter import ttk
from datetime import datetime
import pandas as pd

MOVEMENT_TOPIC = "elee2045sp25/kjohnsen/movement"
BATTERY_TOPIC = "elee2045sp25/kjohnsen/battery"
SOUND_TOPIC = "elee2045sp25/kjohnsen/sound"
RATE_TOPIC = "elee2045sp25/kjohnsen/rate"

class MQTTApp:
    def __init__(self, root):
        self.root = root
        self.root.title("MQTT Data Viewer")

        self.accel_label = ttk.Label(root, text="Accelerometer: ")
        self.accel_label.grid(row=0, column=0, sticky="w")
        self.accel_data = ttk.Label(root, text="")
        self.accel_data.grid(row=0, column=1, sticky="w")

        self.gyro_label = ttk.Label(root, text="Gyroscope: ")
        self.gyro_label.grid(row=1, column=0, sticky="w")
        self.gyro_data = ttk.Label(root, text="")
        self.gyro_data.grid(row=1, column=1, sticky="w")

        self.battery_label = ttk.Label(root, text="Battery: ")
        self.battery_label.grid(row=2, column=0, sticky="w")
        self.battery_data = ttk.Label(root, text="")
        self.battery_data.grid(row=2, column=1, sticky="w")

        self.sound_label = ttk.Label(root, text="Sound: ")
        self.sound_label.grid(row=3, column=0, sticky="w")
        self.sound_data = ttk.Label(root, text="")
        self.sound_data.grid(row=3, column=1, sticky="w")

        self.sound_bar = ttk.Progressbar(root, orient="horizontal", length=200, mode="determinate", maximum=1.0)
        self.sound_bar.grid(row=4, column=0, columnspan=2, pady=5)

        self.rate_button = ttk.Button(root, text="Send Rate", command=self.send_rate)
        self.rate_button.grid(row=5, column=0, columnspan=2, pady=10)

        self.last_received_label = ttk.Label(root, text="Last Received: ")
        self.last_received_label.grid(row=6, column=0, sticky="w")
        self.last_received_time = ttk.Label(root, text="")
        self.last_received_time.grid(row=6, column=1, sticky="w")

        self.rate_input_label = ttk.Label(root, text="Rate (seconds):")
        self.rate_input_label.grid(row=7, column=0, sticky="w")
        self.rate_input = ttk.Entry(root)
        self.rate_input.grid(row=7, column=1, sticky="w")
        self.rate_input.insert(0, "60.0")

        self.client = mqtt.Client()
        self.client.username_pw_set(username="giiuser", password="giipassword")
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.last_received = 0

        print("Connecting to broker...")
        self.client.connect(host="eduoracle.ugavel.com", port=1883)
        self.client.loop_start()

        # Initialize Pandas DataFrames
        self.movement_df = pd.DataFrame(columns=["Timestamp", "ax", "ay", "az", "gx", "gy", "gz"])
        self.battery_df = pd.DataFrame(columns=["Timestamp", "Voltage (mV)"])
        self.sound_df = pd.DataFrame(columns=["Timestamp", "Sound Level"])

    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            print("Connected")
            client.subscribe(topic=MOVEMENT_TOPIC)
            client.subscribe(topic=BATTERY_TOPIC)
            client.subscribe(topic=SOUND_TOPIC)
        else:
            print("Failed to connect, rc = ", rc)
            exit(-1)

    def on_message(self, client, userdata, msg):
        self.last_received = time.time()
        formatted_time = datetime.fromtimestamp(self.last_received).strftime("%Y-%m-%d %H:%M:%S")
        self.last_received_time.config(text=formatted_time)

        timestamp = datetime.now()

        if msg.topic == MOVEMENT_TOPIC:
            ax, ay, az, gx, gy, gz = struct.unpack("<ffffff", msg.payload)
            self.accel_data.config(text=f"{ax:.2f}, {ay:.2f}, {az:.2f}")
            self.gyro_data.config(text=f"{gx:.2f}, {gy:.2f}, {gz:.2f}")
            self.movement_df.loc[len(self.movement_df)] = [timestamp, ax, ay, az, gx, gy, gz]
        if msg.topic == BATTERY_TOPIC:
            battery_voltage_mv = struct.unpack("<h", msg.payload)[0]
            self.battery_data.config(text=f"{battery_voltage_mv} mV")
            self.battery_df.loc[len(self.battery_df)] = [timestamp, battery_voltage_mv]
        if msg.topic == SOUND_TOPIC:
            sound_level = struct.unpack("<f", msg.payload)[0]
            self.sound_data.config(text=f"{sound_level:.2f}")
            self.sound_bar["value"] = sound_level
            self.sound_df.loc[len(self.sound_df)] = [timestamp, sound_level]

    def send_rate(self):
        if self.client.is_connected:
            try:
                rate_seconds = float(self.rate_input.get())
                rate_ms = int(rate_seconds * 1000)
                to_send = struct.pack("<I", rate_ms)
                self.client.publish(RATE_TOPIC, to_send, retain=True)
                print(f"Rate sent: {rate_seconds} seconds.")
            except ValueError:
                print("Invalid rate input. Please enter a number.")

    def save_files(self):
        # Save DataFrames to CSV files
        self.movement_df.to_csv("movement_data.csv", index=False)
        self.battery_df.to_csv("battery_data.csv", index=False)
        self.sound_df.to_csv("sound_data.csv", index=False)

if __name__ == "__main__":
    root = tk.Tk()
    app = MQTTApp(root)
    root.mainloop()
    app.save_files()