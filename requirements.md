Your concern is justified, but the Gaussian assumption is probably not your biggest problem. Here is how I would rank the issues.

2. What is actually wrong with the measurement model

Magnitude vs. power. If you use |CQT|, overlapping partials from different notes interfere with each other (octaves, fifths, and so on share bins). The magnitudes add only if the phases align, and in general they don't, so z = Hx is not exactly linear in magnitude. In power (|CQT|²), the expected spectrum is additive when phases are effectively random: E|a+b|² = |a|² + |b|². That is the reason NMF-style transcription models work in power or magnitude-squared. Consider using power with x as note powers.

3. Other mismatches in the state model
   Nonnegativity and sparsity of x. The Gaussian posterior happily gives negative amplitudes and small nonzero values everywhere. This matters for overtone suppression, since a weak true note and an overtone are hard to separate without a sparsity or nonnegativity prior.
   F = I with Gaussian Q. Note onsets and offsets are jumps, which is heavy-tailed process noise. With small Q the filter lags at onsets, and with large Q it is noisy. Static F = I effectively makes this recursive least squares.
4. What to do

First, diagnose. Compute innovations ν = z − H x̂⁻ and check:

normalized innovation squared against a χ² distribution
whether the residual variance scales with the predicted level (plot |ν| against Hx̂)
whether innovations are white across time, and whether they are correlated across neighboring bins

Cheap fixes within the Kalman framework:

Make R signal-dependent: R_t = diag(σ₀² + α·(Hx̂⁻)²), or something similar
Use power instead of magnitude, or log-compress z (with care, since a log makes the measurement nonlinear)
Use an adaptive Q or a covariance reset when the innovation spikes (onset detection)
Clip or project x̂ ≥ 0 (a constrained KF), or use a soft threshold for sparsity

If that is not enough, switch to a model that matches the data statistics:

Online NMF / PLCA with fixed templates H: multiplicative updates with KL divergence (Poisson noise) or Itakura-Saito (complex Gaussian, which matches power spectra). It is nonnegative by construction, and you can add a sparsity penalty. This is the standard approach to this problem.
Gamma-Poisson or Gamma-exponential state-space models with particle filters or variational updates if you want a proper Bayesian filter with positivity.
EKF/UKF with x = exp(s), which gives positivity and multiplicative dynamics, but the measurement is nonlinear.
Switching models (note on/off per bin, with a Kalman filter conditioned on the active set) if you want to handle onsets and sparsity explicitly.
