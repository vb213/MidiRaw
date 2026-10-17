import os
import numpy as np
import log
from utils import notename_to_index
from viz import plot1dtimeseries
#n_semitones=12\log_2(n)

pattern=[0, 12, 19, 24, 28, 31, 34, 36, 38, 40]#, 42,43,44, 46, 47]
N_FREQ_BINS = 84
def filter_spec(spec, index, pattern):
    spec_filtered = np.zeros_like(spec)
    for p in pattern:
        if 0 <= index + p < len(spec):
            spec_filtered[index + p] = spec[index + p]
        if 0 < index + p - 1 < len(spec):
            spec_filtered[index + p - 1] = spec[index + p - 1]
        if 0 < index + p + 1 < len(spec):
            spec_filtered[index + p + 1] = spec[index + p + 1]
    return spec_filtered

    return 
def get_mean_characteristics(spectrogram, index, pattern):
    mean_spec = np.mean(spectrogram[20:-300], axis=0)

    return filter_spec(mean_spec, index, pattern)



def create_H_matrix(pattern, save_path, single_notes_path):
    H = np.eye(N_FREQ_BINS)
    # index 0 ist tief



    log.clear_logfile(save_path)
    # add bases
    for filename in os.listdir(single_notes_path):
        if filename.endswith(".txt"):
            spectrogram = np.loadtxt(os.path.join(single_notes_path, filename))
            index = notename_to_index(filename.split(".")[0])
            mean_characteristics = get_mean_characteristics(spectrogram, index, pattern)
            H[:, index] = mean_characteristics


    # shift bases for
    update = []
    last_active_i = 0
    for i in range(H.shape[1]):
        if np.linalg.norm(H[:, i]- np.eye(H.shape[0])[:, i]) < 1e-6:
            update.append(i)
            
        else: 
            print("Updating ", update)
            for j in update:
                H[:, j] = np.roll(H[:,i], j - i)
                #H[:max(j-1,0), j] = 0
                H[j - i:, j] = 0
            last_active_i = i
            update = []
    print("Updating ", update)
    for j in update:
        H[:, j] = np.roll(H[:,last_active_i], j - last_active_i)
        H[:j-1, j] = 0


    H_n = np.zeros((H.shape[0], H.shape[1] + 1))
    H_n[:, :-1] = H
    H = H_n
    H[12:80, -1] = 1
    np.savetxt(save_path, H, fmt="%.6f")


def estimate_decay():
    F = np.eye(N_FREQ_BINS)

    for filename in os.listdir("python-tests/spectrograms/singlenotes"):
        if filename.endswith(".txt"):
            spectrogram = np.loadtxt(os.path.join("python-tests/spectrograms/singlenotes", filename))[100:-300]
            index = notename_to_index(filename.split(".")[0])
            #filtered_spec = filter_spec(spectrogram, index, pattern)

            y_amplitude_intensity = np.sum(spectrogram, axis=1)
            print(y_amplitude_intensity)
            plot1dtimeseries(y_amplitude_intensity, f"python-tests/plots/amplitudes/{filename.split(".")[0]}.png")
            #ratios = [y_amplitude_intensity[i+1] / y_amplitude_intensity[i] for i in range(y_amplitude_intensity.shape[0] -1)]

            #decay_factor = np.mean(ratios)
            #F[index, index] = decay_factor
    np.savetxt("python-tests/pythonlogs/logF.txt", F, fmt="%.6f")



create_H_matrix(pattern=pattern, save_path="python-tests/pythonlogs/logH_power.txt", single_notes_path="python-tests/spectrograms/power/singlenotes")