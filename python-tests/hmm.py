import numpy as np
from dataclasses import dataclass

"""
                 previous state
                       │
                       ▼
              ┌─────────────────┐
              │ Note dynamics    │
              │ P(z_t | z_{t-1}) │
              └────────┬────────┘
                       │
                       ▼
spectrum ───────► amplitude estimation
   X_t                 │
                       ▼
                ┌─────────────┐
                │ Harmonic H  │
                │     ×       │
                │ amplitudes  │
                └──────┬──────┘
                       │
                       ▼
                predicted spectrum
                       │
                       ▼
                 likelihood
                       │
                       ▼
              P(z_t | X_1:t)
                       │
                       ▼
                 binary notes

"""

# ============================================================
# Configuration
# ============================================================

@dataclass
class Config:
    n_notes: int
    n_bins: int

    # Temporal dynamics
    p_on_on: float = 0.98
    p_off_off: float = 0.995

    # Amplitude dynamics
    amplitude_smoothing: float = 0.9

    # Numerical parameters
    noise_variance: float = 1e-3


# ============================================================
# Harmonic model
# ============================================================

class HarmonicDictionary:
    """
    H[:, i] describes the expected spectrum of note i.

    H has shape:
        (n_frequency_bins, n_notes)

    Column i contains the expected fundamental + harmonics
    of note i.
    """

    def __init__(self, spectrum_frequencies, note_frequencies):
        self.frequencies = spectrum_frequencies
        self.note_frequencies = note_frequencies

        self.H = self._build_dictionary()

    def _build_dictionary(self):
        """
        TODO:
            Construct harmonic templates.

        For note frequency f_i, this might contain energy around:

            f_i
            2*f_i
            3*f_i
            ...

        In a real implementation, the harmonic amplitudes should
        probably depend on the instrument/string/register.
        """
        raise NotImplementedError

    def reconstruct(self, amplitudes):
        """
        amplitudes: shape (n_notes,)

        returns:
            predicted spectrum, shape (n_bins,)
        """
        return self.H @ amplitudes


# ============================================================
# Latent state
# ============================================================

@dataclass
class LatentState:
    """
    State maintained by the real-time filter.

    z[i] = whether note i is currently active
    a[i] = current amplitude of note i
    """

    z: np.ndarray
    a: np.ndarray

    @classmethod
    def initial(cls, n_notes):
        return cls(
            z=np.zeros(n_notes, dtype=np.float64),
            a=np.zeros(n_notes, dtype=np.float64),
        )


# ============================================================
# Note-state dynamics
# ============================================================

class NoteDynamics:
    """
    Models:

        P(z_t | z_{t-1})

    independently for each note.

    This is the simplest version of a factorial HMM.
    """

    def __init__(self, config):
        self.p_on_on = config.p_on_on
        self.p_off_off = config.p_off_off

    def transition_probability(self, old_z, new_z):
        """
        Return P(new_z | old_z).

        In a more sophisticated model this could depend on:
            - note duration
            - neighboring notes
            - velocity
            - musical context
            - attack/release state
        """

        if old_z == 1:
            return (
                self.p_on_on
                if new_z == 1
                else 1.0 - self.p_on_on
            )

        return (
            1.0 - self.p_off_off
            if new_z == 1
            else self.p_off_off
        )

    def predict(self, z):
        """
        Predict the probability that each note is active
        before seeing the new spectrum.

        This is the causal temporal prior.
        """
        return np.where(
            z > 0.5,
            self.p_on_on,
            1.0 - self.p_off_off,
        )


# ============================================================
# Amplitude dynamics
# ============================================================

class AmplitudeDynamics:
    """
    Models how note amplitudes evolve over time.
    """

    def __init__(self, config):
        self.alpha = config.amplitude_smoothing

    def predict(self, previous_amplitudes):
        """
        Simple smooth amplitude prediction.

        TODO:
            A better model would explicitly model attack,
            sustain and release phases.
        """
        return self.alpha * previous_amplitudes

    def update(self, previous_amplitudes, estimated_amplitudes):
        """
        Smooth the newly estimated amplitudes.
        """
        return (
            self.alpha * previous_amplitudes
            + (1.0 - self.alpha) * estimated_amplitudes
        )


# ============================================================
# Observation model
# ============================================================

class ObservationModel:
    """
    Models:

        P(X_t | z_t, a_t)

    where

        X_t ~= H @ a_t
    """

    def __init__(self, harmonic_dictionary, noise_variance):
        self.H = harmonic_dictionary.H
        self.noise_variance = noise_variance

    def reconstruct(self, amplitudes):
        return self.H @ amplitudes

    def log_likelihood(self, spectrum, amplitudes):
        """
        Gaussian observation model for now:

            X ~ N(Ha, sigma^2 I)

        TODO:
            For real spectra, a log/Poisson/Gamma-like model may
            be more appropriate depending on how spectrum_series
            was generated.
        """
        prediction = self.reconstruct(amplitudes)

        error = spectrum - prediction

        return -0.5 * np.sum(
            error ** 2
        ) / self.noise_variance


# ============================================================
# Amplitude estimation
# ============================================================

