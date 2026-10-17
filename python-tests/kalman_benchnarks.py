from kalmanfilter import filter, I
import numpy as np
import os
from utils import index_to_notename, notename_to_index
from viz import plot_spectrogram_comparison4, plot_spectrogram_comparison

def get_default_matrices(H_path="python-tests/pythonlogs/logH.txt"):
    H = np.loadtxt(H_path)

    # write H into csv file with one decimal point
    F = I(H.shape[1]) *0.7
    P = I(H.shape[1])

    # Q, R = estimate_R_Q(spectrum, N=10, per_bin=True, eps=1e-12)
    Q = I(H.shape[1])
    # measurement noise
    R = I(H.shape[0])  # np.full((spectrum.shape[1], spectrum.shape[1]), 100)
    return H, F, P, Q, R

#H = create_matrix(spectrum.shape[1])
def benchmark_singlenotes(singlenotes_path="python-tests/spectrograms/power/singlenotes"):
    H, F, P, Q, R = get_default_matrices()

    # Q, R = estimate_R_Q(spectrum, N=10, per_bin=True, eps=1e-12)
    Q = I(H.shape[0])
    # measurement noise
    R = I(H.shape[0])  # np.full((spectrum.shape[1], spectrum.shape[1]), 100)

    correct = 0
    for filename in os.listdir(singlenotes_path):
        if filename.endswith(".txt"):
            spectrum = np.loadtxt(os.path.join(singlenotes_path, filename))

            x_est_all, innseq, corrseq = filter(spectrum, H, P, R, F, Q)

            estimated_note = index_to_notename(np.argmax(np.mean(x_est_all, axis=0)))
            # print(f"{filename}: estimated note: {estimated_note}")
            if estimated_note == filename.split(".")[0]:
                correct += 1
            else:
                print(f"Incorrect estimation for {filename.split('.')[0]}: estimated {estimated_note}")
    print(f"Accuracy: {correct}/{len(os.listdir(singlenotes_path))} = {correct/len(os.listdir(singlenotes_path))*100:.2f}%")


def viz_spectrum(spectrum_path, post_processing=lambda x: x):
    H, F, P, Q, R = get_default_matrices()


    spectrum = np.loadtxt(spectrum_path)

    x_est_all, innseq, corrseq = filter(spectrum, H, P, R, F, Q)
    x_est_all = post_processing(x_est_all)

    plot_spectrogram_comparison4(spectrum, x_est_all, innseq, corrseq, titles=("Original Spectrum", "Reconstructed Spectrum", "Innovation", "Correction"), shared_color_scale=False, log_amplitude=False)

def filter_spectrum(path, H_path="python-tests/pythonlogs/logH.txt"):
    H, F, P, Q, R = get_default_matrices(H_path=H_path)

    spectrum = np.loadtxt(path)

    x_est_all, innseq, corrseq = filter(spectrum, H, P, R, F, Q)
    return x_est_all, spectrum



def post(spectrum, threshold=0.1):
    # set all values below 0.1 to 0
    spectrum[spectrum < threshold] = 0

    return spectrum
    

if __name__ == "__main__":
    benchmark_singlenotes()
    #exit()



"""
    expected = ["c3", "g3", "c4", "e4", "g4", "c5"]
    for e in expected:
        print(notename_to_index(e))
    filtered_spec_extra, spec_extra = filter_spectrum("python-tests/spectrograms/C_high_strum.txt", H_path="python-tests/pythonlogs/logH_extra.txt")#, post_processing=lambda x: post(x, threshold=0.7))
    filtered_spec, spec = filter_spectrum("python-tests/spectrograms/C_high_strum.txt", H_path="python-tests/pythonlogs/logH.txt")
    plot_spectrogram_comparison4(spec_extra, filtered_spec_extra, spec, filtered_spec, titles=("spec", "filtered spec", "spec extra", "filtered spec extra"))

    """

#viz_spectrum("python-tests/spectrograms/power/singlenotes/h4.txt")    
spec_mag = np.loadtxt("python-tests/spectrograms/magnitude/singlenotes/h4.txt")
spec_pow = np.loadtxt("python-tests/spectrograms/power/singlenotes/h4.txt")

plot_spectrogram_comparison(spectrum1=spec_mag, spectrum2=spec_pow)

# todo: model decay in F dependent on frequency bin, so that higher frequencies decay faster than lower frequencies