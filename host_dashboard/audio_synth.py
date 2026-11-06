import sounddevice as sd
import numpy as np

# class to generate audio signals based on frequency input
class AudioSynth:

    # initialize the AudioSynth with a given sampling frequency (default 44100 Hz)
    def __init__(self, fs=44100):
        self.fs = fs
        self.current_freq = 200.0
        self.stream = None

    # set the frequency for the audio signal
    def set_frequency(self, freq):
        self.current_freq = freq

    # callback function for the audio stream that generates a sine wave based on the current frequency
    def _callback(self, outdata, frames, time_info, status):
        # divide the time array by the sampling frequency to get time in seconds
        t = np.arange(frames) / self.fs
        # generate a sine wave with the current frequency and a fixed amplitude of 0.2
        wave = 0.2 * np.sin(2 * np.pi * self.current_freq * t)
        # reshape the wave to match the output data shape and assign it to outdata
        outdata[:] = wave.reshape(-1, 1)

    # start the audio stream and begin generating sound
    def start(self):
        self.stream = sd.OutputStream(samplerate=self.fs, channels=1, callback=self._callback)
        self.stream.start()