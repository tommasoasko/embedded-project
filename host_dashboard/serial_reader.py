import serial
import threading
import queue

# class to read data from the serial port in a separate thread
class SerialReader:

    # initialize the SerialReader with the specified port and baudrate
    def __init__(self, port='COM3', baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self.data_queue = queue.Queue()
        self.running = False

    # start the serial reading thread
    def start(self):
        self.running = True
        threading.Thread(target=self._read_loop, daemon=True).start()

    # function that the thread runs to continuously read from the serial port and put the data into a queue
    def _read_loop(self):
        try:
            ser = serial.Serial(self.port, self.baudrate, timeout=1)
            print(f"Connected to serial {self.port}")
            while self.running:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if "POT" in line and "NTC" in line:
                    try:
                        parts = line.split(',')
                        pot = int(parts[0].split(':')[1].strip())
                        ntc = int(parts[1].split(':')[1].strip())
                        self.data_queue.put((pot, ntc))
                    except (IndexError, ValueError):
                        pass
        except Exception as e:
            print(f"Error serial: {e}")