import sys
import numpy as np
import pyqtgraph as pg
from PyQt5.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QWidget
from PyQt5.QtCore import QTimer

from serial_reader import SerialReader
from audio_synth import AudioSynth

# class to create the main dashboard window that integrates serial reading and audio synthesis
class Dashboard(QMainWindow):
    # initialize the Dashboard window, set up the serial reader and audio synthesizer, and configure the GUI layout
    def __init__(self):
        super().__init__()
        self.setWindowTitle("E-Agle TRT - Telemetry Dashboard")
        self.resize(800, 500)

        # initialize the serial reader and audio synthesizer
        self.serial = SerialReader(port='COM3', baudrate=115200) # Controlla la tua porta
        self.synth = AudioSynth()

        # start the serial reader and audio synthesizer
        self.serial.start()
        self.synth.start()

        # setup the GUI
        layout = QVBoxLayout()
        central_widget = QWidget()
        central_widget.setLayout(layout)
        self.setCentralWidget(central_widget)

        self.plot_widget = pg.PlotWidget()
        self.plot_widget.setBackground('#1e1e1e')
        self.plot_widget.addLegend()
        layout.addWidget(self.plot_widget)

        # setup the curves for POT and NTC data with different colors and line widths
        self.pot_curve = self.plot_widget.plot(pen=pg.mkPen('r', width=2), name="POT")
        self.ntc_curve = self.plot_widget.plot(pen=pg.mkPen('g', width=2), name="NTC")

        self.history_len = 100
        self.pot_data = np.zeros(self.history_len)
        self.ntc_data = np.zeros(self.history_len)

        # refresh the dashboard at a regular interval (30 ms) to update the plots with new data
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_dashboard)
        self.timer.start(30)

    # update the dashboard by consuming data from the serial reader's queue, updating the plots, and adjusting the audio frequency based on the POT value
    def update_dashboard(self):
        updated = False
        # consume all available data from the serial reader's queue
        while not self.serial.data_queue.empty():
            pot, ntc = self.serial.data_queue.get()
            
            # update the POT and NTC data arrays by shifting the old data and appending the new values
            self.pot_data[:-1], self.pot_data[-1] = self.pot_data[1:], pot
            self.ntc_data[:-1], self.ntc_data[-1] = self.ntc_data[1:], ntc
            
            # map the POT value (0-4095) to a frequency range (150-1200 Hz) and set the audio synthesizer's frequency accordingly
            freq = 150.0 + (pot / 4095.0) * 1050.0
            self.synth.set_frequency(freq)
            
            updated = True

        # refresh the plots if new data was received and processed
        if updated:
            self.pot_curve.setData(self.pot_data)
            self.ntc_curve.setData(self.ntc_data)

# run the application if this script is executed as the main program
if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = Dashboard()
    window.show()
    sys.exit(app.exec_())