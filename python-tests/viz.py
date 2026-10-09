import numpy as np
import matplotlib.pyplot as plt


def plot_spectrogram_comparison(
    spectrum1: np.ndarray,
    spectrum2: np.ndarray,
    frequencies: np.ndarray | None = None,
    times: np.ndarray | None = None,
    titles: tuple[str, str] = ("Spectrum 1", "Spectrum 2"),
    cmap: str = "magma",
    log_amplitude: bool = False,
):
    """
    Display two audio spectrograms side-by-side for visual comparison.

    Parameters
    ----------
    spectrum1, spectrum2 : np.ndarray
        Arrays of shape (T, k), where rows are time points and
        columns are frequency bins.

    frequencies : np.ndarray, optional
        Frequency values for the k bins, shape (k,).

    times : np.ndarray, optional
        Time values for the T time points, shape (T,).

    titles : tuple[str, str]
        Titles for the two spectrograms.

    cmap : str
        Matplotlib colormap.

    log_amplitude : bool
        If True, display amplitude in dB using 10*log10(amplitude).
    """

    spectrum1 = np.asarray(spectrum1)
    spectrum2 = np.asarray(spectrum2)

    if spectrum1.ndim != 2 or spectrum2.ndim != 2:
        raise ValueError("Both spectra must have shape (T, k).")

    if spectrum1.shape != spectrum2.shape:
        raise ValueError(
            f"Spectra must have the same shape, got "
            f"{spectrum1.shape} and {spectrum2.shape}."
        )

    T, k = spectrum1.shape

    if frequencies is not None:
        frequencies = np.asarray(frequencies)
        if frequencies.shape != (k,):
            raise ValueError("frequencies must have shape (k,)")

    if times is not None:
        times = np.asarray(times)
        if times.shape != (T,):
            raise ValueError("times must have shape (T,)")

    # Convert to floating point
    data1 = spectrum1.astype(float)
    data2 = spectrum2.astype(float)

    # Optionally convert amplitudes to dB
    if log_amplitude:
        eps = np.finfo(float).eps
        data1 = 10 * np.log10(np.maximum(data1, eps))
        data2 = 10 * np.log10(np.maximum(data2, eps))

    # Shared color scale
    vmin = min(data1.min(), data2.min())
    vmax = max(data1.max(), data2.max())

    # Axis extents
    if times is not None:
        x_min, x_max = times[0], times[-1]
    else:
        x_min, x_max = 0, T - 1

    if frequencies is not None:
        y_min, y_max = frequencies[0], frequencies[-1]
    else:
        y_min, y_max = 0, k - 1

    # Create side-by-side plots
    fig, axes = plt.subplots(
        1,
        2,
        figsize=(16, 6),
        sharey=True,
    )

    im1 = axes[0].imshow(
        data1.T,
        aspect="auto",
        origin="lower",
        extent=[x_min, x_max, y_min, y_max],
        cmap=cmap,
        vmin=vmin,
        vmax=vmax,
        interpolation="nearest",
    )

    im2 = axes[1].imshow(
        data2.T,
        aspect="auto",
        origin="lower",
        extent=[x_min, x_max, y_min, y_max],
        cmap=cmap,
        vmin=vmin,
        vmax=vmax,
        interpolation="nearest",
    )

    # Labels
    axes[0].set_title(titles[0])
    axes[1].set_title(titles[1])

    axes[0].set_xlabel("Time")
    axes[1].set_xlabel("Time")
    axes[0].set_ylabel("Frequency")


    fig.suptitle("Audio Spectrum Comparison", fontsize=14)

    plt.tight_layout()
    plt.show()

