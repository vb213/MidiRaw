import numpy as np
import matplotlib.pyplot as plt


def plot_spectrogram_comparison(
    spectrum1: np.ndarray,
    spectrum2: np.ndarray,
    frequencies: np.ndarray | tuple[np.ndarray, np.ndarray] | None = None,
    times: np.ndarray | tuple[np.ndarray, np.ndarray] | None = None,
    titles: tuple[str, str] = ("Spectrum 1", "Spectrum 2"),
    cmap: str = "viridis",
    log_amplitude: bool = False,
    shared_color_scale: bool = False,
):
    """
    Display two audio spectrograms side-by-side for visual comparison.

    Parameters
    ----------
    spectrum1, spectrum2 : np.ndarray
        2D arrays of shape (T, k), where rows are time points and
        columns are frequency bins. The two spectra may have different
        shapes, e.g. (T1, k1) and (T2, k2).

    frequencies : np.ndarray or tuple of two np.ndarray, optional
        Frequency values. Either a single array of shape (k,), used for
        both spectra (requires k1 == k2), or a tuple
        (frequencies1, frequencies2) with shapes (k1,) and (k2,).

    times : np.ndarray or tuple of two np.ndarray, optional
        Time values. Either a single array of shape (T,), used for
        both spectra (requires T1 == T2), or a tuple
        (times1, times2) with shapes (T1,) and (T2,).

    titles : tuple[str, str]
        Titles for the two spectrograms.

    cmap : str
        Matplotlib colormap.

    log_amplitude : bool
        If True, display amplitude in dB using 10*log10(amplitude).

    shared_color_scale : bool
        If True, both spectrograms use the same color limits.
    """

    spectrum1 = np.asarray(spectrum1)
    spectrum2 = np.asarray(spectrum2)

    if spectrum1.ndim != 2 or spectrum2.ndim != 2:
        raise ValueError(
            f"Both spectra must be 2D with shape (T, k), got "
            f"{spectrum1.shape} and {spectrum2.shape}."
        )

    T1, k1 = spectrum1.shape
    T2, k2 = spectrum2.shape

    def _resolve_axis(values, expected_lengths, name):
        """Return a pair of arrays (or None, None) validated against the spectra."""
        if values is None:
            return None, None

        if isinstance(values, tuple):
            if len(values) != 2:
                raise ValueError(f"{name} tuple must contain exactly two arrays.")
            pair = tuple(np.asarray(v) for v in values)
        else:
            single = np.asarray(values)
            if expected_lengths[0] != expected_lengths[1]:
                raise ValueError(
                    f"The spectra have different lengths along the {name} axis "
                    f"({expected_lengths[0]} and {expected_lengths[1]}), so "
                    f"{name} must be a tuple of two arrays."
                )
            pair = (single, single)

        for i, (arr, n) in enumerate(zip(pair, expected_lengths), start=1):
            if arr.shape != (n,):
                raise ValueError(
                    f"{name}[{i - 1}] must have shape ({n},), got {arr.shape}."
                )
        return pair

    frequencies1, frequencies2 = _resolve_axis(frequencies, (k1, k2), "frequencies")
    times1, times2 = _resolve_axis(times, (T1, T2), "times")

    # Convert to floating point
    data1 = spectrum1.astype(float)
    data2 = spectrum2.astype(float)

    # Optionally convert amplitudes to dB
    if log_amplitude:
        eps = np.finfo(float).eps
        data1 = 10 * np.log10(np.maximum(data1, eps))
        data2 = 10 * np.log10(np.maximum(data2, eps))

    # Shared color scale
    vmin1, vmax1 = data1.min(), data1.max()
    vmin2, vmax2 = data2.min(), data2.max()
    if shared_color_scale:
        vmin1 = min(vmin1, vmin2)
        vmax1 = max(vmax1, vmax2)
        vmin2 = vmin1
        vmax2 = vmax1

    # Axis extents (per spectrum)
    def _extent(t, f, T, k):
        x_min, x_max = (t[0], t[-1]) if t is not None else (0, T - 1)
        y_min, y_max = (f[0], f[-1]) if f is not None else (0, k - 1)
        return [x_min, x_max, y_min, y_max]

    extent1 = _extent(times1, frequencies1, T1, k1)
    extent2 = _extent(times2, frequencies2, T2, k2)

    # Only share the y-axis if both spectra cover the same frequency range
    share_y = extent1[2:] == extent2[2:]

    # Create side-by-side plots
    fig, axes = plt.subplots(
        1,
        2,
        figsize=(16, 6),
        sharey=share_y,
    )

    axes[0].imshow(
        data1.T,
        aspect="auto",
        origin="lower",
        extent=extent1,
        cmap=cmap,
        vmin=vmin1,
        vmax=vmax1,
        interpolation="nearest",
    )

    axes[1].imshow(
        data2.T,
        aspect="auto",
        origin="lower",
        extent=extent2,
        cmap=cmap,
        vmin=vmin2,
        vmax=vmax2,
        interpolation="nearest",
    )

    # Labels
    axes[0].set_title(titles[0])
    axes[1].set_title(titles[1])

    axes[0].set_xlabel("Time")
    axes[1].set_xlabel("Time")
    axes[0].set_ylabel("Frequency")
    if not share_y:
        axes[1].set_ylabel("Frequency")

    fig.suptitle("Audio Spectrum Comparison. Unten ist tief. Index 0 ist tief", fontsize=14)

    plt.tight_layout()
    plt.show()



