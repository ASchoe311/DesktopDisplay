import threading
from pyexpat.errors import messages
from queue import Queue
import serial
import time
from datetime import datetime
from enum import IntEnum
import psutil
import math
import sys
import argparse

import platform
if platform.system() == "Linux":
    import pyamdgpuinfo
elif platform.system() == "Windows":
    import wmi
    w = wmi.WMI(namespace="root\\LibreHardwareMonitor")
    intelHWID = 0
    amdHWID = 0
    memHWID = "/ram"
    hwSensors = w.Sensor()
    for hw in w.Hardware():
        if 'AMD' in hw.Name:
            amdHWID = hw.Identifier
        if 'Intel' in hw.Name:
            intelHWID = hw.Identifier
else:
    sys.exit(1)

class Commands(IntEnum):
    READY = 0x00
    DISPLAY = 0x01
    DATE = 0x02
    TIME = 0x03
    CPU_TEMP = 0x04
    CPU_USE = 0x05
    GPU_TEMP = 0x07
    GPU_USE = 0x08
    GPU_FAN_SPEED = 0x09
    MEM_USE = 0x0A
    SONG = 0x0B
    VRAM_USE = 0x0C

class Displays(IntEnum):
    RIGHT = 0x00
    UP = 0x01
    DOWN = 0x02
    LEFT = 0x03
    SELECT = 0x04

def calculate_checksum(data):
    """Calculate XOR checksum of a byte array"""
    checksum = 0
    for byte in data:
        checksum ^= byte
    return checksum

def verify_checksum(data):
    given_checksum = data[-1]
    calc_checksum = calculate_checksum(data)

    return given_checksum == calc_checksum

def send_command(command, data, ser, do_checksum = True):
    """Send a command with data and checksum"""
    # Form the message
    message = bytearray([command, len(data)] + data)

    # Calculate and append checksum
    if do_checksum:
        checksum = calculate_checksum(message)
        message.append(checksum)

    # Send to Arduino
    ser.write(message)
    return message

def send_current_time(ser):
    """Send current time using the specified protocol format"""
    # Get current time
    now = datetime.now()

    # Extract hours (12-hour format), minutes, and AM/PM flag
    hour = now.hour % 12
    if hour == 0:  # Handle midnight/noon case
        hour = 12

    minute = now.minute
    is_pm = 1 if now.hour >= 12 else 0

    data = [hour, minute, is_pm]
    message = send_command(Commands.TIME, data, ser)

    # Debug output
    time_str = f"{hour}:{minute:02d} {'PM' if is_pm else 'AM'}"
    print(f"Sent time: {time_str}")
    print(f"Bytes: {[hex(b) for b in message]}")

def send_current_date(ser):
    now = datetime.now()
    year_parts = [now.year >> 8, now.year & 0xFF]

    data = [now.month, now.day, year_parts[0], year_parts[1]]

    message = send_command(Commands.DATE, data, ser)

    date_str = f"{now.month:02d}/{now.day:02d}/{now.year}"
    print(f"Sent date: {date_str}")
    print(f"Bytes: {[hex(b) for b in message]}")

def process_command(command_data):
    if command_data == "quit":
        return None
    if not verify_checksum(command_data[0:-1]): return False
    cmd = Commands(command_data[0])
    match cmd:
        case Commands.READY:
            return
        case Commands.DISPLAY:
            return Displays(command_data[2])

def send_cpu_temp(ser):
    if '_wmi' not in sys.modules:
        data = [math.ceil(psutil.sensors_temperatures()["coretemp"][0].current)]
        
    else:
        data = [math.ceil(next((x for x in hwSensors if x.Name == "Core Average" and x.Parent == intelHWID), None).Value)]

    message = send_command(Commands.CPU_TEMP, data, ser)
    print(f"Sent CPU temperature: {data[0]}")
    print(f"Bytes: {[hex(b) for b in message]}")

def send_cpu_use(ser):
    if '_wmi' not in sys.modules:
        data = [math.ceil(psutil.cpu_percent(interval=0.1))]
    else:
        data = [math.ceil(next((x for x in hwSensors if x.Name == "CPU Total" and x.Parent == intelHWID), None).Value)]

    message = send_command(Commands.CPU_USE, data, ser)
    print(f"Sent CPU usage: {data[0]}")
    print(f"Bytes: {[hex(b) for b in message]}")

def send_mem_use(ser):
    if '_wmi' not in sys.modules:
        data = [math.ceil(psutil.virtual_memory().percent)]
    else:
        data = [math.ceil(next((x for x in hwSensors if x.Name == "Memory" and x.Parent == memHWID), None).value)]

    message = send_command(Commands.MEM_USE, data, ser)
    print(f"Sent memory usage: {data[0]}")
    print(f"Bytes: {[hex(b) for b in message]}")

def send_gpu_temp(gpu, ser):
    if '_wmi' not in sys.modules:
        data = [math.ceil(gpu.query_temperature())]
    else:
        data = [math.ceil(next((x for x in hwSensors if x.Name == "GPU Core" and x.SensorType == "Temperature" and x.Parent == amdHWID), None).value)]
    message = send_command(Commands.GPU_TEMP, data, ser)
    print(f"Sent GPU temperature: {data[0]}")
    print(f"Bytes: {[hex(b) for b in message]}")

