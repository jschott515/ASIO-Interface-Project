import matplotlib.pyplot as plt
import numpy as np
import wave
import math

conv_factor = ((2**31) + .49999)


# TREM ------------------------------------------------------------------

def apply_trem(signal: np.ndarray) -> np.ndarray:
	dist_signal = np.copy(signal)

	Fx = 5
	Fs = 48000
	alpha = 0.5
	for i, x in enumerate(dist_signal):
		trem = (1 + alpha * math.sin(2 * math.pi * i * (Fx / Fs)))
		dist_signal[i] = trem * x

	return dist_signal


# SIGNAL PLOT -----------------------------------------------------------

def plot_signal(signal: np.ndarray):
	plt.figure(1)
	plt.title("Signal Wave")
	plt.plot(signal)
	plt.show()

	return


def main():
	spf = wave.open("test.wav", "r")

	# Extract Raw Audio from Wav File
	signal = spf.readframes(-1)
	signal = np.frombuffer(signal, int)
	# Convert to float
	signal = np.copy(signal).astype(float)
	signal /= conv_factor



	dist_signal = apply_trem(signal)

	plot_signal(signal)
	plot_signal(dist_signal)

	# Convert to int
	dist_signal *= conv_factor
	dist_signal = dist_signal.astype(int)

	# Write to wav file
	res = wave.open("pywave.wav", "w")
	res.setparams(spf.getparams())
	res.writeframes(dist_signal)
	spf.close()
	res.close()


if __name__ == "__main__":
	main()