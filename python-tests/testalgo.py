import numpy as np
import scipy.optimize as opt
from viz import plot_spectrogram_comparison
def load_data(path):
    basenotes = []
    harmonics = []
    spectrum = []

    with open(path, "r", encoding="utf-8") as f:
        for line_number, line in enumerate(f, 1):
            line = line.strip()

            try:
                section1, section2, section3 = line.split("#")

                values1 = [int(x) for x in section1.split(",")]
                values2 = [int(x) for x in section2.split(",")]
                values3 = [float(x) for x in section3.split(",")]


                if any(value != 0.000 for value in line):
                    basenotes.append(values1)
                    harmonics.append(values2)
                    spectrum.append(values3)
                else:
                    print(f"Line {line_number}: all values are zero, skipping line.")
                    continue

            except ValueError as e:
                print(f"Line {line_number}: invalid format: {e}")
                continue

    basenotes = np.array(basenotes)
    harmonics = np.array(harmonics)
    spectrum = np.array(spectrum)
    print("basenotes shape:", basenotes.shape)
    print("harmonics shape:", harmonics.shape)
    print("spectrum shape:", spectrum.shape)
    return basenotes, harmonics, spectrum

basenotes, harmonics, spectrum = load_data("testfiles/testlogs/testlogE.txt")
def threshholds():
    # find largest element in 2d matrix spectrum
    # spectrum = spectrum / np.max(spectrum)

    print (np.max(spectrum))
    harmonic_threshhold = 0.25
    basenote_threshhold = 0.4

    basenotes_new = np.zeros_like(spectrum)
    harmonics_new = np.zeros_like(spectrum)

    for line in spectrum:
        max_peak = np.max(line)
        for i in range(len(line)):
            if line[i] > basenote_threshhold: #* max_peak:
                basenotes_new[np.where(spectrum == line)[0][0]][i] = 1
            if line[i] > harmonic_threshhold: # * max_peak:
                harmonics_new[np.where(spectrum == line)[0][0]][i] = 1



    count_basenotes = len([x for x in np.reshape(basenotes_new, -1) if x == 1])
    count_harmonics = len([x for x in np.reshape(harmonics_new, -1) if x == 1])
    print(f"Number of detected base notes: {count_basenotes}")
    print(f"Number of detected harmonics: {count_harmonics}")

    overtonePattern = [-42, -38, -34, -28, -19, 19, 28, 34, 38, 42]
    scoreThreshhold = 2
    for i in range(len(basenotes_new)):
        for j in range(len(basenotes_new[i])):
            if basenotes_new[i][j] == 1:
                score = 0
                for k in overtonePattern:
                    if 0 <= j + k < len(basenotes_new[i]) and harmonics_new[i][j + k] == 1:
                        score += 1
                if score >= scoreThreshhold:
                    print(f"Timestep {i}: Base note at index {j} has overtone  score {score:.2f}")
                        


def solve_eq():
    H = create_matrix()
    X_tic = np.zeros((spectrum.shape[0], spectrum.shape[1]))
    for i in range(spectrum.shape[0]):
        x = spectrum[i]
        res = opt.nnls(A=H, b=x)
        x_tic = np.matmul(H, res[0])
        X_tic[i] = x_tic
        #print(f"Timestep {i}: NNLS solution: {res[0]} with residual {res[1]:.4f}, {res[0][28]}")
        # print index of highest value in res[0]
        print(f"Timestep {i}:{np.argmax(res[0])}")
    plot_spectrogram_comparison(spectrum, X_tic, titles=("Original Spectrum", "Reconstructed Spectrum"), cmap="viridis", log_amplitude=True)

def create_matrix():
    H = np.zeros((spectrum.shape[1], spectrum.shape[1]))
    pattern = [0, 12, 19, 24, 28, 31, 34, 36, 38, 40]
    pattern2 = [-42, -38, -34, -28, -19, 0, 19, 28, 34, 38, 42]
    for i in range(H.shape[0]):
        for j in pattern2:
            if 0 <= i + j < H.shape[0]:
                H[i][i + j] = 1

    print("H matrix shape:", H.shape)
    return H

if __name__ == "__main__":
    solve_eq()