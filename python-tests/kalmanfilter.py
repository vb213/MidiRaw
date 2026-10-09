import numpy as np
from testalgo import load_data
from viz import plot_spectrogram_comparison
_,_, spectrum = load_data("testfiles/testlogs/testlogG#HD#.txt")

def create_matrix(size= spectrum.shape[1]):
    H = np.zeros((size, size))
    #H = np.ones((size, size)) * 0.5
    # upper triangle of H to 1
    #H = np.tril(H, 1) * 0.1

    pattern3 = [-40, -38, -36, -34, -31, -28, -24, -19, -12, 0, 12, 19, 24, 28, 31, 34, 36, 38, 40]
    pattern2 = [0, 19, 28, 34, 38, 42]
    pattern4 = [-24, -12, 0, 19, 28, 34, 38]
    pattern = [0, 12, 19, 24, 28, 31]#, 34, 36, 38, 40]
    for i in range(H.shape[0]):
        for j in pattern2:
            if 0 <= i + j < H.shape[0]:
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


I = np.eye(spectrum.shape[1])

H = create_matrix()
# write H into csv file with one decimal point
np.savetxt("python-tests/H_matrix.csv", H, delimiter=",", fmt="%.1f")

np.savetxt("python-tests/spectrum.csv", spectrum[10:20], delimiter=",", fmt="%f")
F = I

# Q, R = estimate_R_Q(spectrum, N=10, per_bin=True, eps=1e-12)
Q = I 

spectrum_variance = np.var(spectrum, axis=0)

# measurement noise
R = np.full((spectrum.shape[1], spectrum.shape[1]), 100)
print(R)

# https://de.wikipedia.org/wiki/Kalman-Filter
def correction(H, P, R, x_pred, z):
    K = P @ H.T @ np.linalg.inv(H @ P @ H.T + R)
    #S = H @ P @ H.T + R
    #K = np.linalg.solve(S, H @ P).T
    x_est = x_pred + K @ (z - H @ x_pred)
    P = (I - K @ H) @ P
    return K, x_est, P

def prediction(F, x_est, P, Q):
    x = F @ x_est
    P_pred = F @ P @ F.T + Q

    return x, P_pred

threshhold = 0#.1
threshhold_post =0.2

spectrum[spectrum < threshhold] = 0

# init
x_pred = np.zeros_like(spectrum[0])
z = spectrum[0]
P = I
K, x_est, P = correction(H, P, R, x_pred, z)

x_est_all = np.zeros((spectrum.shape[0], spectrum.shape[1]))

x_est_all[0] = x_est
for k in range(1,len(spectrum)):
    x_pred, P_pred = prediction(F, x_est, P, Q)
    z = spectrum[k]
    K, x_est, P = correction(H, P_pred, R, x_pred, z)
    # x_est_all[k] = post_processing(x_est, threshhold = threshhold_post)
    x_est_all[k] = x_est


# x_est_all[x_est_all < threshhold_post] = 0

plot_spectrogram_comparison(spectrum, x_est_all,titles=("Original Spectrum", "Reconstructed Spectrum"), cmap="viridis", log_amplitude=True)