def send_gpu_use(gpu, ser):
    if '_wmi' not in sys.modules:
        data = [math.ceil(gpu.query_utilisation()[max(gpu.query_utilisation())]*100)]
    else:
        data = [math.ceil(next((x for x in hwSensors if x.Name == "GPU Core" and x.SensorType == "Load" and x.Parent == amdHWID), None).value)]
    message = send_command(Commands.GPU_USE, data, ser)
    print(f"Sent GPU usage: {data[0]}")
    print(f"Bytes: {[hex(b) for b in message]}")

def send_gpu_fan_speed(gpu, ser):
    if '_wmi' not in sys.modules:
        speed = psutil.sensors_fans()["amdgpu"][0].current
    else:
        speed = math.ceil(next((x for x in hwSensors if x.Name == "GPU Fan" and x.SensorType == "Fan" and x.Parent == amdHWID), None).value)    
    data = [speed >> 8, speed & 0xFF]
    message = send_command(Commands.GPU_FAN_SPEED, data, ser)
    print(f"Sent GPU fan speed: {data[0]}")
    print(f"Bytes: {[hex(b) for b in message]}")

def send_vram_use(gpu, ser):
    if '_wmi' not in sys.modules:
        data = [math.ceil(gpu.query_vram_usage() / gpu.memory_info["vram_size"])]
    else:
        data = [math.ceil(next((x for x in hwSensors if x.Name == "GPU Memory" and x.SensorType == "Load" and x.Parent == amdHWID), None).value)]
    message = send_command(Commands.VRAM_USE, data, ser)
    print(f"Sent VRAM usage: {data[0]}")
    print(f"Bytes: {[hex(b) for b in message]}")

def send_not_implemented_msg(disp, ser):
    msg = "Not Done"
    data = [int(ord(c)) for c in msg]
    data.append(int(disp))

    message = send_command(Commands.SONG, data, ser)

    song_str = f"{msg}"
    print(f"Sent song: {song_str}")
    print(f"Bytes: {[hex(b) for b in message]}")

def write_serial(ser, q):
    disp = Displays.RIGHT
    GPU = 0
    if 'pyamdgpuinfo' in sys.modules:
        GPU = pyamdgpuinfo.get_gpu(0)
        GPU.start_utilisation_polling()
    while True:
        if not q.empty():
            cmd = q.get()
            disp = process_command(cmd)
            if disp == None:
                break
            q.task_done()
        try:
            match disp:
                case Displays.RIGHT:
                    send_current_time(ser)
                    send_current_date(ser)
                case Displays.UP:
                    send_cpu_temp(ser)
                    send_mem_use(ser)
                    send_cpu_use(ser)
                case Displays.DOWN:
                    send_gpu_temp(GPU, ser)
                    send_gpu_use(GPU, ser)
                    send_gpu_fan_speed(GPU, ser)
                    send_vram_use(GPU, ser)
                case _:
                    send_not_implemented_msg(disp, ser)
                    while q.empty():
                        time.sleep(1)
        except:
            continue        
        time.sleep(1)

def main():
    parser = argparse.ArgumentParser(description="Sends data to arduino")
    parser.add_argument("-p", "--port", type=str, help="Serial port (optional)")
    args = parser.parse_args()
    port='/dev/ttyACM0'
    if args.port:
        port = args.port
    print(port)
    # Configure serial port - adjust as needed
    baud_rate = 115200

    try:
        # Open serial port
        ser = serial.Serial(port, baud_rate, timeout=1)
        print(f"Connected to {port} at {baud_rate} baud")

        q = Queue()

        while True:
            print("Waiting for arduino to be ready")
            send_command(Commands.READY, [], ser, False)
            time.sleep(2)
            if ser.in_waiting > 0:
                print("Data in serial")
                data_in = []
                data_in.append(int(ser.read(1).hex()))
                data_in.append(int(ser.read(1).hex()))
                print(data_in)
                if data_in == [0x00, 0x00]:
                    print("Arduino is ready")
                    break

        t1 = threading.Thread(target=write_serial, args=(ser,q,))
        t1.start()

        while True:
            if ser.in_waiting > 0:
                data_in = []
                while ser.in_waiting > 0:
                    data_in.append(int(ser.read(1).hex()))
                print(f"Received data: {data_in}")
                q.put(data_in)
                q.join()
            global hwSensors
            hwSensors = w.Sensor()
            time.sleep(0.1)

        # Close serial port
        ser.close()
        print("Serial port closed")

    except serial.SerialException as e:
        print(f"Error: {e}")

    except KeyboardInterrupt:
        print("Interrupt signal received")
        print("Joining serial write thread")
        q.put("quit")
        t1.join()
        print("Closing serial connection")
        ser.close()
        print("Exiting")
        sys.exit(0)

if __name__ == "__main__":
    main()
