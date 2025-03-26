import serial
import time
import sys

def send_string_with_length(ser, text):
    """Send a string over UART preceded by its length as a byte"""
    # Convert string to bytes if it's not already
    if isinstance(text, str):
        text_bytes = text.encode('utf-8')
    else:
        text_bytes = text

    # Get length (capped at 255 due to one byte limit)
    length = min(len(text_bytes), 255)

    # Create message: length byte + text
    message = bytes([length]) + text_bytes[:length]

    # Send the message
    ser.write(message)

    # Print what was sent (for debugging)
    print(f"Sent: Length={length}, Data={text_bytes[:length]}")

    # Small delay to ensure transmission completes
    time.sleep(0.1)

def main():
    # Configure serial port - adjust these settings as needed
    port = '/dev/ttyACM0'  # Windows: 'COM1', Linux: '/dev/ttyUSB0', Mac: '/dev/tty.usbserial'
    baud_rate = 115200

    # Get port from command line if provided
    if len(sys.argv) > 1:
        port = sys.argv[1]

    try:
        # Open serial port
        ser = serial.Serial(port, baud_rate, timeout=1)
        print(f"Connected to {port} at {baud_rate} baud")

        # Main loop
        while True:
            # Get input from user
            text = input("Enter text to send (or 'exit' to quit): ")

            # Check for exit command
            if text.lower() == 'exit':
                break

            # Send the string with length prefix
            send_string_with_length(ser, text)

        # Close the serial port
        ser.close()
        print("Serial port closed")

    except serial.SerialException as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()