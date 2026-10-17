import numpy as np
import librosa
import os
from tqdm import tqdm

import numpy as np
import librosa

def wav_to_cqt_spectrogram(
    filepath: str,
    sr: int | None = 22050,
    hop_length: int = 512,
    n_bins: int = 84,
    bins_per_octave: int = 12,
    fmin: float | None = None,
    domain: str = "magnitude",   # "magnitude" | "power" | "db"
    normalize: bool = False,
) -> np.ndarray:
    """
    Returns array of shape (time_frames, n_bins).
    domain="power" gives |C|^2 (use this for a KF with additive power model).
    domain="db" is for visualization only (log scale, not linear in x).
    """
    y, sr = librosa.load(filepath, sr=sr, mono=True)
    if fmin is None:
        fmin = librosa.note_to_hz("C1")

    C = librosa.cqt(y, sr=sr, hop_length=hop_length, fmin=fmin,
                    n_bins=n_bins, bins_per_octave=bins_per_octave)
    mag = np.abs(C)

    if normalize:
        mag = mag / (mag.max() + 1e-12)

    if domain == "magnitude":
        spec = mag
    elif domain == "power":
        spec = mag ** 2
    elif domain == "db":
        spec = librosa.amplitude_to_db(mag, ref=1.0)   # no max-relative scaling
    else:
        raise ValueError(f"unknown domain: {domain}")

    return spec.T


def generate_spectrograms(domain="power"):
    for filename in tqdm(os.listdir("python-tests/samples")):
        if filename.endswith(".wav"):
            filepath = os.path.join("python-tests/samples", filename)
            spec = wav_to_cqt_spectrogram(filepath, domain=domain)
            savename = os.path.splitext(filename)[0] + ".txt"
            if len(filename) == 6:
                savename = "singlenotes/" + savename
            np.savetxt(os.path.join(f"python-tests/spectrograms/{domain}", savename), spec)

if __name__ == "__main__":
    generate_spectrograms()