def _per_spectrum_axis(values, name, expected_lengths):
    """
    Normalize a time/frequency argument to a list of 4 arrays (or None).

    Accepts:
      - None
      - a single 1D array, applied to all four spectra (lengths must match)
      - a sequence of four 1D arrays (or None entries), one per spectrum
    """
    n = len(expected_lengths)

    if values is None:
        return [None] * n

    is_per_spectrum = len(values) == n and all(
        v is None or np.ndim(v) == 1 for v in values
    )

    if is_per_spectrum:
        result = [None if v is None else np.asarray(v) for v in values]
    else:
        arr = np.asarray(values)
        result = [arr] * n

    for i, (arr, length) in enumerate(zip(result, expected_lengths), start=1):
        if arr is not None and arr.shape != (length,):
            raise ValueError(
                f"{name} for spectrum {i} must have shape ({length},), "
                f"got {arr.shape}."
            )
    return result


def plot_spectrogram_comparison4(
    spectrum1: np.ndarray,
    spectrum2: np.ndarray,
    spectrum3: np.ndarray,
    spectrum4: np.ndarray,
    frequencies=None,
    times=None,
    titles: tuple[str, str, str, str] = (
        "Spectrum 1",
        "Spectrum 2",
        "Spectrum 3",
        "Spectrum 4",
    ),
    cmap: str = "viridis",
    log_amplitude: bool = False,
    shared_color_scale: bool = True,
    share_axes: bool = False,
):
    """
    Display four audio spectrograms in a 2x2 grid. The spectrograms may
    have different shapes.

    Parameters
    ----------
    spectrum1, spectrum2, spectrum3, spectrum4 : np.ndarray
        Arrays of shape (T_i, k_i), where rows are time points and
        columns are frequency bins. Shapes may differ between spectra.

    frequencies : np.ndarray or sequence of 4 np.ndarray, optional
        Either a single array of shape (k,) used for all spectra (all
        spectra must then have k bins), or four arrays, one per spectrum,
        with shapes (k_i,).

    times : np.ndarray or sequence of 4 np.ndarray, optional
        Either a single array of shape (T,) used for all spectra, or four
        arrays, one per spectrum, with shapes (T_i,).

    titles : tuple[str, str, str, str]
        Titles for the four spectrograms.

    cmap : str
        Matplotlib colormap.

    log_amplitude : bool
        If True, display amplitude in dB using 10*log10(amplitude).

    shared_color_scale : bool
        If True, all spectra use the same vmin/vmax and one shared
        colorbar is drawn. Otherwise each has its own scale and colorbar.

    share_axes : bool
        If True, the x and y axes are shared between subplots. Only
        meaningful when times/frequencies are given in comparable units;
        with index-based axes of different sizes, leave this False.
    """
    spectra = [
        np.asarray(spectrum1),
        np.asarray(spectrum2),
        np.asarray(spectrum3),
        np.asarray(spectrum4),
    ]

    if len(titles) != 4:
        raise ValueError("titles must contain exactly four strings.")

    for i, spectrum in enumerate(spectra, start=1):
        if spectrum.ndim != 2:
            raise ValueError(f"Spectrum {i} must have shape (T, k).")

    n_times = [s.shape[0] for s in spectra]
    n_freqs = [s.shape[1] for s in spectra]

    times_list = _per_spectrum_axis(times, "times", n_times)
    freqs_list = _per_spectrum_axis(frequencies, "frequencies", n_freqs)

    # Convert to floating point
    data = [spectrum.astype(float) for spectrum in spectra]

    # Optionally convert amplitudes to dB
    if log_amplitude:
        eps = np.finfo(float).eps
        data = [10 * np.log10(np.maximum(s, eps)) for s in data]

    # Color limits: one (vmin, vmax) pair per spectrum
    if shared_color_scale:
        vmin = min(s.min() for s in data)
        vmax = max(s.max() for s in data)
        limits = [(vmin, vmax)] * 4
    else:
        limits = [(s.min(), s.max()) for s in data]

    # Create a 2x2 grid
    fig, axes = plt.subplots(
        2,
        2,
        figsize=(16, 10),
        sharex=share_axes,
        sharey=share_axes,
    )
    axes = axes.ravel()

    images = []
    for idx, ax in enumerate(axes):
        T_i, k_i = data[idx].shape
        t = times_list[idx]
        f = freqs_list[idx]

        x_min, x_max = (t[0], t[-1]) if t is not None else (0, T_i - 1)
        y_min, y_max = (f[0], f[-1]) if f is not None else (0, k_i - 1)

        im = ax.imshow(
            data[idx].T,
            aspect="auto",
            origin="lower",
            extent=[x_min, x_max, y_min, y_max],
            cmap=cmap,
            vmin=limits[idx][0],
            vmax=limits[idx][1],
            interpolation="nearest",
        )
        images.append(im)

        ax.set_title(titles[idx])
        ax.set_xlabel("Time")
        ax.set_ylabel("Frequency")

    label = "Amplitude (dB)" if log_amplitude else "Amplitude"

    if shared_color_scale:
        fig.colorbar(images[0], ax=axes.tolist(), label=label)
    else:
        for im, ax in zip(images, axes):
            fig.colorbar(im, ax=ax, label=label)

    fig.suptitle("Audio Spectrum Comparison", fontsize=14)

    # tight_layout conflicts with a shared colorbar spanning several axes,
    # so only use it when each subplot has its own colorbar.
    if not shared_color_scale:
        fig.tight_layout()

    plt.show()

def plot1dtimeseries(time_series, save_path):
    """Plot 1D time-series values over time and save the figure to save_path.

    No plot window is opened.

    Args:
        time_series: 1D array-like sequence of values.
        save_path: Path where the plot image will be saved.
    """
    fig, ax = plt.subplots()

    ax.plot(time_series)
    ax.set_xlabel("Time")
    ax.set_ylabel("Value")
    ax.set_title("Time Series")
    ax.grid(True, alpha=0.3)

    fig.tight_layout()
    fig.savefig(save_path, dpi=300, bbox_inches="tight")
    plt.close(fig)
    
if __name__ == "__main__":
    spectrum = np.loadtxt("python-tests/spectrograms/singlenotes/d4.txt")
    plot_spectrogram_comparison(spectrum, spectrum, titles=("Original Spectrum", "Reconstructed Spectrum"), cmap="viridis")


