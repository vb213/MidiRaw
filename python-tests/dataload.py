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

    return basenotes, harmonics, spectrum


