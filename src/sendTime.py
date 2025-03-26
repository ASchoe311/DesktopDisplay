import threading
from pyexpat.errors import messages
from queue import Queue
import serial
import time
from datetime import datetime
from enum import IntEnum

class Commands(IntEnum):
    READY = 0x00
    DISPLAY = 0x01
    DATE = 0x02
    TIME = 0x03
    SONG = 0x0B

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

def send_command(command, data, ser):
    """Send a command with data and checksum"""
    # Form the message
    message = bytearray([command, len(data)] + data)

    # Calculate and append checksum
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
    if not verify_checksum(command_data[0:-1]): return
    cmd = Commands(command_data[0])
    match cmd:
        case Commands.READY:
            return
        case Commands.DISPLAY:
            return Displays(command_data[2])

def send_not_implemented_msg(disp, ser):
    msg = "Not Done"
    data = [int(ord(c)) for c in msg]

    message = send_command(Commands.SONG, data, ser)

    song_str = f"{msg}"
    print(f"Sent song: {song_str}")
    print(f"Bytes: {[hex(b) for b in message]}")

def write_serial(ser, q):
    disp = Displays.RIGHT
    while True:
        if not q.empty():
            cmd = q.get()
            disp = process_command(cmd)
            q.task_done()
        match disp:
            case Displays.RIGHT:
                send_current_time(ser)
                send_current_date(ser)
            case _:
                send_not_implemented_msg(disp, ser)
        time.sleep(1)
def main():
    # Configure serial port - adjust as needed
    port = '/dev/ttyACM0'  # Change to your CDC-ACM port
    baud_rate = 115200

    try:
        # Open serial port
        ser = serial.Serial(port, baud_rate, timeout=1)
        print(f"Connected to {port} at {baud_rate} baud")

        q = Queue()

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

            time.sleep(1)

        # Close serial port
        ser.close()
        print("Serial port closed")

    except serial.SerialException as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()