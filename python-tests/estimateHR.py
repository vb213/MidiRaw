from dataload import load_data
import numpy as np
import log
from kalmanfilter import create_matrix

_,_,spectrum_E = load_data("python-tests/testlogs/testlogE.txt")
"""
_,_,spectrum_EGisDis = load_data("python-tests/testlogs/testlogEG#D#.txt")
_,_,spectrum_GisA = load_data("python-tests/testlogs/testlogG#A.txt")
_,_,spectrum_GisH = load_data("python-tests/testlogs/testlogG#H.txt")
_,_,spectrum_GisHDis = load_data("python-tests/testlogs/testlogG#HD#.txt")"""
def estimateH():
    Z = spectrum_E.T
    X = np.zeros_like(Z)
    X[28] = 1

    print(X.shape)
    print(Z.shape)

    XX = X@X.T
    log.append_calculation_step_log("python-tests/pythonlogs/estlog.txt", "XXT = ",XX)
    inv = np.linalg.inv(XX + np.eye(spectrum_E.shape[1]) * 1e-6)
    log.append_calculation_step_log("python-tests/pythonlogs/estlog.txt", "np.linalg.inv(XX + np.eye(spectrum_E.shape[1]) * 1e-6) = ",inv)

    H = Z @ X.T @ inv

    log.append_calculation_step_log("python-tests/pythonlogs/estlog.txt", "H = ",H)


def estimateR():
    H = create_matrix()