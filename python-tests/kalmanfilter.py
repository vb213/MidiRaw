import numpy as np
from utils import index_to_notename, load_data
from viz import plot_spectrogram_comparison, plot_spectrogram_comparison4
import log
import os
def expand_spectrum(spectrum, expandwidth=12):
    spectrum_expanded = np.zeros((spectrum.shape[0], spectrum.shape[1] + expandwidth))
    spectrum_expanded[:, expandwidth:] = spectrum
    return spectrum_expanded





def create_matrix(size, pattern=[0, 12, 19, 28, 31, 34, 38, 42]):
    H = np.zeros((size, size))


    pattern3 = [-40, -38, -36, -34, -31, -28, -24, -19, -12, 0, 12, 19, 24, 28, 31, 34, 36, 38, 40]
    pattern2 = [0, 19, 28, 34, 38, 42]
    pattern4 = [-24, -12, 0, 19, 28, 34, 38]
    pattern1 = [0, 12, 19, 24, 28, 31]#, 34, 36, 38, 40]
    for i in range(H.shape[0]):
        for j in pattern:
            if 0 <= i + j < H.shape[0]:
                H[i + j][i] = -0.05
                H[i][i + j] = 1

                

    return H.T



def post_processing(x_est, threshhold=0.1):
    # set all values below 0.1 to 0
    x_est[x_est < threshhold] = 0
    x_new = np.zeros_like(x_est)
    for i in range(len(x_est)):
        prev = x_est[i-1] if i > 0 else 1
        succ = x_est[i+1] if i < len(x_est)-1 else 1
        x_new[i] = prev * succ * x_est[i] 
    return x_est

def I(size):
    return np.eye(size)


# https://de.wikipedia.org/wiki/Kalman-Filter
def correction(H, P, R, x_pred, z, logfile=None):
    log.append_log_line(logfile, "------------------ CORRECTION ----------------")
    log.append_calculation_step_log(logfile, "H = ", H, fmt="%d")
    log.append_calculation_step_log(logfile, "P = ", P, fmt="%d")
    log.append_calculation_step_log(logfile, "R = ", R)
    log.append_calculation_step_log(logfile, "x_pred = ", x_pred)
    log.append_calculation_step_log(logfile, "z = ", z)
    K = P @ H.T @ np.linalg.inv(H @ P @ H.T + R)

    innovation = z - H @ x_pred
    correction = K @ innovation
    x_est = x_pred + correction
    P = (I(x_pred.shape[0]) - K @ H) @ P

    log.append_calculation_step_log(logfile, "K = P @ H.T @ (H @ P @ H.T + R)^{-1} = ", K)
    log.append_calculation_step_log(logfile, "x_est = x_pred + K @ (z - H @ x_pred) = ", x_est)
    log.append_calculation_step_log(logfile, "P = (I - K @ H) @ P = ", P)

    return K, x_est, P, innovation, correction

def prediction(F, x_est, P, Q, logfile=None):
    log.append_log_line(logfile, "------------------ PREDICTION ----------------")
    log.append_calculation_step_log(logfile, "F = ", F, fmt="%d")
    log.append_calculation_step_log(logfile, "P = ", P)
    log.append_calculation_step_log(logfile, "Q = ", Q)
    log.append_calculation_step_log(logfile, "x_est = ", x_est)
    x_pred = F @ x_est
    P_pred = F @ P @ F.T + Q

    log.append_calculation_step_log(logfile, "x_pred = F @ x_est", x_pred)
    log.append_calculation_step_log(logfile, "P_pred = F @ P @ F.T + Q", P_pred)


    return x_pred, P_pred



def filter(spectrum, H, P, R, F, Q, threshold_pre = 0.0, threshold_post = 0.0, logfile=None):
    log.clear_logfile(logfile)
    innovation_sequence = np.zeros_like(spectrum)
    correction_sequence = np.zeros((spectrum.shape[0], H.shape[1]))

    spectrum[spectrum < threshold_pre] = 0
    # init
    x_pred = np.zeros((H.shape[1]))
    z = spectrum[0]
    K, x_est, P, _, _ = correction(H, P, R, x_pred, z, logfile=logfile)

    x_est_all = np.zeros((spectrum.shape[0], H.shape[1]))

    x_est_all[0] = x_est

    
                

    for k in range(1,len(spectrum)):
        log.append_log_line(logfile, f"################### ITERATION {k} ###################")
        x_pred, P_pred = prediction(F, x_est, P, Q, logfile=logfile)
        z = spectrum[k]
        K, x_est, P, inn, corr = correction(H, P_pred, R, x_pred, z, logfile=logfile)
        # x_est_all[k] = post_processing(x_est, threshhold = threshhold_post)
        x_est_all[k] = x_est

        innovation_sequence[k] = inn
        correction_sequence[k] = corr


    x_est_all[x_est_all < threshold_post] = 0

    return x_est_all, innovation_sequence, correction_sequence 


if __name__ == "__main__":

    pass