import numpy as np
from scipy.stats import norm
import matplotlib.pyplot as plt


class OnlineHMM:
    # A, mu, stds, pi are learnable
    def __init__(self, A, means, stds, pi=None):
        self.A = np.asarray(A)              # (K, K), rows sum to 1
        self.mu = np.asarray(means)         # (K,)
        self.sd = np.asarray(stds)          # (K,)
        K = len(self.mu)
        self.alpha = np.full(K, 1 / K) if pi is None else np.asarray(pi)
        self.first = True

    def update(self, x):
        pred = self.alpha if self.first else self.alpha @ self.A
        self.first = False
        lik = norm.pdf(x, self.mu, self.sd)
        print(f"\tpred={pred}, lik={lik}")
        a = pred * lik
        print(f"\tafter pred*lik: {a}")
        s = a.sum()
        # guard against underflow on extreme outliers
        self.alpha = a / s if s > 0 else pred
        return int(self.alpha.argmax()), self.alpha



rng = np.random.default_rng(0)
A = np.array([[.95, .05], [.10, .90]])
mu, sd = np.array([0, 3]), np.array([1, 1])
T = 300

# simulate: state at t, then emission from that state
z = np.zeros(T, dtype=int)
x = np.zeros(T)
z[0] = rng.choice(2)
x[0] = rng.normal(mu[z[0]], sd[z[0]])
for t in range(1, T):
    z[t] = rng.choice(2, p=A[z[t-1]])
    x[t] = rng.normal(mu[z[t]], sd[z[t]])

# run the online filter from earlier
hmm = OnlineHMM(A=A, means=mu, stds=sd)
z_hat = np.zeros(T, dtype=int)
belief = np.zeros((T, 2))
for t in range(T):
    z_hat[t], belief[t] = hmm.update(x[t])













# plot
colors = np.array(["tab:blue", "tab:orange"])
t_axis = np.arange(T)

fig, axes = plt.subplots(3, 1, figsize=(10, 8), sharex=True)

# 1) observations colored by true state
axes[0].plot(t_axis, x, color="lightgray", lw=0.8, zorder=1)
axes[0].scatter(t_axis, x, c=colors[z], s=12, zorder=2)
axes[0].set_ylabel("x (colored by true z)")

# 2) observations colored by filtered estimate
axes[1].plot(t_axis, x, color="lightgray", lw=0.8, zorder=1)
axes[1].scatter(t_axis, x, c=colors[z_hat], s=12, zorder=2)
axes[1].set_ylabel("x (colored by estimated z)")

# 3) filter's belief in state 1, with true state as a step line
axes[2].plot(t_axis, belief[:, 1], color="tab:orange", label="P(z=1 | x_1..t)")
axes[2].step(t_axis, z, color="k", lw=0.8, alpha=0.6, where="mid", label="true z")
axes[2].set_ylabel("belief")
axes[2].set_xlabel("t")
axes[2].legend(loc="upper right")

# legend for the colors
from matplotlib.lines import Line2D
handles = [Line2D([], [], marker="o", ls="", color=c, label=f"state {k}")
           for k, c in enumerate(colors)]
axes[0].legend(handles=handles, loc="upper right")

plt.tight_layout()
plt.show()

print(f"filter accuracy: {(z_hat == z).mean():.2%}")