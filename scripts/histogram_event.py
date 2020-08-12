import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import math
import sys

#load the particle list
particle_list = pd.read_csv(sys.argv[1], sep=',')

mcid = particle_list['mcid']

#spacetime info 
tau = particle_list['tau']
x   = particle_list['x']
eta = particle_list['eta']
#momentum info 
E  = particle_list['E']
px = particle_list['px']
py = particle_list['py']
pz = particle_list['pz']

#species dependent info 
#pion 211
pi_pT  = []
pi_y   = []
pi_phi = [] 

#kaon 321
k_pT  = []
k_y   = []
k_phi = [] 

#proton 2212
p_pT  = []
p_y   = []
p_phi = [] 

#3d momentum space lists
for i in range(1, len(E) ):
    if ( mcid[i] == 211 ):
        pi_pT.append( math.sqrt( px[i]*px[i] + py[i]*py[i] ) )
        pi_y.append( 0.5 * math.log( (E[i] + pz[i]) / (E[i] - pz[i]) ) )
        pi_phi.append( math.atan( py[i] / px[i] ) )
    if ( mcid[i] == 321 ):
        k_pT.append( math.sqrt( px[i]*px[i] + py[i]*py[i] ) )
        k_y.append( 0.5 * math.log( (E[i] + pz[i]) / (E[i] - pz[i]) ) )
        k_phi.append( math.atan( py[i] / px[i] ) )
    if ( mcid[i] == 2212 ):
        p_pT.append( math.sqrt( px[i]*px[i] + py[i]*py[i] ) )
        p_y.append( 0.5 * math.log( (E[i] + pz[i]) / (E[i] - pz[i]) ) )
        p_phi.append( math.atan( py[i] / px[i] ) )

#midrapidity
pi_pT_mid = []
k_pT_mid = []
p_pT_mid = []

#the range of rapidity to integrate over 
ymax = 0.5
for i in range(1, len(pi_pT) ):
    if ( abs(pi_y[i]) < ymax ):
        pi_pT_mid.append( pi_pT[i] )

for i in range(1, len(k_pT) ):
    if ( abs(k_y[i]) < ymax ):
        k_pT_mid.append( k_pT[i] )

for i in range(1, len(p_pT) ):
    if ( abs(p_y[i]) < ymax ):
        p_pT_mid.append( p_pT[i] )

#histogram of particle yields
plt.hist(mcid, bins='auto')
plt.title("Particle Yields")
plt.xlabel("MC ID")
plt.show()

#pT bins
pT_bins = pd.read_csv('tables/pT_nodes.dat', header=None)
pT_bins = [0,.0072, .038, .094, .175, .28, .42, .58, .78, 1.01, 1.3, 1.6, 1.97, 2.4, 2.96, 3.7]

#histogram of tau (proper time of production)
plt.hist(tau, bins='auto')
plt.title("Proper time of particle production")
plt.xlabel("tau (fm/c)")
plt.show()

#pion spectra at midrapidity
plt.hist(pi_pT_mid, bins=pT_bins)
plt.title("Pion spectra midrapidity")
plt.xlabel("pT (GeV)")
plt.show()

#pion spectra at midrapidity
plt.hist(k_pT_mid, bins=pT_bins)
plt.title("Kaon spectra midrapidity")
plt.xlabel("pT (GeV)")
plt.show()

#pion spectra at midrapidity
plt.hist(p_pT_mid, bins=pT_bins)
plt.title("Proton spectra midrapidity")
plt.xlabel("pT (GeV)")
plt.show()

