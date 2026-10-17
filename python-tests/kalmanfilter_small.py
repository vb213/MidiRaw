from kalmanfilter import filter, create_matrix
from utils import load_data
import numpy as np
from viz import plot_spectrogram_comparison

_,_, spectrum = load_data("python-tests/testlogs/artspec3.txt")


def generate_spectrum_sequence1(length=12):
    width = 7
    spectrum = np.zeros((length, width))
    spectrum[:, 2] = 1
    spectrum[:, 3] = 1

    H = create_matrix(width, pattern=[0,2])

    spectrum = spectrum @ H.T
    print(spectrum)

spectrum = generate_spectrum_sequence1()
exit()
I = np.eye(spectrum.shape[1])

H = create_matrix(spectrum.shape[1], pattern=[0,2]) # measurement equation

F = I # system equation

P_init = I # covariance of the state

Q = I# I is more responsive than np.zeros((spectrum.shape[1], spectrum.shape[1])) # system noise
#R = np.full((spectrum.shape[1], spectrum.shape[1]), 1) # measurement noise
R = I
x_est_all = filter(spectrum, H, P_init, R, F, Q, logfile="python-tests/pythonlogs/log.txt")

plot_spectrogram_comparison(spectrum, x_est_all,titles=("Original Spectrum", "Reconstructed Spectrum"), cmap="viridis", log_amplitude=True)
