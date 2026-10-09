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


    fig.suptitle("Audio Spectrum Comparison. Oben ist tief", fontsize=14)

    plt.tight_layout()
    plt.show()





def plot_spectrogram_comparison4(
    spectrum1: np.ndarray,
    spectrum2: np.ndarray,
    spectrum3: np.ndarray,
    spectrum4: np.ndarray,
    frequencies: np.ndarray | None = None,
    times: np.ndarray | None = None,
    titles: tuple[str, str, str, str] = (
        "Spectrum 1",
        "Spectrum 2",
        "Spectrum 3",
        "Spectrum 4",
    ),
    cmap: str = "magma",
    log_amplitude: bool = False,
):
    """
    Display four audio spectrograms in a 2x2 grid.

    Parameters
    ----------
    spectrum1, spectrum2, spectrum3, spectrum4 : np.ndarray
        Arrays of shape (T, k), where rows are time points and
        columns are frequency bins.

    frequencies : np.ndarray, optional
        Frequency values for the k bins, shape (k,).

    times : np.ndarray, optional
        Time values for the T time points, shape (T,).

    titles : tuple[str, str, str, str]
        Titles for the four spectrograms.

    cmap : str
        Matplotlib colormap.

    log_amplitude : bool
        If True, display amplitude in dB using 10*log10(amplitude).
    """

    spectra = [
        np.asarray(spectrum1),
        np.asarray(spectrum2),
        np.asarray(spectrum3),
        np.asarray(spectrum4),
    ]

    # Validate input dimensions
    for i, spectrum in enumerate(spectra, start=1):
        if spectrum.ndim != 2:
            raise ValueError(
                f"Spectrum {i} must have shape (T, k)."
            )

    # All spectra must have the same shape
    shape = spectra[0].shape
    for i, spectrum in enumerate(spectra[1:], start=2):
        if spectrum.shape != shape:
            raise ValueError(
                f"All spectra must have the same shape. "
                f"Spectrum 1 has shape {shape}, but "
                f"spectrum {i} has shape {spectrum.shape}."
            )

    T, k = shape

    if len(titles) != 4:
        raise ValueError("titles must contain exactly four strings.")

    if frequencies is not None:
        frequencies = np.asarray(frequencies)
        if frequencies.shape != (k,):
            raise ValueError("frequencies must have shape (k,)")

    if times is not None:
        times = np.asarray(times)
        if times.shape != (T,):
            raise ValueError("times must have shape (T,)")

    # Convert to floating point
    data = [spectrum.astype(float) for spectrum in spectra]

    # Optionally convert amplitudes to dB
    if log_amplitude:
        eps = np.finfo(float).eps
        data = [
            10 * np.log10(np.maximum(spectrum, eps))
            for spectrum in data
        ]

    # Shared color scale across all four spectra
    vmin = min(spectrum.min() for spectrum in data)
    vmax = max(spectrum.max() for spectrum in data)

    # Axis extents
    x_min, x_max = (
        (times[0], times[-1])
        if times is not None
        else (0, T - 1)
    )

    y_min, y_max = (
        (frequencies[0], frequencies[-1])
        if frequencies is not None
        else (0, k - 1)
    )

    # Create a 2x2 grid
    fig, axes = plt.subplots(
        2,
        2,
        figsize=(16, 10),
        sharex=True,
        sharey=True,
    )

    axes = axes.ravel()

    # Plot all four spectrograms
    for ax, spectrum, title in zip(axes, data, titles):
        im = ax.imshow(
            spectrum.T,
            aspect="auto",
            origin="lower",
            extent=[x_min, x_max, y_min, y_max],
            cmap=cmap,
            vmin=vmin,
            vmax=vmax,
            interpolation="nearest",
        )

        ax.set_title(title)
        ax.set_xlabel("Time")
        ax.set_ylabel("Frequency")

    # One shared colorbar
    

    fig.suptitle(
        "Audio Spectrum Comparison",
        fontsize=14,
    )

    plt.tight_layout()
    plt.show()


