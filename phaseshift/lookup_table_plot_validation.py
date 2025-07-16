# Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
#
# You may use, distribute and modify this code under the
# terms of the Apache 2.0 license.
# If you don't have a copy of this license, please visit:
#     https://github.com/gillesdegottex/phaseshift


# Uncomment the lines corresponding to `validation_file` in `lookup_table.h`

import numpy as np

import matplotlib.pyplot as plt
plt.ion()

X = np.loadtxt('lt_vf.txt')

refs = X[:,0]
estims = X[:,1]
relerrs = X[:,2]

ax1 = plt.subplot(311)
plt.plot(refs, 'ok', label='ref')
plt.plot(estims, 'xb', label='estim')
plt.grid()
plt.legend

plt.subplot(312, sharex=ax1)
plt.plot((refs-estims), 'k')
plt.grid()
plt.ylabel('Absolute error')

plt.subplot(313, sharex=ax1)
plt.plot(relerrs, 'k')
plt.grid()
plt.ylabel('Relative error')


from IPython.core.debugger import  Pdb; Pdb().set_trace()
