import numpy as np

import matplotlib.pyplot as plt
plt.ion()

def a_weighting(f):
    return (12194**2) * (f**4) / ( (f**2 + 20.6**2) * np.sqrt( (f**2 + 107.7**2) * (f**2 + 737.9**2) ) * (f**2 + 12194.0**2) )

def b_weighting(f):
    return (12194**2) * (f**3) / ( (f**2 + 20.6**2) * np.sqrt( (f**2 + 158.5**2) ) * (f**2 + 12194.0**2) )

def c_weighting(f):
    return (12194**2) * (f**2) / ( (f**2 + 20.6**2) * (f**2 + 12194.0**2) )

def d_weighting(f):
    h_f = ((1037918.48 - f**2)**2 + 1080768.16*f**2) / ( (9837328 - f**2)**2 + 11723776*f**2 )
    return (f/6.8966888496476*10e-5) * np.sqrt(h_f / ((f**2 + 79919.29)*(f**2 + 1345600)))

def lin2db(x):
    return 20.0 * np.log10(x)

sr = 44100
freqs = np.arange(sr/2)
freq_idx_1000 = np.argmin(abs(freqs - 1000))
aw = a_weighting(freqs)
aw /= np.max(aw)
# aw /= aw[freq_idx_1000]
# aw = aw / np.sum(aw)
bw = b_weighting(freqs)
bw /= np.max(bw)
# bw /= bw[freq_idx_1000]
# bw = bw / np.sum(bw)
cw = c_weighting(freqs)
cw /= np.max(cw)
# cw /= cw[freq_idx_1000]
# cw = cw / np.sum(cw)
dw = d_weighting(freqs)
dw /= np.max(dw)
# dw /= dw[freq_idx_1000]
# dw = dw / np.sum(dw)
plt.plot(freqs, lin2db(aw), 'k', label="A-Weighting")
plt.plot(freqs, lin2db(bw), 'g', label="B-Weighting")
plt.plot(freqs, lin2db(cw), 'b', label="C-Weighting")
plt.plot(freqs, lin2db(dw), 'r', label="D-Weighting")
plt.grid()
plt.legend()
plt.xlabel("Frequency (Hz)")
plt.ylabel("Weighting (dB)")
# plt.xlim([0, sr/2])
# plt.ylim([-120, -70])
plt.gca().set_xscale('log')

from IPython.core.debugger import  Pdb; Pdb().set_trace() # fmt: skip
