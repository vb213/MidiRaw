import numpy as np
import librosa
import os
from tqdm import tqdm

def wav_to_cqt_spectrogram(
    filepath: str,
    sr: int | None = None,
    hop_length: int = 128,
    n_bins: int = 84,
    bins_per_octave: int = 12,
    fmin: float | None = None,
    to_db: bool = False,
) -> np.ndarray:
    """
    Load a .wav file and compute its Constant-Q Transform spectrogram.

    Parameters
    ----------
    filepath : str
        Path to the .wav file.
    sr : int | None
        Target sample rate. None keeps the file's native sample rate.
    hop_length : int
        Number of samples between successive CQT columns.
    n_bins : int
        Total number of frequency bins.
    bins_per_octave : int
        Number of bins per octave.
    fmin : float | None
        Minimum frequency in Hz. Defaults to C1 (~32.7 Hz).
    to_db : bool
        If True, return magnitude in dB (relative to the max); otherwise
        return the linear magnitude.

    Returns
    -------
    np.ndarray
        Array of shape (time_frames, n_bins).
    """
    y, sr = librosa.load(filepath, sr=sr, mono=True)

    if fmin is None:
        fmin = librosa.note_to_hz("C1")

    cqt = librosa.cqt(
        y,
        sr=sr,
        hop_length=hop_length,
        fmin=fmin,
        n_bins=n_bins,
        bins_per_octave=bins_per_octave,
    )  # complex, shape: (n_bins, time_frames)

    spec = np.abs(cqt)

    if to_db:
        spec = librosa.amplitude_to_db(spec, ref=np.max)

    return spec.T  # shape: (time_frames, n_bins)


if __name__ == "__main__":


    for filename in tqdm(os.listdir("python-tests/chords")):
        if filename.endswith(".wav"):
            filepath = os.path.join("python-tests/chords", filename)
            spec = wav_to_cqt_spectrogram(filepath)
            savename = os.path.splitext(filename)[0] + ".txt"
            np.savetxt(os.path.join("python-tests/spectograms", savename), spec)