class AmplitudeEstimator:
    """
    Given the current spectrum and the current belief about
    active notes, estimate their amplitudes.
    """

    def __init__(self, harmonic_dictionary):
        self.H = harmonic_dictionary.H

    def estimate(self, spectrum, active_probability):
        """
        Estimate:

            a_t ~= argmin ||X_t - H a_t||^2

        subject to:

            a >= 0

        and preferably:

            a_i ~= 0 when note i is inactive.

        """

        # ----------------------------------------------------
        # STUB
        # ----------------------------------------------------

        # Something like scipy.optimize.nnls could eventually
        # be used here.

        raise NotImplementedError


# ============================================================
# Causal Bayesian filter
# ============================================================

class NoteFilter:
    """
    Main real-time inference engine.

    Given one spectrum X_t, update the posterior state.
    """

    def __init__(
        self,
        config,
        harmonic_dictionary,
    ):
        self.config = config

        self.note_dynamics = NoteDynamics(config)

        self.amplitude_dynamics = AmplitudeDynamics(config)

        self.observation_model = ObservationModel(
            harmonic_dictionary,
            config.noise_variance,
        )

        self.amplitude_estimator = AmplitudeEstimator(
            harmonic_dictionary
        )

        self.state = LatentState.initial(
            config.n_notes
        )

    def process_frame(self, spectrum):
        """
        Process ONE spectrum frame.

        This function is causal:
            it only sees the current frame and previous state.
        """

        # ----------------------------------------------------
        # 1. Temporal prediction
        # ----------------------------------------------------

        predicted_note_probability = (
            self.note_dynamics.predict(
                self.state.z
            )
        )

        predicted_amplitudes = (
            self.amplitude_dynamics.predict(
                self.state.a
            )
        )

        # ----------------------------------------------------
        # 2. Explain current spectrum
        # ----------------------------------------------------

        estimated_amplitudes = (
            self.amplitude_estimator.estimate(
                spectrum,
                predicted_note_probability,
            )
        )

        # ----------------------------------------------------
        # 3. Update amplitudes
        # ----------------------------------------------------

        amplitudes = (
            self.amplitude_dynamics.update(
                predicted_amplitudes,
                estimated_amplitudes,
            )
        )

        # ----------------------------------------------------
        # 4. Compute how well each note explains the spectrum
        # ----------------------------------------------------

        note_probability = self._update_note_probabilities(
            spectrum,
            predicted_note_probability,
            amplitudes,
        )

        # ----------------------------------------------------
        # 5. Update state
        # ----------------------------------------------------

        self.state = LatentState(
            z=note_probability,
            a=amplitudes,
        )

        # ----------------------------------------------------
        # 6. Binary transcription
        # ----------------------------------------------------

        binary_notes = (
            note_probability > 0.5
        ).astype(np.int8)

        return binary_notes

    def _update_note_probabilities(
        self,
        spectrum,
        prior_probability,
        amplitudes,
    ):
        """
        Bayesian update:

            P(z_t | X_t, past)

        TODO.

        The important point is that the likelihood should be
        based on the WHOLE harmonic template of a note, not
        simply the amplitude of its fundamental frequency.

        In a more complete implementation this would compare
        hypotheses such as:

            note i OFF
            note i ON

        while accounting for the other simultaneously active
        notes.
        """

        raise NotImplementedError


# ============================================================
# Complete transcription pipeline
# ============================================================

def transcribe_realtime(
    spectrum_series,
    spectrum_frequencies,
    note_frequencies,
):
    """
    spectrum_series:
        shape (T, k)

        One spectrum per time step.

    spectrum_frequencies:
        frequencies corresponding to the k spectrum bins.

    note_frequencies:
        frequencies corresponding to the tonal notes
        we want to detect.
    """

    T, k = spectrum_series.shape
    n_notes = len(note_frequencies)

    config = Config(
        n_notes=n_notes,
        n_bins=k,
    )

    # --------------------------------------------------------
    # Build harmonic model
    # --------------------------------------------------------

    harmonic_dictionary = HarmonicDictionary(
        spectrum_frequencies,
        note_frequencies,
    )

    # --------------------------------------------------------
    # Create causal filter
    # --------------------------------------------------------

    filter_ = NoteFilter(
        config,
        harmonic_dictionary,
    )

    # --------------------------------------------------------
    # Process audio sequentially
    # --------------------------------------------------------

    transcription = np.zeros(
        (T, n_notes),
        dtype=np.int8,
    )

    for t in range(T):

        spectrum_t = spectrum_series[t]

        transcription[t] = (
            filter_.process_frame(
                spectrum_t
            )
        )

    return transcription


# ============================================================
# Example
# ============================================================

if __name__ == "__main__":

    # Already computed spectrum:
    #
    # spectrum_series.shape == (T, k)
    #
    # Example:
    #
    # spectrum_series = ...

    # Frequencies represented by the spectrum bins:
    #
    # spectrum_frequencies.shape == (k,)
    #
    # spectrum_frequencies = ...

    # Frequencies of the western tonal scale:
    #
    # note_frequencies.shape == (n_notes,)
    #
    # note_frequencies = ...

    transcription = transcribe_realtime(
        spectrum_series,
        spectrum_frequencies,
        note_frequencies,
    )

    # transcription[t, i] == 1
    # means note i is estimated to be active at time t.